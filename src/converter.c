#include "converter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cherryaudio.h"
#include "opusenc.h"

#include "str_tools.h"

void* converter_convert_func(void* arg) {
    ConverterConvertArguments* args = (ConverterConvertArguments*)arg;

    char song_path[1024 * 3];
    char conversion_path[1024 * 3];
    char conversion_file[1024];
    char conversion_parent_path[1024];

    memset(song_path, 0, sizeof(song_path) / sizeof(song_path[0]));
    sprintf(song_path, "%s/%s", args->input_folder, args->input_file);

    memset(conversion_path, 0, sizeof(conversion_path) / sizeof(conversion_path[0]));

    str_tools_replace_extension(conversion_file, args->input_file, "opus");
    sprintf(conversion_path, "%s/%s", args->output_folder, conversion_file);

    memset(conversion_parent_path, 0, sizeof(conversion_parent_path) / sizeof(conversion_parent_path[0]));
    str_tools_remove_extension(conversion_parent_path, conversion_path);

    {
        const char command[] = "mkdir -p \"\"";
        size_t command_size = sizeof(command) / sizeof(command[0]) - 1;
        char mkdir_p[1024 + command_size + 1];
        sprintf(mkdir_p, "mkdir -p \"%s\"", conversion_parent_path);
        system(mkdir_p);
    }

    const char* extension = str_tools_get_extension(args->input_file);

    if (!str_tools_strs_eql_case_insensitive(extension, "flac") && !str_tools_strs_eql_case_insensitive(extension, "wav")) {
        {
            const char command[] = "cp   \"\"\"\"";
            size_t command_size = sizeof(command) / sizeof(command[0]) - 1;
            char command_final[1024 + command_size + 1];
            sprintf(command_final, "cp \"%s\" \"%s\"", song_path, conversion_path);
            system(command_final);
        }
        
        args->error = CONVERTER_ERR_OK;

        pthread_mutex_lock(&args->progress_percent_mutex);
        args->progress_percent = 1.0;
        pthread_mutex_unlock(&args->progress_percent_mutex);

        pthread_mutex_lock(&args->finished_mutex);
        args->finished = true;
        pthread_mutex_unlock(&args->finished_mutex);

        pthread_exit(NULL);
    }

    cherryaudio_file_format format = str_tools_strs_eql_case_insensitive(extension, "flac") ? CHERRYAUDIO_FILE_FORMAT_FLAC : CHERRYAUDIO_FILE_FORMAT_WAV_AIFF;

    cherryaudio_stream stream = cherryaudio_stream_from_path(song_path, format);
    cherryaudio_metadata stream_meta = stream.metadata;
    bool song_loaded = stream_meta.total_sample_count > 0;

    if (!song_loaded) {
        cherryaudio_stream_free(stream);
        args->error = CONVERTER_ERR_FAILED_TO_OPEN_STREAM;

        pthread_mutex_lock(&args->finished_mutex);
        args->finished = true;
        pthread_mutex_unlock(&args->finished_mutex);

        pthread_exit(NULL);
    }

    OggOpusComments* comments = ope_comments_create();

    int encoder_init_error;
    OggOpusEnc* encoder = ope_encoder_create_file(conversion_path, comments, stream_meta.sample_rate, stream_meta.channels, stream_meta.channels > 8 ? 255 : stream_meta.channels > 2, &encoder_init_error);
    if (encoder_init_error != OPE_OK) {
        ope_comments_destroy(comments);
        cherryaudio_stream_free(stream);
        args->error = CONVERTER_ERR_FAILED_TO_INIT_ENCODER;

        pthread_mutex_lock(&args->finished_mutex);
        args->finished = true;
        pthread_mutex_unlock(&args->finished_mutex);

        pthread_exit(NULL);
    }

    uint64_t decode_buffer_size = stream_meta.sample_rate * stream_meta.channels;
    float* decode_buffer = malloc(decode_buffer_size * sizeof(float));

    uint64_t samples_done = 0;
    while (samples_done < stream_meta.total_sample_count) {
        cherryaudio_pcm pcm = cherryaudio_stream_decode_pcm(stream, CHERRYAUDIO_PCM_FORMAT_F32, decode_buffer_size / stream_meta.channels, decode_buffer);
        if (pcm.frames_len == 0) {
            printf("\nreached eof early\n");
            break;
        }

        samples_done += pcm.frames_len / stream_meta.channels;

        int encode_res = ope_encoder_write_float(encoder, pcm.frames, pcm.frames_len / stream_meta.channels);
        if (encode_res != OPE_OK) {
            free(decode_buffer);
            ope_encoder_drain(encoder);
            ope_encoder_destroy(encoder);
            ope_comments_destroy(comments);
            cherryaudio_stream_free(stream);
            args->error = CONVERTER_ERR_FAILED_TO_ENCODE;

            pthread_mutex_lock(&args->finished_mutex);
            args->finished = true;
            pthread_mutex_unlock(&args->finished_mutex);

            pthread_exit(NULL);
        }

        pthread_mutex_lock(&args->progress_percent_mutex);
        args->progress_percent = (double)samples_done / (double)stream_meta.total_sample_count;
        pthread_mutex_unlock(&args->progress_percent_mutex);
    }
    
    free(decode_buffer);
    ope_encoder_drain(encoder);
    ope_encoder_destroy(encoder);
    ope_comments_destroy(comments);
    cherryaudio_stream_free(stream);

    args->error = CONVERTER_ERR_OK;

    pthread_mutex_lock(&args->finished_mutex);
    args->finished = true;
    pthread_mutex_unlock(&args->finished_mutex);

    pthread_exit(NULL);
}
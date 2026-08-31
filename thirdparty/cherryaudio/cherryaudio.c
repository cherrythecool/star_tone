#include "cherryaudio.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#ifdef __cplusplus
extern "C" {
#endif

cherryaudio_file cherryaudio_decode_from_path(
    const char* file_path, cherryaudio_file_format file_format,
    cherryaudio_pcm_format pcm_format) {
    cherryaudio_file file = (cherryaudio_file){0};

    switch (file_format) {
        case CHERRYAUDIO_FILE_FORMAT_WAV_AIFF:
            switch (pcm_format) {
                case CHERRYAUDIO_PCM_FORMAT_F32:
                    file.pcm_data.frames =
                        drwav_open_file_and_read_pcm_frames_f32(
                            file_path, &file.metadata.channels,
                            &file.metadata.sample_rate,
                            &file.metadata.total_sample_count, NULL);
                    break;

                case CHERRYAUDIO_PCM_FORMAT_I16:
                    file.pcm_data.frames =
                        drwav_open_file_and_read_pcm_frames_s16(
                            file_path, &file.metadata.channels,
                            &file.metadata.sample_rate,
                            &file.metadata.total_sample_count, NULL);
                    break;
            }

            file.pcm_data.frames_len =
                file.metadata.total_sample_count * file.metadata.channels;
            file.pcm_data.format = pcm_format;
            break;

        case CHERRYAUDIO_FILE_FORMAT_FLAC:
            switch (pcm_format) {
                case CHERRYAUDIO_PCM_FORMAT_F32:
                    file.pcm_data.frames =
                        drflac_open_file_and_read_pcm_frames_f32(
                            file_path, &file.metadata.channels,
                            &file.metadata.sample_rate,
                            &file.metadata.total_sample_count, NULL);
                    break;

                case CHERRYAUDIO_PCM_FORMAT_I16:
                    file.pcm_data.frames =
                        drflac_open_file_and_read_pcm_frames_s16(
                            file_path, &file.metadata.channels,
                            &file.metadata.sample_rate,
                            &file.metadata.total_sample_count, NULL);
                    break;
            }

            file.pcm_data.frames_len =
                file.metadata.total_sample_count * file.metadata.channels;
            file.pcm_data.format = pcm_format;
            break;
    }

    return file;
}

cherryaudio_stream cherryaudio_stream_from_path(const char* path, cherryaudio_file_format file_format) {
    cherryaudio_stream stream = (cherryaudio_stream){0};

    switch (file_format) {
        case CHERRYAUDIO_FILE_FORMAT_WAV_AIFF: {
            stream.file_format = file_format;
            stream.format_handle = malloc(sizeof(drwav));
            drwav_init_file(stream.format_handle, path, NULL);
            stream.metadata.channels = ((drwav*)stream.format_handle)->channels;
            stream.metadata.sample_rate = ((drwav*)stream.format_handle)->sampleRate;
            stream.metadata.total_sample_count = ((drwav*)stream.format_handle)->totalPCMFrameCount;
            break;
        }

        case CHERRYAUDIO_FILE_FORMAT_FLAC: {
            stream.file_format = file_format;
            stream.format_handle = drflac_open_file(path, NULL);
            stream.metadata.channels = ((drflac*)stream.format_handle)->channels;
            stream.metadata.sample_rate = ((drflac*)stream.format_handle)->sampleRate;
            stream.metadata.total_sample_count = ((drflac*)stream.format_handle)->totalPCMFrameCount;
            break;
        }
    }

    return stream;
}

cherryaudio_pcm cherryaudio_stream_decode_pcm(cherryaudio_stream stream, cherryaudio_pcm_format requested_pcm_format, uint64_t sample_count, void* buf) {
    cherryaudio_pcm pcm = (cherryaudio_pcm){0};

    switch (stream.file_format) {
        case CHERRYAUDIO_FILE_FORMAT_WAV_AIFF:
            pcm.format = requested_pcm_format;

            switch (requested_pcm_format) {
                case CHERRYAUDIO_PCM_FORMAT_F32: {
                    pcm.frames = buf;//malloc(sizeof(float) * sample_count * stream.metadata.channels);
                    drwav_uint64 frames_read = drwav_read_pcm_frames_f32(stream.format_handle, sample_count, pcm.frames);
                    pcm.frames_len = frames_read * stream.metadata.channels;
                    break;
                }

                case CHERRYAUDIO_PCM_FORMAT_I16: {
                    pcm.frames = buf;//malloc(sizeof(short) * sample_count * stream.metadata.channels);
                    drwav_uint64 frames_read = drwav_read_pcm_frames_s16(stream.format_handle, sample_count, pcm.frames);
                    pcm.frames_len = frames_read * stream.metadata.channels;
                    break;
                }
            }

            break;

        case CHERRYAUDIO_FILE_FORMAT_FLAC:
            pcm.format = requested_pcm_format;

            switch (requested_pcm_format) {
                case CHERRYAUDIO_PCM_FORMAT_F32: {
                    pcm.frames = buf;//malloc(sizeof(float) * sample_count * stream.metadata.channels);
                    drflac_uint64 frames_read = drflac_read_pcm_frames_f32(stream.format_handle, sample_count, pcm.frames);
                    pcm.frames_len = frames_read * stream.metadata.channels;
                    break;
                }

                case CHERRYAUDIO_PCM_FORMAT_I16: {
                    pcm.frames = buf;//malloc(sizeof(short) * sample_count * stream.metadata.channels);
                    drflac_uint64 frames_read = drflac_read_pcm_frames_s16(stream.format_handle, sample_count, pcm.frames);
                    pcm.frames_len = frames_read * stream.metadata.channels;
                    break;
                }
            }

            break;
    }

    return pcm;
}

void cherryaudio_stream_free(cherryaudio_stream stream) {
    switch (stream.file_format) {
        case CHERRYAUDIO_FILE_FORMAT_WAV_AIFF:
            drwav_uninit(stream.format_handle);
            free(stream.format_handle);
            break;
        case CHERRYAUDIO_FILE_FORMAT_FLAC:
            drflac_close(stream.format_handle);
            break;
    }
}

#ifdef __cplusplus
}
#endif  // extern "C" {

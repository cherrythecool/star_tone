#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// posix shit we prolly wanna replace eventually lol (with a cross platform api)
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>

#include "cherryaudio.h"
#include "opusenc.h"

#define ANSI_ESC(v) "\033[" v "m"
#define ANSI_RESET ANSI_ESC("")

typedef enum {
    ERR_OK = 0,
    ERR_FAILED_TO_OPEN_STREAM,
    ERR_FAILED_TO_INIT_ENCODER,
    ERR_FAILED_TO_ENCODE,
} file_load_err;

typedef struct {
    pthread_t thread;
    const char* input_folder;
    const char* input_file;
    const char* output_folder;
    file_load_err load_error;

    bool finished;
    pthread_mutex_t finished_mutex;

    double progress_percent;
    pthread_mutex_t progress_percent_mutex;
} file_load_t;

const char* file_path_get_ext(const char* path) {
    bool found_period = false;
    size_t last_period = 0;

    for (size_t index = 0; path[index] != '\0'; ++index) {
        if (path[index] == '.') {
            found_period = true;
            last_period = index;
        }
    }

    if (!found_period) {
        return NULL;
    }

    return path + last_period + 1;
}

bool strs_eql_nocase(const char* s1, const char* s2) {
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2);

    if (l1 != l2) {
        return false;
    }

    for (size_t i = 0; i < l1; i++) {
        if (tolower(s1[i]) != tolower(s2[i])) {
            return false;
        }
    }

    return true;
}

void replace_extension_with(char* dst, const char* path, const char* newext) {
    bool found_period = false;
    size_t last_period = 0;

    for (size_t index = 0; path[index] != '\0'; ++index) {
        if (path[index] == '.') {
            found_period = true;
            last_period = index;
        }
    }

    size_t size;
    if (!found_period) {
        size = strlen(path) + 1 + strlen(newext);
    } else {
        size = last_period + 1 + strlen(newext);
    }

    memset(dst, 0, size + 1);

    if (!found_period) {
        sprintf(dst, "%s.%s", path, newext);
    } else {
        memcpy(dst, path, last_period);
        dst[last_period] = '.';
        memcpy(dst + last_period + 1, newext, strlen(newext));
    }
}

double time_get_seconds(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + ((double)time.tv_nsec / 1000.0 / 1000.0 / 1000.0);
}

// void busy_sleep(double seconds) {
//     double start = time_get_seconds();
//     while (time_get_seconds() - start < seconds) {
//         continue;
//     }
// }

void* file_load(void* arg) {
    file_load_t* args = (file_load_t*)arg;

    char song_path[1024 * 3];
    char conversion_path[1024 * 3];
    char conversion_file[1024];

    memset(song_path, 0, sizeof(song_path) / sizeof(song_path[0]));
    sprintf(song_path, "%s/%s", args->input_folder, args->input_file);

    memset(conversion_path, 0, sizeof(conversion_path) / sizeof(conversion_path[0]));

    replace_extension_with(conversion_file, args->input_file, "opus");
    sprintf(conversion_path, "%s/%s", args->output_folder, conversion_file);

    const char* extension = file_path_get_ext(args->input_file);
    cherryaudio_file_format format = strs_eql_nocase(extension, "flac") ? CHERRYAUDIO_FILE_FORMAT_FLAC : CHERRYAUDIO_FILE_FORMAT_WAV_AIFF;

    cherryaudio_stream stream = cherryaudio_stream_from_path(song_path, format);
    cherryaudio_metadata stream_meta = stream.metadata;
    bool song_loaded = stream_meta.total_sample_count > 0;

    if (!song_loaded) {
        cherryaudio_stream_free(stream);
        args->load_error = ERR_FAILED_TO_OPEN_STREAM;

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
        args->load_error = ERR_FAILED_TO_INIT_ENCODER;

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
            args->load_error = ERR_FAILED_TO_ENCODE;

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

    args->load_error = ERR_OK;

    pthread_mutex_lock(&args->finished_mutex);
    args->finished = true;
    pthread_mutex_unlock(&args->finished_mutex);

    pthread_exit(NULL);
}

typedef struct {
    size_t len;
    bool found;
} remove_last_newline_t;

remove_last_newline_t str_remove_last_newline(char* str, size_t len) {
    bool found_newline = false;
    size_t newline_index = 0;

    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            found_newline = true;
            newline_index = i;
        }
    }

    if (found_newline) {
        str[newline_index] = '\0';
    }

    return (remove_last_newline_t) {newline_index, found_newline};
}

char* get_line(size_t* len) {
    char* line = NULL;
    size_t line_len = 0;
    getline(&line, &line_len, stdin);

    if (line == NULL) {
        if (len) {
            *len = 0;
        }

        return line;
    }

    remove_last_newline_t newline_ret = str_remove_last_newline(line, line_len);

    if (len) {
        *len = newline_ret.found ? newline_ret.len : line_len;
    }

    return line;
}

int main(void) {
    printf(ANSI_ESC("1") ANSI_ESC("33") "[star_tone] " ANSI_RESET "Starting\n");

    printf("Input directory: ");
    char* songs_folder = get_line(NULL);

    printf("Output directory: ");
    char* conversion_folder = get_line(NULL);
    
    printf("Opening songs directory '%s' and converting into '%s'\n", songs_folder, conversion_folder);
    
    DIR *dp;
    dp = opendir(songs_folder);
    if (dp == NULL) {
        fprintf(stderr, "Couldn't open the songs directory (%s)\n", songs_folder);
        return 1;
    }
    
    char** songs = malloc(sizeof(char*));
    size_t songs_size = 1;
    size_t songs_count = 0;
    
    struct dirent* ep;
    while ((ep = readdir(dp)) != NULL) {
        const char* extension = file_path_get_ext(ep->d_name);
        
        if (strs_eql_nocase(extension, "flac") || strs_eql_nocase(extension, "wav")) {
            if (songs_count > songs_size - 1) {
                songs_size *= 2;
                songs = realloc(songs, sizeof(char*) * songs_size);
            }
            
            size_t path_size = sizeof(char) * ep->d_namlen;
            char* perma_path = malloc(path_size + 1);
            memset(perma_path, 0, path_size + 1);
            memcpy(perma_path, ep->d_name, path_size);
            
            songs[songs_count] = perma_path;
            songs_count += 1;
        }
    }
    
    closedir(dp);
    
    printf("Found %zu files to convert\n", songs_count);
    
    file_load_t* threads = calloc(songs_count, sizeof(file_load_t));
    size_t threads_count = songs_count;
    
    for (size_t i = 0; i < songs_count; i++) {
        threads[i] = (file_load_t) {
            (pthread_t) {0},
            songs_folder,
            songs[i],
            conversion_folder,
            ERR_OK,
            false,
            (pthread_mutex_t) {0},
            0.0,
            (pthread_mutex_t) {0},
        };
        
        pthread_mutex_init(&threads[i].finished_mutex, NULL);
        pthread_mutex_init(&threads[i].progress_percent_mutex, NULL);
        
        int res = pthread_create(&threads[i].thread, NULL, file_load, &threads[i]);
        if (res) {
            fprintf(stderr, "pthread_create failed: %d\n", res);
            return EXIT_FAILURE;
        }
    }
    
    while (true) {
        printf("\r");
        
        bool needs_wait = false;
        for (size_t i = 0; i < threads_count; i++) {
            if (needs_wait) {
                break;
            }
            
            pthread_mutex_lock(&threads[i].finished_mutex);
            
            if (!threads[i].finished) {
                needs_wait = true;
            }
            
            pthread_mutex_unlock(&threads[i].finished_mutex);
        }
        
        for (size_t i = 0; i < threads_count; i++) {
            if (i > 0) {
                printf(", ");
            }
            
            file_load_t* data = &threads[i];
            pthread_mutex_lock(&data->progress_percent_mutex);
            const char* color_str = data->load_error != ERR_OK ? ANSI_ESC("31") : data->progress_percent >= 1.0 ? ANSI_ESC("32") : ANSI_ESC("0");
            
            printf("%s" ANSI_ESC("1") "%s: " ANSI_ESC("0") "%s%.2f%%" ANSI_RESET, color_str, data->input_file, color_str, data->progress_percent * 100.0);
            pthread_mutex_unlock(&data->progress_percent_mutex);
        }
        
        fflush(stdout);
        
        if (!needs_wait) {
            break;
        }
        
        usleep(1000 * 100);
    }
    
    printf("\n");
    
    for (size_t i = 0; i < threads_count; i++) {
        file_load_t data = threads[i];
        if (data.load_error != ERR_OK) {
            printf(ANSI_ESC("1") ANSI_ESC("31") "#%zu" ANSI_RESET ANSI_ESC("31") " %s failed to convert, error: %d\n" ANSI_RESET, i + 1, data.input_file, data.load_error);
        } else {
            printf(ANSI_ESC("1") ANSI_ESC("32") "#%zu" ANSI_RESET ANSI_ESC("32") " %s successfully converted\n" ANSI_RESET, i + 1, data.input_file);
        }
        pthread_join(data.thread, NULL);
    }
    
    free(threads);
    
    for (size_t i = 0; i < songs_count; i++) {
        free(songs[i]);
    }
    
    free(songs);

    free(conversion_folder);
    free(songs_folder);
    
    printf(ANSI_ESC("1") ANSI_ESC("32") "Conversion complete\n" ANSI_RESET);
    
    return EXIT_SUCCESS;
}
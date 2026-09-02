#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <time.h>

// posix shit we prolly wanna replace eventually lol (with a cross platform api)
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>

#include "converter.h"
#include "str_tools.h"
#include "io_tools.h"

#define ANSI_ESC(v) "\033[" v "m"
#define ANSI_RESET ANSI_ESC("")

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

    size_t new_len = str_tools_remove_last_newline(line, line_len);

    if (len) {
        *len = new_len;
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

    IOToolsDirectoryList dir_list = io_tools_list_dir_recurse(songs_folder, NULL);

    char** songs = dir_list.paths;
    size_t songs_count = dir_list.paths_count;
    
    printf("Found %zu files to copy / convert\n", dir_list.paths_count);

    size_t threads_count = songs_count > 16 ? 16 : songs_count;
    size_t songs_index = 0;
    ConverterConvertArguments* threads = calloc(threads_count, sizeof(ConverterConvertArguments));
    
    for (size_t i = 0; i < threads_count; i++) {
        threads[i] = (ConverterConvertArguments) {
            songs_folder,
            songs[i],
            conversion_folder,
            CONVERTER_ERR_OK,
            false,
            0.0,
            (pthread_t) {0},
            (pthread_mutex_t) {0},
            (pthread_mutex_t) {0},
        };
        
        pthread_mutex_init(&threads[i].finished_mutex, NULL);
        pthread_mutex_init(&threads[i].progress_percent_mutex, NULL);
        songs_index++;

        int res = pthread_create(&threads[i].thread, NULL, converter_convert_func, &threads[i]);
        if (res) {
            fprintf(stderr, "pthread_create failed: %d\n", res);
            return EXIT_FAILURE;
        }
    }

    while (songs_index < songs_count) {
        for (size_t i = 0; i < threads_count; i++) {
            bool kill_thread = false;
            pthread_mutex_lock(&threads[i].finished_mutex);
            kill_thread = threads[i].finished;
            pthread_mutex_unlock(&threads[i].finished_mutex);

            if (kill_thread) {
                pthread_join(threads[i].thread, NULL);
                pthread_mutex_destroy(&threads[i].finished_mutex);
                pthread_mutex_destroy(&threads[i].progress_percent_mutex);

                if (threads[i].error != CONVERTER_ERR_OK) {
                    fprintf(stderr, ANSI_ESC("31") "file %s/%s had error %d\n" ANSI_RESET, threads[i].output_folder, threads[i].input_file, threads[i].error);
                }

                threads[i] = (ConverterConvertArguments) {
                    songs_folder,
                    songs[songs_index],
                    conversion_folder,
                    CONVERTER_ERR_OK,
                    false,
                    0.0,
                    (pthread_t) {0},
                    (pthread_mutex_t) {0},
                    (pthread_mutex_t) {0},
                };
                
                pthread_mutex_init(&threads[i].finished_mutex, NULL);
                pthread_mutex_init(&threads[i].progress_percent_mutex, NULL);
                songs_index++;

                int res = pthread_create(&threads[i].thread, NULL, converter_convert_func, &threads[i]);
                if (res) {
                    fprintf(stderr, "pthread_create failed: %d\n", res);
                    return EXIT_FAILURE;
                }
            }
        }

        printf("\r%zu queued / %zu total", songs_index, songs_count);
        fflush(stdout);

        usleep(1000);
    }
    
    printf("\n");

    printf("Waiting on the last %zu files...\n", threads_count);

    for (size_t i = 0; i < threads_count; i++) {
        ConverterConvertArguments data = threads[i];
        pthread_join(data.thread, NULL);
        pthread_mutex_destroy(&data.finished_mutex);
        pthread_mutex_destroy(&data.progress_percent_mutex);

        printf("#%zu completed\n", i);
    }
    
    free(threads);

    for (size_t i = 0; i < dir_list.paths_count; i++) {
        free(dir_list.paths[i]);
    }
    
    free(dir_list.paths);

    free(conversion_folder);
    free(songs_folder);
    
    printf(ANSI_ESC("1") ANSI_ESC("32") "Conversion complete\n" ANSI_RESET);
    
    return EXIT_SUCCESS;
}
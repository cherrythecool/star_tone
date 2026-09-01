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
    
    for (size_t i = 0; i < songs_count; i++) {
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
    
    while (true) {
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
        
        if (!needs_wait) {
            break;
        }
        
        usleep(1000 * 100);
    }
    
    printf("\n");
    
    for (size_t i = 0; i < threads_count; i++) {
        ConverterConvertArguments data = threads[i];
        if (data.error != CONVERTER_ERR_OK) {
            printf(ANSI_ESC("1") ANSI_ESC("31") "#%zu" ANSI_RESET ANSI_ESC("31") " %s failed to convert, error: %d\n" ANSI_RESET, i + 1, data.input_file, data.error);
        } else {
            printf(ANSI_ESC("1") ANSI_ESC("32") "#%zu" ANSI_RESET ANSI_ESC("32") " %s successfully converted\n" ANSI_RESET, i + 1, data.input_file);
        }
        pthread_join(data.thread, NULL);
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
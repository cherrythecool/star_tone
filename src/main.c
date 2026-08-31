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

#include "converter.h"
#include "str_tools.h"

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
        const char* extension = str_tools_get_extension(ep->d_name);
        
        if (str_tools_strs_eql_case_insensitive(extension, "flac") || str_tools_strs_eql_case_insensitive(extension, "wav")) {
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
    
    ConverterConvertArguments* threads = calloc(songs_count, sizeof(ConverterConvertArguments));
    size_t threads_count = songs_count;
    
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

        int res = pthread_create(&threads[i].thread, NULL, converter_convert_func, &threads[i]);
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
            
            ConverterConvertArguments* data = &threads[i];
            pthread_mutex_lock(&data->progress_percent_mutex);
            const char* color_str = data->error != CONVERTER_ERR_OK ? ANSI_ESC("31") : data->progress_percent >= 1.0 ? ANSI_ESC("32") : ANSI_ESC("0");
            
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
        ConverterConvertArguments data = threads[i];
        if (data.error != CONVERTER_ERR_OK) {
            printf(ANSI_ESC("1") ANSI_ESC("31") "#%zu" ANSI_RESET ANSI_ESC("31") " %s failed to convert, error: %d\n" ANSI_RESET, i + 1, data.input_file, data.error);
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
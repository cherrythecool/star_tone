#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "cherryaudio.h"
#include "opusenc.h"

char* replace_extension_with(const char* path, const char* newext) {
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

    char* fart = calloc(1, size + 1);
    if (!found_period) {
        sprintf(fart, "%s.%s", path, newext);
    } else {
        memcpy(fart, path, last_period);
        fart[last_period] = '.';
        memcpy(fart + last_period + 1, newext, strlen(newext));
    }

    return fart;
}

int main(void) {
    printf("starting star_tone\n");

    const char* songs_folder = "./songs";
    const char* conversion_folder = "./songs-converted";
    printf("opening songs dir at '%s' & converting to '%s'\n", songs_folder, conversion_folder);

    DIR *dp;
    dp = opendir(songs_folder);
    if (dp == NULL) {
        fprintf(stderr, "couldn't open the songs directory\n");
        return 1;
    }

    char** songs = malloc(sizeof(char*));
    size_t songs_size = 1;
    size_t songs_count = 0;

    struct dirent* ep;
    while ((ep = readdir(dp)) != NULL) {
        const char* extension = cherryaudio_str_pathext(ep->d_name);

        // TODO: lowercase & uppercase extension support
        if (strcmp(extension, "flac") == 0 || strcmp(extension, "wav") == 0) {
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

    printf("found %zu files to convert from path\n", songs_count);

    char song_path[1024 * 3];
    char conversion_path[1024 * 3];

    for (size_t i = 0; i < songs_count; i++) {
        memset(song_path, 0, sizeof(song_path) / sizeof(song_path[0]));
        sprintf(song_path, "%s/%s", songs_folder, songs[i]);

        memset(conversion_path, 0, sizeof(conversion_path) / sizeof(conversion_path[0]));

        char* converted_song_path = replace_extension_with(songs[i], "opus");
        sprintf(conversion_path, "%s/%s", conversion_folder, converted_song_path);
        free(converted_song_path);

        printf("converting '%s' to opus '%s'\n", song_path, conversion_path);
        printf("decoding original file\n");

        cherryaudio_file song = cherryaudio_decode_from_path(song_path, CHERRYAUDIO_FILE_FORMAT_DETECT, CHERRYAUDIO_PCM_FORMAT_F32);
        bool song_loaded = song.pcm_data.frames_len > 0;
    
        if (song_loaded) {
            printf("song loaded with %llu audio frames\n", song.pcm_data.frames_len);
        } else {
            fprintf(stderr, "failed to load song\n");
            return 1;
        }

        printf("converting to opus now\n");
    
        OggOpusComments* comments = ope_comments_create();
    
        int encoder_init_error;
        OggOpusEnc* encoder = ope_encoder_create_file(conversion_path, comments, song.metadata.sample_rate, song.metadata.channels, song.metadata.channels > 8 ? 255 : song.metadata.channels > 2, &encoder_init_error);
        if (encoder_init_error != OPE_OK) {
            fprintf(stderr, "failed to initialize opus encoder with error %d\n", encoder_init_error);
            return 1;
        }
    
        int encode_res = ope_encoder_write_float(encoder, song.pcm_data.frames, song.metadata.total_sample_count);
        if (encode_res != OPE_OK) {
            fprintf(stderr, "failed to encode opus with error %d\n", encode_res);
            return 1;
        }

        printf("freeing old pcm frames\n");
        free(song.pcm_data.frames);

        printf("draining and destroying encoder now\n");
    
        ope_encoder_drain(encoder);
        
        ope_encoder_destroy(encoder);
        ope_comments_destroy(comments);

        printf("finished conversion\n");
    }

    printf("freeing bs\n");

    for (size_t i = 0; i < songs_count; i++) {
        free(songs[i]);
    }

    free(songs);

    return 0;
}
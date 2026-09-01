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

static void cherryaudio_drflac_metadata(void* user_data, drflac_metadata* flac_metadata) {
    cherryaudio_stream* stream = (cherryaudio_stream*)user_data;

    switch (flac_metadata->type) {
        case DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT: {
            stream->comments_count++;

            if (!stream->comments) {
                stream->comments_size = 1;
                stream->comments = calloc(1, sizeof(cherryaudio_stream_comment));
            } else if (stream->comments_count > stream->comments_size) {
                stream->comments_size *= 2;
                stream->comments = realloc(stream->comments, sizeof(cherryaudio_stream_comment) * stream->comments_size);
            }

            cherryaudio_stream_comment cherry_comment = (cherryaudio_stream_comment) {0};
            cherry_comment.vendor_length = flac_metadata->data.vorbis_comment.vendorLength;
            cherry_comment.vendor = calloc(cherry_comment.vendor_length + 1, sizeof(char));
            memcpy(cherry_comment.vendor, flac_metadata->data.vorbis_comment.vendor, cherry_comment.vendor_length);

            drflac_vorbis_comment_iterator iterator;
            drflac_init_vorbis_comment_iterator(&iterator, flac_metadata->data.vorbis_comment.commentCount, flac_metadata->data.vorbis_comment.pComments);

            size_t cherry_comments_size = 0;

            const char* comment;
            uint32_t comment_length;
            while ((comment = drflac_next_vorbis_comment(&iterator, &comment_length)) != NULL) {
                cherry_comment.comments_count++;

                if (!cherry_comment.comments) {
                    cherry_comments_size = 1;
                    cherry_comment.comments = calloc(1, sizeof(char*));
                } else if (cherry_comment.comments_count > cherry_comments_size) {
                    cherry_comments_size *= 2;
                    cherry_comment.comments = realloc(cherry_comment.comments, sizeof(char*) * cherry_comments_size);
                }

                char* comment_copy = calloc(comment_length + 1, sizeof(char));
                memcpy(comment_copy, comment, comment_length);
                cherry_comment.comments[cherry_comment.comments_count - 1] = comment_copy;
            }

            stream->comments[stream->comments_count - 1] = cherry_comment;
            break;
        }

        case DRFLAC_METADATA_BLOCK_TYPE_PICTURE: {
            stream->pictures_count++;

            if (!stream->pictures) {
                stream->pictures_size = 1;
                stream->pictures = calloc(1, sizeof(cherryaudio_stream_picture));
            } else if (stream->pictures_count > stream->pictures_size) {
                stream->pictures_size *= 2;
                stream->pictures = realloc(stream->pictures, sizeof(cherryaudio_stream_picture) * stream->pictures_size);
            }

            cherryaudio_stream_picture cherry_picture = (cherryaudio_stream_picture) {0};
            cherry_picture.picture_data_size = flac_metadata->data.picture.pictureDataSize;
            cherry_picture.picture_data = malloc(cherry_picture.picture_data_size);
            memcpy(cherry_picture.picture_data, flac_metadata->data.picture.pPictureData, cherry_picture.picture_data_size);

            cherry_picture.description_length = flac_metadata->data.picture.descriptionLength;

            if (cherry_picture.description_length > 0) {
                cherry_picture.description = calloc(cherry_picture.description_length + 1, sizeof(char));
                memcpy(cherry_picture.description, flac_metadata->data.picture.description, cherry_picture.description_length);
            }

            stream->pictures[stream->pictures_count - 1] = cherry_picture;
            break;
        }

        default: {
            break;
        }
    }
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
            stream.format_handle = drflac_open_file_with_metadata(path, cherryaudio_drflac_metadata, &stream, NULL);
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

    if (stream.comments) {
        for (size_t i = 0; i < stream.comments_count; i++) {
            if (stream.comments[i].vendor) {
                free(stream.comments[i].vendor);
            }

            if (stream.comments[i].comments) {
                for (size_t j = 0; j < stream.comments[i].comments_count; j++) {
                    free(stream.comments[i].comments[j]);
                }
    
                free(stream.comments[i].comments);
            }
        }

        free(stream.comments);
    }

    if (stream.pictures) {
        for (size_t i = 0; i < stream.pictures_count; i++) {
            if (stream.pictures[i].description) {
                free(stream.pictures[i].description);
            }

            if (stream.pictures[i].picture_data) {
                free(stream.pictures[i].picture_data);
            }
        }

        free(stream.pictures);
    }
}

#ifdef __cplusplus
}
#endif  // extern "C" {

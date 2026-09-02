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

#include "opusenc.h"

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
    OggOpusComments* comments = (OggOpusComments*)user_data;

    switch (flac_metadata->type) {
        case DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT: {
            if (flac_metadata->data.vorbis_comment.commentCount == 0) {
                break;
            }

            drflac_vorbis_comment_iterator iterator;
            drflac_init_vorbis_comment_iterator(&iterator, flac_metadata->data.vorbis_comment.commentCount, flac_metadata->data.vorbis_comment.pComments);

            const char* comment;
            uint32_t comment_length;
            while ((comment = drflac_next_vorbis_comment(&iterator, &comment_length)) != NULL) {
                char temp_comment[comment_length + 1];
                memset(temp_comment, 0, comment_length + 1);
                memcpy(temp_comment, comment, comment_length);
                
                int add_err = ope_comments_add_string(comments, temp_comment);
                if (add_err != OPE_OK) {
                    fprintf(stderr, "ope_comments_add_string failed with err %d\n", add_err);
                }
            }
            
            break;
        }

        case DRFLAC_METADATA_BLOCK_TYPE_PICTURE: {
            if (flac_metadata->data.picture.pictureDataSize == 0 || !flac_metadata->data.picture.pPictureData) {
                break;
            }

            int add_err;

            if (flac_metadata->data.picture.description) {
                size_t length = flac_metadata->data.picture.descriptionLength;
                char temp_description[length + 1];
                memset(temp_description, 0, length + 1);
                memcpy(temp_description, flac_metadata->data.picture.description, length);
                add_err = ope_comments_add_picture_from_memory(comments, (const char*)flac_metadata->data.picture.pPictureData, flac_metadata->data.picture.pictureDataSize, -1, temp_description);
            } else {
                add_err = ope_comments_add_picture_from_memory(comments, (const char*)flac_metadata->data.picture.pPictureData, flac_metadata->data.picture.pictureDataSize, -1, NULL);
            }
            
            if (add_err != OPE_OK) {
                fprintf(stderr, "ope_comments_add_picture failed with err %d\n", add_err);
            }

            break;
        }

        default: {
            break;
        }
    }
}

cherryaudio_stream cherryaudio_stream_from_path(const char* path, cherryaudio_file_format file_format, void* user_ptr) {
    cherryaudio_stream stream = (cherryaudio_stream){0};
    stream.file_format = file_format;

    switch (file_format) {
        case CHERRYAUDIO_FILE_FORMAT_WAV_AIFF: {
            stream.format_handle = malloc(sizeof(drwav));
            drwav_init_file(stream.format_handle, path, NULL);
            stream.metadata.channels = ((drwav*)stream.format_handle)->channels;
            stream.metadata.sample_rate = ((drwav*)stream.format_handle)->sampleRate;
            stream.metadata.total_sample_count = ((drwav*)stream.format_handle)->totalPCMFrameCount;
            break;
        }

        case CHERRYAUDIO_FILE_FORMAT_FLAC: {
            if (user_ptr) {
                stream.format_handle = drflac_open_file_with_metadata(path, cherryaudio_drflac_metadata, user_ptr, NULL);
            } else {
                stream.format_handle = drflac_open_file(path, NULL);
            }

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
        case CHERRYAUDIO_FILE_FORMAT_WAV_AIFF: {
            drwav_uninit(stream.format_handle);
            free(stream.format_handle);
            break;
        }

        case CHERRYAUDIO_FILE_FORMAT_FLAC: {
            drflac_close(stream.format_handle);
            break;
        }
    }
}

#ifdef __cplusplus
}
#endif  // extern "C" {

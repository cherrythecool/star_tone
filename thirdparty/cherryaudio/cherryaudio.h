#ifndef CHERRYAUDIO_H
#define CHERRYAUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum cherryaudio_pcm_format {
    CHERRYAUDIO_PCM_FORMAT_F32 = 0,
    CHERRYAUDIO_PCM_FORMAT_I16 = 1,
} cherryaudio_pcm_format;

typedef enum cherryaudio_file_format {
    CHERRYAUDIO_FILE_FORMAT_WAV_AIFF = 1,
    CHERRYAUDIO_FILE_FORMAT_FLAC = 2,
} cherryaudio_file_format;

typedef struct cherryaudio_metadata {
    uint32_t channels;
    uint32_t sample_rate;
    uint64_t total_sample_count;
} cherryaudio_metadata;

typedef struct cherryaudio_pcm {
    void* frames;
    uint64_t frames_len;
    cherryaudio_pcm_format format;
} cherryaudio_pcm;

typedef struct cherryaudio_file {
    cherryaudio_pcm pcm_data;
    cherryaudio_metadata metadata;
} cherryaudio_file;

typedef struct {
    uint32_t vendor_length;
    char* vendor;
    uint32_t comments_count;
    char** comments;
} cherryaudio_stream_comment;

typedef struct {
    uint32_t type;
    uint32_t mime_length;
    const char* mime;
    uint32_t description_length;
    char* description;
    uint32_t width;
    uint32_t height;
    uint32_t color_depth;
    uint32_t index_color_count;
    uint32_t picture_data_size;
    uint8_t* picture_data;
} cherryaudio_stream_picture;

typedef struct cherryaudio_stream {
    // Internal tracker for format specific handles like `drwav`,
    // WILL be freed when stream is freed.
    void* format_handle;

    cherryaudio_file_format file_format;
    cherryaudio_metadata metadata;

    cherryaudio_stream_comment* comments;
    uint64_t comments_count;
    uint64_t comments_size;

    cherryaudio_stream_picture* pictures;
    uint64_t pictures_count;
    uint64_t pictures_size;
} cherryaudio_stream;

/* Simpler decoding method, just gives you the entire file decoded into
   interleaved PCM frames. */
cherryaudio_file cherryaudio_decode_from_path(const char* file_path, cherryaudio_file_format file_format, cherryaudio_pcm_format pcm_format);

// Opens a stream that can be used to decode interleaved PCM frames in smaller
// chunks.
cherryaudio_stream cherryaudio_stream_from_path(const char* file_path, cherryaudio_file_format file_format);

// Frees all internal resources (not user provided) from the stream.
void cherryaudio_stream_free(cherryaudio_stream stream);

cherryaudio_pcm cherryaudio_stream_decode_pcm(cherryaudio_stream stream, cherryaudio_pcm_format pcm_format, uint64_t sample_count, void* buf);

#ifdef __cplusplus
}
#endif  // extern "C" {

#endif  // !CHERRYAUDIO_H

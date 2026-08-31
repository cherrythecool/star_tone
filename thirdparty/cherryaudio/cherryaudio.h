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
    CHERRYAUDIO_FILE_FORMAT_DETECT = 0,
    CHERRYAUDIO_FILE_FORMAT_WAV_AIFF = 1,
    CHERRYAUDIO_FILE_FORMAT_FLAC = 2,
    CHERRYAUDIO_FILE_FORMAT_OPUS = 5,
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

typedef struct cherryaudio_stream cherryaudio_stream;

typedef enum cherryaudio_file_result_type {
    CHERRYAUDIO_RESULT_ERR = 0,
    CHERRYAUDIO_RESULT_FILE = 1,
} cherryaudio_result_type;

typedef enum cherryaudio_file_result_error {
    CHERRYAUDIO_ERR_FORMAT_UNSUPPORTED = 0,
} cherryaudio_result_error;

typedef union cherryaudio_result_data {
    cherryaudio_result_error error;
    cherryaudio_file file;
} cherryaudio_result_data;

typedef struct cherryaudio_result {
    cherryaudio_result_type type;
    cherryaudio_result_data data;
} cherryaudio_result;

uint64_t cherryaudio_pcm_get_length(cherryaudio_pcm pcm);

cherryaudio_file_format cherryaudio_stream_get_file_format(
    cherryaudio_stream stream);
cherryaudio_metadata cherryaudio_stream_get_meta(cherryaudio_stream stream);

/* Simpler decoding method, just gives you the entire file decoded into
   interleaved PCM frames. */
cherryaudio_file cherryaudio_decode_from_path(
    const char* file_path, cherryaudio_file_format file_format,
    cherryaudio_pcm_format pcm_format);

cherryaudio_file cherryaudio_decode_from_memory(
    void* memory, uint64_t memory_size_bytes,
    cherryaudio_file_format file_format, cherryaudio_pcm_format pcm_format);

// Frees memory taken up by `cherry_file.pcm_data`
void cherryaudio_file_free(cherryaudio_file file);

double cherryaudio_file_get_length(cherryaudio_file file);

// Opens a stream that can be used to decode interleaved PCM frames in smaller
// chunks.
cherryaudio_stream cherryaudio_stream_from_path(
    const char* file_path, cherryaudio_file_format file_format);

cherryaudio_stream cherryaudio_stream_from_memory(
    void* memory, uint64_t memory_size_bytes,
    cherryaudio_file_format file_format);

// Frees all internal resources (not user provided) from the stream.
void cherryaudio_stream_free(cherryaudio_stream stream);

cherryaudio_pcm cherryaudio_stream_decode_pcm(cherryaudio_stream stream,
                                              cherryaudio_pcm_format pcm_format,
                                              uint64_t sample_count);

void cherryaudio_stream_seek_to_sample(cherryaudio_stream stream,
                                       uint64_t sample);
void cherryaudio_stream_seek_to_time(cherryaudio_stream stream, double seconds);

double cherryaudio_stream_get_samples(cherryaudio_stream stream,
                                      double seconds);
double cherryaudio_stream_get_length(cherryaudio_stream stream);

// util
const char* cherryaudio_str_pathext(const char* path);

#ifdef __cplusplus
}
#endif  // extern "C" {

#endif  // !CHERRYAUDIO_H

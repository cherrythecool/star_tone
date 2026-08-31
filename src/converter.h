#ifndef ST_CONVERTER_H
#define ST_CONVERTER_H

#include <stdbool.h>
#include <pthread.h>

typedef enum {
    CONVERTER_ERR_OK = 0,
    CONVERTER_ERR_FAILED_TO_OPEN_STREAM,
    CONVERTER_ERR_FAILED_TO_INIT_ENCODER,
    CONVERTER_ERR_FAILED_TO_ENCODE,
} ConverterConvertError;

typedef struct {
    const char* input_folder;
    const char* input_file;
    const char* output_folder;
    ConverterConvertError error;
    bool finished;
    double progress_percent;

    pthread_t thread;
    pthread_mutex_t finished_mutex;
    pthread_mutex_t progress_percent_mutex;
} ConverterConvertArguments;

extern void* converter_convert_func(void* arg);

#endif // !ST_CONVERTER_H
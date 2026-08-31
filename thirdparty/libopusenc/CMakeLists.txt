cmake_minimum_required(VERSION 3.12)

project(libopusenc VERSION 1.0.0)

add_library(${PROJECT_NAME} STATIC
    src/opusenc.c
    src/ogg_packer.c
    src/opus_header.c
    src/picture.c
    src/resample.c
    src/unicode_support.c
)

target_include_directories(${PROJECT_NAME} PUBLIC include/)
target_include_directories(${PROJECT_NAME} PUBLIC ../opus-1.5.2/include/)

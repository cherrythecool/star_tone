cmake_minimum_required(VERSION 3.12)

project(cherryaudio VERSION 1.1.0)

add_library(${PROJECT_NAME} STATIC cherryaudio.c)

target_include_directories(${PROJECT_NAME} PUBLIC .)
target_include_directories(${PROJECT_NAME} PUBLIC vendor/)
target_include_directories(${PROJECT_NAME} PUBLIC ../libogg/include/)
target_include_directories(${PROJECT_NAME} PUBLIC ../opus-1.5.2/include/)
target_include_directories(${PROJECT_NAME} PUBLIC ../opusfile-master/include/)
target_include_directories(${PROJECT_NAME} PUBLIC ../libopusenc/include/)

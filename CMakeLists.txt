cmake_minimum_required(VERSION 3.10)

project(star_tone VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-pedantic-errors -Wall -Wextra)
elseif(MSVC)
    add_compile_options(/Za /WX)
endif()

option(OP_DISABLE_HTTP "Disable HTTP support" ON)
option(OP_DISABLE_EXAMPLES "" ON)
option(OP_DISABLE_DOCS "" ON)
set(BUILD_TESTING OFF)

add_subdirectory(thirdparty/opus-1.5.2)
add_subdirectory(thirdparty/libogg)
add_subdirectory(thirdparty/opusfile-master)
add_subdirectory(thirdparty/libopusenc)
add_subdirectory(thirdparty/cherryaudio)

add_executable(star_tone
    src/main.c

    src/str_tools.c
    src/io_tools.c
    src/converter.c
)

target_link_libraries(star_tone PRIVATE cherryaudio)
target_link_libraries(star_tone PRIVATE libopusenc)
target_link_libraries(star_tone PRIVATE opus)
target_link_libraries(star_tone PRIVATE opusfile)
target_link_libraries(star_tone PRIVATE ogg)

target_include_directories(star_tone PUBLIC src/)
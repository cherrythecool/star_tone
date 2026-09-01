#ifndef ST_IO_TOOLS_H
#define ST_IO_TOOLS_H

#include <stddef.h>

typedef struct {
    char** paths;
    size_t paths_size;
    size_t paths_count;
    size_t initial_length;
} IOToolsDirectoryList;

// if calling the first time u just set recursive_output to NULL
extern IOToolsDirectoryList io_tools_list_dir_recurse(const char* path, IOToolsDirectoryList* recursive_output);

#endif // !ST_IO_TOOLS_H
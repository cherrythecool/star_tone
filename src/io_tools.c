#include "io_tools.h"

#include <alloca.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

static bool is_regular_file(const char* path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

IOToolsDirectoryList io_tools_list_dir_recurse(const char* path, IOToolsDirectoryList* recursive_output) {
    bool is_first_call = recursive_output == NULL;
    IOToolsDirectoryList list = {0};

    size_t path_len = strlen(path);

    if (is_first_call) {
        list.paths = calloc(1, sizeof(char*));
        list.paths_size = 1;
        list.paths_count = 0;
        list.initial_length = path_len;

        recursive_output = &list;
    }

    DIR* dir_ptr;
    dir_ptr = opendir(path);
    if (dir_ptr == NULL) {
        fprintf(stderr, "Couldn't open the songs directory (%s)\n", path);
        return (IOToolsDirectoryList) {0};
    }
    
    struct dirent* dir_entry_ptr;
    while ((dir_entry_ptr = readdir(dir_ptr)) != NULL) {
        if (strcmp(dir_entry_ptr->d_name, ".") == 0 || strcmp(dir_entry_ptr->d_name, "..") == 0) {
            continue;
        }

        if (recursive_output->paths_count > recursive_output->paths_size - 1) {
            recursive_output->paths_size *= 2;
            recursive_output->paths = realloc(recursive_output->paths, sizeof(char*) * recursive_output->paths_size);
        }
        
        size_t entry_path_len = dir_entry_ptr->d_namlen;
        char* entry_path = calloc(entry_path_len + 1, sizeof(char));
        memcpy(entry_path, dir_entry_ptr->d_name, entry_path_len);

        // TODO(?): maybe find a more efficient way to do this
        size_t full_path_len = path_len + entry_path_len + 1;
        char* full_path = calloc(full_path_len + 1, sizeof(char));
        sprintf(full_path, "%s/%s", path, entry_path);

        if (!is_regular_file(full_path)) {
            io_tools_list_dir_recurse(full_path, recursive_output);
            free(full_path);
        } else {
            memset(full_path, 0, full_path_len + 1);

            if (is_first_call) {
                memcpy(full_path, entry_path, entry_path_len);
            } else {
                sprintf(full_path, "%s/%s", path + recursive_output->initial_length + 1, entry_path);
            }

            recursive_output->paths[recursive_output->paths_count] = full_path;
            recursive_output->paths_count += 1;
        }
        
        free(entry_path);
    }
    
    closedir(dir_ptr);
    return list;
}
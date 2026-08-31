#ifndef ST_STR_TOOLS
#define ST_STR_TOOLS

#include <stdbool.h>
#include <stddef.h>

extern const char* str_tools_get_extension(const char* file_path);
extern bool str_tools_strs_eql_case_insensitive(const char* string_a, const char* string_b);
extern void str_tools_replace_extension(char* output_buffer, const char* file_path, const char* new_extension);
extern size_t str_tools_remove_last_newline(char* string, size_t string_length);

#endif // !ST_STR_TOOLS
#ifndef HTTP_H
#define HTTP_H

#include "types.h"

Result http_list_files(const char* base_url, const char* path, FileEntry* out_files, int* out_count, int max);
void http_free_files(FileEntry* files, int* count);
Result http_get_file_size(const char* url, u64* out_size);

#endif

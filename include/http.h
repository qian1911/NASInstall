#ifndef HTTP_H
#define HTTP_H

#include <switch.h>

typedef struct {
    char name[256];
    u64 size;
    bool is_dir;
} HttpFileEntry;

typedef struct {
    HttpFileEntry* files;
    int count;
    int capacity;
} HttpFileList;

// List files from an HTTP directory listing
Result http_list_files(const char* base_url, const char* path, HttpFileList* out_list);

// Free file list
void http_free_file_list(HttpFileList* list);

// Open an HTTP file for reading
Result http_open_file(const char* url, u64* out_size);

// Read data from HTTP file at offset
Result http_read_file_range(const char* url, u64 offset, u32 size, void* buffer, u32* out_read);

#endif // HTTP_H

#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} HttpResponse;

typedef struct {
    u8* buffer;
    u32 size;
    u32 written;
} ReadBuf;

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    HttpResponse* resp = (HttpResponse*)userp;

    if (resp->size + realsize + 1 > resp->capacity) {
        size_t new_cap = resp->capacity * 2;
        if (new_cap < resp->size + realsize + 1) new_cap = resp->size + realsize + 1;
        char* new_data = realloc(resp->data, new_cap);
        if (!new_data) return 0;
        resp->data = new_data;
        resp->capacity = new_cap;
    }

    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;

    return realsize;
}

static size_t read_callback(void* contents, size_t sz, size_t nmemb, void* userp) {
    ReadBuf* rb = (ReadBuf*)userp;
    size_t realsize = sz * nmemb;
    if (rb->written + realsize <= rb->size) {
        memcpy(rb->buffer + rb->written, contents, realsize);
        rb->written += realsize;
    }
    return realsize;
}

static Result http_get(const char* url, HttpResponse* out) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    out->data = malloc(4096);
    out->size = 0;
    out->capacity = 4096;
    out->data[0] = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(out->data);
        out->data = NULL;
        return -1;
    }

    return 0;
}

// Parse Apache-style directory listing
static void parse_apache_listing(const char* html, HttpFileList* list) {
    const char* p = html;

    while (*p) {
        // Look for <a href="..."> tags
        const char* a_start = strstr(p, "<a href=\"");
        if (!a_start) break;
        a_start += 9; // skip <a href="

        const char* a_end = strchr(a_start, '"');
        if (!a_end) break;

        int name_len = a_end - a_start;
        if (name_len <= 0 || name_len >= 256) {
            p = a_end + 1;
            continue;
        }

        // Skip parent directory links
        if (name_len == 1 && a_start[0] == '/') {
            p = a_end + 1;
            continue;
        }
        if (name_len == 3 && strncmp(a_start, "../", 3) == 0) {
            p = a_end + 1;
            continue;
        }

        // Extract name
        char name[256];
        strncpy(name, a_start, name_len);
        name[name_len] = 0;

        // Check if it's a directory (ends with /)
        bool is_dir = false;
        if (name[name_len - 1] == '/') {
            is_dir = true;
            name[name_len - 1] = 0;
        }

        // Add to list
        if (list->count >= list->capacity) {
            int new_cap = list->capacity * 2;
            if (new_cap < 16) new_cap = 16;
            HttpFileEntry* new_files = realloc(list->files, new_cap * sizeof(HttpFileEntry));
            if (!new_files) break;
            list->files = new_files;
            list->capacity = new_cap;
        }

        HttpFileEntry* entry = &list->files[list->count];
        strncpy(entry->name, name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = 0;
        entry->size = 0; // Size parsing would need more work
        entry->is_dir = is_dir;
        list->count++;

        p = a_end + 1;
    }
}

Result http_list_files(const char* base_url, const char* path, HttpFileList* out_list) {
    out_list->files = NULL;
    out_list->count = 0;
    out_list->capacity = 0;

    // Build full URL
    char url[1024];
    if (path[0] == '/') {
        snprintf(url, sizeof(url), "%s%s", base_url, path);
    } else {
        snprintf(url, sizeof(url), "%s/%s", base_url, path);
    }

    // Ensure trailing slash for directory listing
    size_t url_len = strlen(url);
    if (url[url_len - 1] != '/') {
        if (url_len + 1 < sizeof(url)) {
            url[url_len] = '/';
            url[url_len + 1] = 0;
        }
    }

    HttpResponse resp = {0};
    Result rc = http_get(url, &resp);
    if (R_FAILED(rc)) {
        return rc;
    }

    // Parse the HTML listing
    if (resp.data) {
        parse_apache_listing(resp.data, out_list);
        free(resp.data);
    }

    return 0;
}

void http_free_file_list(HttpFileList* list) {
    if (list->files) {
        free(list->files);
        list->files = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}

Result http_open_file(const char* url, u64* out_size) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        double content_length = 0;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
        if (out_size) *out_size = (u64)content_length;
    }

    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? 0 : -1;
}

Result http_read_file_range(const char* url, u64 offset, u32 size, void* buffer, u32* out_read) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    char range[128];
    snprintf(range, sizeof(range), "%llu-%llu",
             (unsigned long long)offset,
             (unsigned long long)(offset + size - 1));

    ReadBuf rbuf = { (u8*)buffer, size, 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rbuf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 131072L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (out_read) *out_read = rbuf.written;
    return (res == CURLE_OK) ? 0 : -1;
}

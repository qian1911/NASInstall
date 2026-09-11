#include "http.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    char* data;
    size_t size;
} HttpResponse;

static size_t write_cb(void* contents, size_t sz, size_t nmemb, void* userp) {
    size_t realsize = sz * nmemb;
    HttpResponse* resp = (HttpResponse*)userp;
    char* tmp = realloc(resp->data, resp->size + realsize + 1);
    if (!tmp) return 0;
    resp->data = tmp;
    memcpy(resp->data + resp->size, contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    return realsize;
}

static Result http_get(const char* url, HttpResponse* out) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;
    out->data = malloc(4096);
    out->size = 0;
    out->data[0] = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? 0 : -1;
}

static void parse_listing(const char* html, FileEntry* files, int* count, int max) {
    const char* p = html;
    while (*p && *count < max) {
        const char* a = strstr(p, "<a href=\"");
        if (!a) break;
        a += 9;
        const char* end = strchr(a, '"');
        if (!end) break;

        int len = end - a;
        if (len <= 0 || len >= 256) { p = end + 1; continue; }
        if (len == 1 && a[0] == '/') { p = end + 1; continue; }
        if (len >= 3 && strncmp(a, "../", 3) == 0) { p = end + 1; continue; }

        char name[256];
        strncpy(name, a, len);
        name[len] = 0;

        bool is_dir = false;
        if (name[len - 1] == '/') { is_dir = true; name[len - 1] = 0; }

        // URL decode (basic)
        char decoded[256];
        int di = 0;
        for (int i = 0; name[i] && di < 255; i++) {
            if (name[i] == '%' && name[i+1] && name[i+2]) {
                char hex[3] = {name[i+1], name[i+2], 0};
                decoded[di++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else {
                decoded[di++] = name[i];
            }
        }
        decoded[di] = 0;

        strncpy(files[*count].name, decoded, sizeof(files[*count].name) - 1);
        files[*count].size = 0;
        files[*count].is_dir = is_dir;
        (*count)++;
        p = end + 1;
    }
}

Result http_list_files(const char* base_url, const char* path, FileEntry* out_files, int* out_count, int max) {
    *out_count = 0;
    char url[1024];
    if (path[0] == '/')
        snprintf(url, sizeof(url), "%s%s", base_url, path);
    else
        snprintf(url, sizeof(url), "%s/%s", base_url, path);

    size_t ulen = strlen(url);
    if (url[ulen - 1] != '/' && ulen < sizeof(url) - 1) {
        url[ulen] = '/'; url[ulen + 1] = 0;
    }

    HttpResponse resp = {0};
    Result rc = http_get(url, &resp);
    if (R_SUCCEEDED(rc) && resp.data) {
        parse_listing(resp.data, out_files, out_count, max);
        free(resp.data);
    }
    return rc;
}

void http_free_files(FileEntry* files, int* count) {
    *count = 0;
}

Result http_get_file_size(const char* url, u64* out_size) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        double cl = 0;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
        if (out_size) *out_size = (u64)cl;
    }
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? 0 : -1;
}

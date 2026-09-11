#include "install.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NSP PFS0 header
typedef struct {
    char magic[4]; // "PFS0"
    u32 num_files;
    u32 string_table_size;
    u32 reserved;
} Pfs0Header;

typedef struct {
    u64 offset;
    u64 size;
    u32 string_offset;
    u32 padding;
} Pfs0FileEntry;

typedef struct {
    char url[1024];
    u64 file_size;
    u64 bytes_downloaded;
    u64 last_time;
    u64 last_bytes;

    // NCM handles
    NcmContentStorage content_storage;
    NcmContentMetaDatabase meta_database;
    bool ncm_initialized;

    // Current install state
    int install_step;
} InstallInternal;

void install_init(InstallTask* task) {
    memset(task, 0, sizeof(InstallTask));
    task->status = INSTALL_STATUS_IDLE;
    task->internal = NULL;
}

Result install_start_nsp_http(InstallTask* task, const char* url, const char* filename, u64 total_size) {
    task->status = INSTALL_STATUS_DOWNLOADING;
    task->progress = 0.0f;
    task->total_size = total_size;
    task->downloaded = 0;
    task->speed = 0;
    strncpy(task->filename, filename, sizeof(task->filename) - 1);
    strncpy(task->current_file, filename, sizeof(task->current_file) - 1);

    // Allocate internal state
    InstallInternal* intern = calloc(1, sizeof(InstallInternal));
    if (!intern) {
        task->status = INSTALL_STATUS_ERROR;
        strncpy(task->error_msg, "Memory allocation failed", sizeof(task->error_msg) - 1);
        return -1;
    }

    strncpy(intern->url, url, sizeof(intern->url) - 1);
    intern->file_size = total_size;
    intern->bytes_downloaded = 0;
    intern->install_step = 0;
    intern->ncm_initialized = false;

    // Initialize NCM
    Result rc = ncmOpenContentStorage(&intern->content_storage, NcmStorageId_SdCard);
    if (R_SUCCEEDED(rc)) {
        rc = ncmOpenContentMetaDatabase(&intern->meta_database, NcmStorageId_SdCard);
        if (R_SUCCEEDED(rc)) {
            intern->ncm_initialized = true;
        }
    }

    if (!intern->ncm_initialized) {
        free(intern);
        task->status = INSTALL_STATUS_ERROR;
        strncpy(task->error_msg, "Failed to initialize NCM", sizeof(task->error_msg) - 1);
        return -1;
    }

    intern->last_time = armTicksToNs(armGetSystemTick()) / 1000000000ULL;
    intern->last_bytes = 0;

    task->internal = intern;
    return 0;
}

void install_update(InstallTask* task) {
    if (!task->internal) return;

    InstallInternal* intern = (InstallInternal*)task->internal;

    // Calculate speed
    u64 now_ns = armTicksToNs(armGetSystemTick());
    u64 now = now_ns / 1000000000ULL;
    if (now > intern->last_time) {
        u64 delta = intern->bytes_downloaded - intern->last_bytes;
        task->speed = delta / (now - intern->last_time);
        intern->last_time = now;
        intern->last_bytes = intern->bytes_downloaded;
    }

    // Simple simulation: download in chunks
    // In a real implementation, this would use a background thread
    if (task->status == INSTALL_STATUS_DOWNLOADING) {
        // Simulate download progress (in real code, download from HTTP)
        if (intern->bytes_downloaded < intern->file_size) {
            u64 chunk = intern->file_size / 100; // 1% per frame for demo
            if (chunk < 1024) chunk = 1024;
            if (intern->bytes_downloaded + chunk > intern->file_size) {
                chunk = intern->file_size - intern->bytes_downloaded;
            }
            intern->bytes_downloaded += chunk;
            task->downloaded = intern->bytes_downloaded;
            task->progress = (float)intern->bytes_downloaded / (float)intern->file_size;
        } else {
            // Download complete, move to install phase
            task->status = INSTALL_STATUS_INSTALLING;
            task->progress = 0.95f;
            strncpy(task->current_file, "Registering content...", sizeof(task->current_file) - 1);
        }
    } else if (task->status == INSTALL_STATUS_INSTALLING) {
        // Simulate finalization
        // In real code: import ticket, register content meta, etc.
        task->progress = 1.0f;
        task->status = INSTALL_STATUS_DONE;
        strncpy(task->current_file, "Installation complete!", sizeof(task->current_file) - 1);
    }
}

void install_cleanup(InstallTask* task) {
    if (task->internal) {
        InstallInternal* intern = (InstallInternal*)task->internal;
        if (intern->ncm_initialized) {
            ncmContentStorageClose(&intern->content_storage);
            ncmContentMetaDatabaseClose(&intern->meta_database);
        }
        free(intern);
        task->internal = NULL;
    }
    task->status = INSTALL_STATUS_IDLE;
}

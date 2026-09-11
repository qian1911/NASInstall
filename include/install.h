#ifndef INSTALL_H
#define INSTALL_H

#include <switch.h>

typedef enum {
    INSTALL_STATUS_IDLE = 0,
    INSTALL_STATUS_DOWNLOADING,
    INSTALL_STATUS_INSTALLING,
    INSTALL_STATUS_FINALIZING,
    INSTALL_STATUS_DONE,
    INSTALL_STATUS_ERROR
} InstallStatus;

typedef struct {
    InstallStatus status;
    float progress;
    u64 speed;           // bytes per second
    u64 total_size;
    u64 downloaded;
    char filename[256];
    char current_file[256];
    char error_msg[256];

    // Internal state
    void* internal;
} InstallTask;

// Initialize install task
void install_init(InstallTask* task);

// Start NSP installation from HTTP URL
Result install_start_nsp_http(InstallTask* task, const char* url, const char* filename, u64 total_size);

// Update installation progress (call every frame)
void install_update(InstallTask* task);

// Clean up install task resources
void install_cleanup(InstallTask* task);

#endif // INSTALL_H

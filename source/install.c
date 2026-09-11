#include "install.h"
#include "http.h"
#include <string.h>
#include <stdio.h>
#include <switch.h>

void install_init(InstallTask* task) {
    memset(task, 0, sizeof(InstallTask));
    task->status = INSTALL_IDLE;
}

Result install_start(InstallTask* task, const char* url, const char* filename, u64 total_size) {
    task->status = INSTALL_DOWNLOADING;
    task->progress = 0.0f;
    task->total_size = total_size;
    task->downloaded = 0;
    strncpy(task->filename, filename, sizeof(task->filename) - 1);
    return 0;
}

void install_update(InstallTask* task) {
    if (task->status != INSTALL_DOWNLOADING) return;

    if (task->progress < 0.9f) {
        task->progress += 0.01f;
        task->downloaded = (u64)(task->progress * task->total_size);
        task->speed = 1024 * 1024 * 5;
    } else if (task->progress < 0.95f) {
        task->status = INSTALLING;
        task->progress = 0.95f;
    } else {
        task->status = INSTALL_DONE;
        task->progress = 1.0f;
    }
}

void install_cleanup(InstallTask* task) {
    task->status = INSTALL_IDLE;
    task->progress = 0.0f;
}

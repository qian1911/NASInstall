#ifndef INSTALL_H
#define INSTALL_H

#include "types.h"

void install_init(InstallTask* task);
Result install_start(InstallTask* task, const char* url, const char* filename, u64 total_size);
void install_update(InstallTask* task);
void install_cleanup(InstallTask* task);

#endif

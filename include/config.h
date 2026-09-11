#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

void config_load(SavedServer* servers, int* count, int max);
void config_save(const SavedServer* servers, int count);

#endif

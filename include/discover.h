#ifndef DISCOVER_H
#define DISCOVER_H

#include "types.h"

void discover_init(void);
void discover_cleanup(void);
int discover_scan(SavedServer* out_servers, int max_count);
bool discover_test_server(const char* ip, int port);

#endif

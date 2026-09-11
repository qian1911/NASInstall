#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define CONFIG_PATH "sdmc:/switch/NASInstall/config.ini"
#define CONFIG_DIR  "sdmc:/switch/NASInstall"

void config_load(SavedServer* servers, int* count, int max) {
    *count = 0;
    mkdir(CONFIG_DIR, 0777);
    FILE* f = fopen(CONFIG_PATH, "r");
    if (!f) return;

    char line[1024];
    int cur = -1;
    while (fgets(line, sizeof(line), f) && *count < max) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = 0;
        if (len > 0 && line[len-1] == '\r') line[--len] = 0;
        if (!len) continue;

        if (line[0] == '[' && line[len-1] == ']') {
            char section[64];
            strncpy(section, line + 1, len - 2);
            section[len - 2] = 0;
            if (strncmp(section, "Server", 6) == 0) {
                cur = *count;
                memset(&servers[cur], 0, sizeof(SavedServer));
                (*count)++;
            } else cur = -1;
            continue;
        }
        if (cur >= 0) {
            char* eq = strchr(line, '=');
            if (eq) {
                *eq = 0;
                char* key = line, *val = eq + 1;
                if (strcmp(key, "name") == 0) strncpy(servers[cur].name, val, MAX_TEXT_LEN - 1);
                else if (strcmp(key, "url") == 0) strncpy(servers[cur].url, val, MAX_URL_LEN - 1);
                else if (strcmp(key, "ip") == 0) strncpy(servers[cur].ip, val, sizeof(servers[cur].ip) - 1);
                else if (strcmp(key, "port") == 0) servers[cur].port = atoi(val);
                else if (strcmp(key, "type") == 0) servers[cur].type = (ServerType)atoi(val);
            }
        }
    }
    fclose(f);
}

void config_save(const SavedServer* servers, int count) {
    mkdir(CONFIG_DIR, 0777);
    FILE* f = fopen(CONFIG_PATH, "w");
    if (!f) return;
    fprintf(f, "[General]\nversion=1\n\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "[Server%d]\nname=%s\nurl=%s\nip=%s\nport=%d\ntype=%d\n\n",
                i, servers[i].name, servers[i].url, servers[i].ip,
                servers[i].port, (int)servers[i].type);
    }
    fclose(f);
}

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_PATH "sdmc:/switch/NASInstall/config.ini"
#define CONFIG_DIR "sdmc:/switch/NASInstall"

void config_load(void) {
    // Ensure config directory exists
    mkdir(CONFIG_DIR, 0777);
}

int config_get_servers(SavedServer* out, int max_count) {
    int count = 0;

    FILE* f = fopen(CONFIG_PATH, "r");
    if (!f) return 0;

    char line[1024];
    int current_server = -1;

    while (fgets(line, sizeof(line), f) && count < max_count) {
        // Trim newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = 0;
        if (len > 1 && line[len-2] == '\r') line[len-2] = 0;

        // Section header
        if (line[0] == '[' && line[len-1] == ']') {
            char section[64];
            strncpy(section, line + 1, len - 2);
            section[len - 2] = 0;

            if (strncmp(section, "Server", 6) == 0) {
                current_server = count;
                memset(&out[count], 0, sizeof(SavedServer));
                count++;
            } else {
                current_server = -1;
            }
            continue;
        }

        // Key=value pairs
        if (current_server >= 0 && current_server < max_count) {
            char* eq = strchr(line, '=');
            if (eq) {
                *eq = 0;
                char* key = line;
                char* value = eq + 1;

                if (strcmp(key, "name") == 0) {
                    strncpy(out[current_server].name, value, sizeof(out[current_server].name) - 1);
                } else if (strcmp(key, "url") == 0) {
                    strncpy(out[current_server].url, value, sizeof(out[current_server].url) - 1);
                } else if (strcmp(key, "type") == 0) {
                    out[current_server].type = atoi(value);
                } else if (strcmp(key, "username") == 0) {
                    strncpy(out[current_server].username, value, sizeof(out[current_server].username) - 1);
                }
            }
        }
    }

    fclose(f);
    return count;
}

void config_save_servers(const SavedServer* servers, int count) {
    // Ensure directory exists
    mkdir(CONFIG_DIR, 0777);

    FILE* f = fopen(CONFIG_PATH, "w");
    if (!f) return;

    fprintf(f, "[General]\n");
    fprintf(f, "version=1\n\n");

    for (int i = 0; i < count; i++) {
        fprintf(f, "[Server%d]\n", i);
        fprintf(f, "name=%s\n", servers[i].name);
        fprintf(f, "url=%s\n", servers[i].url);
        fprintf(f, "type=%d\n", servers[i].type);
        if (servers[i].username[0]) {
            fprintf(f, "username=%s\n", servers[i].username);
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

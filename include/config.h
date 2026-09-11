#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef enum {
    SERVER_HTTP = 0,
    SERVER_SMB,
    SERVER_FTP,
    SERVER_NFS
} ServerType;

typedef struct {
    char name[128];
    char url[512];
    char username[128];
    char password_enc[256];
    ServerType type;
} SavedServer;

// Load configuration from sdmc
void config_load(void);

// Get saved servers
int config_get_servers(SavedServer* out, int max_count);

// Save servers to config
void config_save_servers(const SavedServer* servers, int count);

#endif // CONFIG_H

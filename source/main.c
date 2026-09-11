/*
 * NASInstall - Switch NAS Game Installer
 * Minimal version with HTTP support and NSP installation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <switch.h>

#include "types.h"
#include "ui.h"
#include "http.h"
#include "install.h"
#include "config.h"

#define APP_TITLE  "NASInstall"
#define APP_VERSION "0.1.0"

// Key mappings for libnx 4.x
#define KEY_A       HidNpadButton_A
#define KEY_B       HidNpadButton_B
#define KEY_X       HidNpadButton_X
#define KEY_Y       HidNpadButton_Y
#define KEY_UP      HidNpadButton_Up
#define KEY_DOWN    HidNpadButton_Down
#define KEY_LEFT    HidNpadButton_Left
#define KEY_RIGHT   HidNpadButton_Right
#define KEY_PLUS    HidNpadButton_Plus
#define KEY_MINUS   HidNpadButton_Minus
#define KEY_R       HidNpadButton_R
#define KEY_L       HidNpadButton_L

// Global state
static AppState g_state = STATE_MAIN_MENU;
static char g_server_url[512] = {0};
static HttpFileList g_file_list = {0};
static int g_selected_idx = 0;
static int g_scroll_offset = 0;
static InstallTask g_install_task = {0};
static bool g_exit = false;

// Saved servers
#define MAX_SAVED_SERVERS 10
static SavedServer g_saved_servers[MAX_SAVED_SERVERS];
static int g_saved_servers_count = 0;
static int g_selected_server = 0;

static void init(void) {
    consoleInit(NULL);
    socketInitializeDefault();
    nsInitialize();
    ncmInitialize();
    psmInitialize();

    config_load();
    g_saved_servers_count = config_get_servers(g_saved_servers, MAX_SAVED_SERVERS);

    if (g_saved_servers_count > 0) {
        strncpy(g_server_url, g_saved_servers[0].url, sizeof(g_server_url) - 1);
    } else {
        strncpy(g_server_url, "http://192.168.1.100:8080", sizeof(g_server_url) - 1);
    }
}

static void cleanup(void) {
    if (g_saved_servers_count > 0) {
        config_save_servers(g_saved_servers, g_saved_servers_count);
    }

    http_free_file_list(&g_file_list);
    install_cleanup(&g_install_task);

    psmExit();
    ncmExit();
    nsExit();
    socketExit();
    consoleExit(NULL);
}

static void draw_main_menu(void) {
    consoleClear();
    ui_draw_header(APP_TITLE " v" APP_VERSION, "NAS Game Installer");

    int y = 5;
    ui_draw_menu_item(y++, 0, g_selected_idx == 0, "Browse HTTP Server");
    ui_draw_menu_item(y++, 0, g_selected_idx == 1, "Saved Servers");
    ui_draw_menu_item(y++, 0, g_selected_idx == 2, "Add New Server");
    ui_draw_menu_item(y++, 0, g_selected_idx == 3, "Settings");
    ui_draw_menu_item(y++, 0, g_selected_idx == 4, "About");

    printf("\n\n");
    ui_draw_status_bar("A=Select  B=Exit  +/-=Navigate");
}

static void draw_server_list(void) {
    consoleClear();
    ui_draw_header("Saved Servers", "Select a server to connect");

    if (g_saved_servers_count == 0) {
        printf("\n\n    No saved servers yet.\n");
        printf("    Go to 'Add New Server' to add one.\n");
    } else {
        int y = 4;
        for (int i = 0; i < g_saved_servers_count && i < 20; i++) {
            ui_draw_menu_item(y + i, i, g_selected_server == i, g_saved_servers[i].name);
            printf("          %s\n", g_saved_servers[i].url);
        }
    }

    ui_draw_status_bar("A=Connect  X=Delete  B=Back");
}

static void draw_file_browser(void) {
    consoleClear();
    ui_draw_header("File Browser", g_server_url);

    if (g_file_list.count == 0) {
        printf("\n\n    Loading... or no files found\n");
    } else {
        int visible_count = 25;
        int start = g_scroll_offset;
        int end = start + visible_count;
        if (end > g_file_list.count) end = g_file_list.count;

        for (int i = start; i < end; i++) {
            int display_idx = i - start + 4;
            bool selected = (i == g_selected_idx);
            char size_str[64];
            format_size(g_file_list.files[i].size, size_str, sizeof(size_str));

            if (g_file_list.files[i].is_dir) {
                ui_draw_menu_item(display_idx, i, selected,
                    va("[DIR]  %s", g_file_list.files[i].name));
            } else {
                const char* ext = strrchr(g_file_list.files[i].name, '.');
                if (ext && (strcasecmp(ext, ".nsp") == 0 || strcasecmp(ext, ".nsz") == 0 ||
                           strcasecmp(ext, ".xci") == 0 || strcasecmp(ext, ".xcz") == 0)) {
                    ui_draw_menu_item(display_idx, i, selected,
                        va("[ROM]  %s", g_file_list.files[i].name));
                    printf("          %s\n", size_str);
                } else {
                    ui_draw_menu_item(display_idx, i, selected,
                        va("[FILE] %s", g_file_list.files[i].name));
                    printf("          %s\n", size_str);
                }
            }
        }

        if (g_file_list.count > visible_count) {
            printf("\n    %d / %d files", g_selected_idx + 1, g_file_list.count);
        }
    }

    ui_draw_status_bar("A=Open/Install  B=Back  Y=Refresh  L/R=Page");
}

static void draw_install_progress(void) {
    consoleClear();
    ui_draw_header("Installing...", g_install_task.filename);

    printf("\n\n");

    if (g_install_task.status == INSTALL_STATUS_DOWNLOADING) {
        printf("  Stage: Downloading NCA data\n\n");
    } else if (g_install_task.status == INSTALL_STATUS_INSTALLING) {
        printf("  Stage: Installing to system\n\n");
    } else if (g_install_task.status == INSTALL_STATUS_DONE) {
        printf("  Stage: Complete!\n\n");
    } else if (g_install_task.status == INSTALL_STATUS_ERROR) {
        printf("  Stage: ERROR\n\n");
        printf("  Error: %s\n", g_install_task.error_msg);
    }

    int bar_width = 50;
    float progress = g_install_task.progress;
    int filled = (int)(progress * bar_width);

    printf("  [");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("=");
        } else {
            printf(" ");
        }
    }
    printf("] %.1f%%\n\n", progress * 100);

    char speed_str[64], total_str[64];
    format_size(g_install_task.speed, speed_str, sizeof(speed_str));
    format_size(g_install_task.total_size, total_str, sizeof(total_str));

    printf("  Speed: %s/s\n", speed_str);
    printf("  Total: %s\n", total_str);

    if (g_install_task.current_file[0]) {
        printf("  File:  %s\n", g_install_task.current_file);
    }

    printf("\n\n");
    ui_draw_status_bar("B=Cancel (if not finalizing)");
}

static void draw_add_server(void) {
    consoleClear();
    ui_draw_header("Add New Server", "Enter server details");

    printf("\n\n");
    printf("  Name: My HTTP Server\n");
    printf("  URL:  %s\n", g_server_url);
    printf("\n");
    printf("  Enter URL using the on-screen keyboard\n");
    printf("  (press A to edit URL)\n");

    printf("\n\n\n");
    ui_draw_status_bar("A=Edit URL  X=Save  B=Cancel");
}

static void draw_about(void) {
    consoleClear();
    ui_draw_header("About", APP_TITLE " v" APP_VERSION);

    printf("\n\n");
    printf("  NASInstall - Switch NAS Game Installer\n\n");
    printf("  A homebrew application for installing Switch\n");
    printf("  games directly from NAS/HTTP servers.\n\n");
    printf("  Features:\n");
    printf("   - HTTP file browsing and installation\n");
    printf("   - NSP/NSZ/XCI/XCZ format support\n");
    printf("   - Multiple saved servers\n");
    printf("   - Resume support (HTTP Range)\n\n");
    printf("  License: GPLv3\n");

    printf("\n\n\n");
    ui_draw_status_bar("B=Back");
}

static void handle_main_menu_input(u64 key) {
    if (key & KEY_DOWN) {
        g_selected_idx++;
        if (g_selected_idx > 4) g_selected_idx = 0;
    }
    if (key & KEY_UP) {
        g_selected_idx--;
        if (g_selected_idx < 0) g_selected_idx = 4;
    }
    if (key & KEY_PLUS) {
        g_exit = true;
        return;
    }
    if (key & KEY_A) {
        switch (g_selected_idx) {
            case 0:
                if (g_saved_servers_count > 0) {
                    strncpy(g_server_url, g_saved_servers[0].url, sizeof(g_server_url) - 1);
                }
                g_state = STATE_FILE_BROWSER;
                g_selected_idx = 0;
                g_scroll_offset = 0;
                http_list_files(g_server_url, "/", &g_file_list);
                break;
            case 1:
                g_state = STATE_SERVER_LIST;
                g_selected_server = 0;
                break;
            case 2:
                g_state = STATE_ADD_SERVER;
                break;
            case 3:
                break;
            case 4:
                g_state = STATE_ABOUT;
                break;
        }
    }
}

static void handle_server_list_input(u64 key) {
    if (key & KEY_DOWN) {
        g_selected_server++;
        if (g_selected_server >= g_saved_servers_count) g_selected_server = 0;
    }
    if (key & KEY_UP) {
        g_selected_server--;
        if (g_selected_server < 0) g_selected_server = g_saved_servers_count - 1;
    }
    if (key & KEY_A && g_saved_servers_count > 0) {
        strncpy(g_server_url, g_saved_servers[g_selected_server].url, sizeof(g_server_url) - 1);
        g_state = STATE_FILE_BROWSER;
        g_selected_idx = 0;
        g_scroll_offset = 0;
        http_list_files(g_server_url, "/", &g_file_list);
    }
    if (key & KEY_B) {
        g_state = STATE_MAIN_MENU;
        g_selected_idx = 1;
    }
}

static void handle_file_browser_input(u64 key) {
    if (key & KEY_DOWN) {
        if (g_selected_idx < g_file_list.count - 1) {
            g_selected_idx++;
            if (g_selected_idx >= g_scroll_offset + 25) {
                g_scroll_offset = g_selected_idx - 24;
            }
        }
    }
    if (key & KEY_UP) {
        if (g_selected_idx > 0) {
            g_selected_idx--;
            if (g_selected_idx < g_scroll_offset) {
                g_scroll_offset = g_selected_idx;
            }
        }
    }
    if (key & KEY_R) {
        g_selected_idx += 10;
        if (g_selected_idx >= g_file_list.count) g_selected_idx = g_file_list.count - 1;
        g_scroll_offset = g_selected_idx;
    }
    if (key & KEY_L) {
        g_selected_idx -= 10;
        if (g_selected_idx < 0) g_selected_idx = 0;
        g_scroll_offset = g_selected_idx;
    }
    if (key & KEY_Y) {
        http_free_file_list(&g_file_list);
        http_list_files(g_server_url, "/", &g_file_list);
        g_selected_idx = 0;
        g_scroll_offset = 0;
    }
    if (key & KEY_A && g_file_list.count > 0) {
        HttpFileEntry* entry = &g_file_list.files[g_selected_idx];
        if (entry->is_dir) {
            char new_path[512];
            snprintf(new_path, sizeof(new_path), "/%s", entry->name);
            http_free_file_list(&g_file_list);
            http_list_files(g_server_url, new_path, &g_file_list);
            g_selected_idx = 0;
            g_scroll_offset = 0;
        } else {
            const char* ext = strrchr(entry->name, '.');
            if (ext && (strcasecmp(ext, ".nsp") == 0 || strcasecmp(ext, ".nsz") == 0)) {
                char file_url[1024];
                snprintf(file_url, sizeof(file_url), "%s/%s", g_server_url, entry->name);

                install_init(&g_install_task);
                install_start_nsp_http(&g_install_task, file_url, entry->name, entry->size);
                g_state = STATE_INSTALL_PROGRESS;
            }
        }
    }
    if (key & KEY_B) {
        http_free_file_list(&g_file_list);
        g_state = STATE_MAIN_MENU;
        g_selected_idx = 0;
    }
}

static void handle_install_progress_input(u64 key) {
    install_update(&g_install_task);

    if (key & KEY_B) {
        if (g_install_task.status == INSTALL_STATUS_DONE ||
            g_install_task.status == INSTALL_STATUS_ERROR) {
            install_cleanup(&g_install_task);
            g_state = STATE_FILE_BROWSER;
        }
    }
}

static void handle_add_server_input(u64 key) {
    if (key & KEY_A) {
        char input[512] = {0};
        strncpy(input, g_server_url, sizeof(input) - 1);

        SwkbdConfig kbd;
        if (R_SUCCEEDED(swkbdCreate(&kbd, 0))) {
            swkbdConfigMakePresetDefault(&kbd);
            swkbdConfigSetInitialText(&kbd, input);
            swkbdConfigSetStringLenMax(&kbd, 511);
            swkbdConfigSetHeaderText(&kbd, "Enter HTTP Server URL");
            swkbdConfigSetOkButtonText(&kbd, "Connect");
            if (R_SUCCEEDED(swkbdShow(&kbd, input, sizeof(input)))) {
                strncpy(g_server_url, input, sizeof(g_server_url) - 1);
            }
            swkbdClose(&kbd);
        }
    }
    if (key & KEY_X) {
        if (g_saved_servers_count < MAX_SAVED_SERVERS) {
            strncpy(g_saved_servers[g_saved_servers_count].name, "HTTP Server",
                    sizeof(g_saved_servers[g_saved_servers_count].name) - 1);
            strncpy(g_saved_servers[g_saved_servers_count].url, g_server_url,
                    sizeof(g_saved_servers[g_saved_servers_count].url) - 1);
            g_saved_servers[g_saved_servers_count].type = SERVER_HTTP;
            g_saved_servers_count++;
            config_save_servers(g_saved_servers, g_saved_servers_count);
        }
        g_state = STATE_MAIN_MENU;
        g_selected_idx = 1;
    }
    if (key & KEY_B) {
        g_state = STATE_MAIN_MENU;
        g_selected_idx = 2;
    }
}

static void handle_about_input(u64 key) {
    if (key & KEY_B) {
        g_state = STATE_MAIN_MENU;
        g_selected_idx = 4;
    }
}

int main(int argc, char* argv[]) {
    init();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 key = padGetButtonsDown(&pad);

        if (g_exit) break;
        if (key & KEY_PLUS) break;

        switch (g_state) {
            case STATE_MAIN_MENU:
                handle_main_menu_input(key);
                draw_main_menu();
                break;
            case STATE_SERVER_LIST:
                handle_server_list_input(key);
                draw_server_list();
                break;
            case STATE_FILE_BROWSER:
                handle_file_browser_input(key);
                draw_file_browser();
                break;
            case STATE_INSTALL_PROGRESS:
                handle_install_progress_input(key);
                draw_install_progress();
                break;
            case STATE_ADD_SERVER:
                handle_add_server_input(key);
                draw_add_server();
                break;
            case STATE_ABOUT:
                handle_about_input(key);
                draw_about();
                break;
            default:
                break;
        }

        consoleUpdate(NULL);
    }

    cleanup();
    return 0;
}

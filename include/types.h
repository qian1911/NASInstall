#ifndef TYPES_H
#define TYPES_H

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720
#define MAX_URL_LEN   512
#define MAX_SERVERS   20
#define MAX_FILES     256
#define MAX_TEXT_LEN  256

typedef enum {
    STATE_SCAN = 0,
    STATE_SERVER_LIST,
    STATE_FILE_BROWSER,
    STATE_INSTALL_PROGRESS,
    STATE_ADD_SERVER,
    STATE_SETTINGS,
    STATE_ABOUT
} AppState;

typedef enum {
    SERVER_HTTP = 0,
    SERVER_SMB,
    SERVER_FTP,
    SERVER_NFS
} ServerType;

typedef struct {
    char name[MAX_TEXT_LEN];
    char url[MAX_URL_LEN];
    char ip[64];
    int port;
    ServerType type;
    bool online;
} SavedServer;

typedef struct {
    char name[256];
    u64 size;
    bool is_dir;
} FileEntry;

typedef enum {
    INSTALL_IDLE = 0,
    INSTALL_DOWNLOADING,
    INSTALLING,
    INSTALL_DONE,
    INSTALL_ERROR
} InstallStatus;

typedef struct {
    InstallStatus status;
    float progress;
    u64 speed;
    u64 total_size;
    u64 downloaded;
    char filename[256];
    char error_msg[256];
} InstallTask;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* font_small;
    TTF_Font* font_large;
    PadState pad;
    bool needs_redraw;
} UIContext;

typedef struct {
    AppState state;
    AppState prev_state;
    UIContext ui;
    SavedServer servers[MAX_SERVERS];
    int server_count;
    int selected_server;
    FileEntry files[MAX_FILES];
    int file_count;
    int selected_file;
    int scroll_offset;
    InstallTask install;
    char current_url[MAX_URL_LEN];
    char current_path[MAX_URL_LEN];
    bool exit_app;
    bool scanning;
    int scan_progress;
} AppContext;

#endif

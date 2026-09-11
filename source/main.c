#include "types.h"
#include "ui.h"
#include "discover.h"
#include "http.h"
#include "install.h"
#include "config.h"
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define APP_TITLE "NASInstall"
#define APP_VERSION "2.0.0"

static void draw_scan(AppContext* app) {
    UIContext* ctx = &app->ui;
    ui_clear(ctx, COL_BG);
    ui_top_bar(ctx, "Scanning Network", "Searching for NAS devices...");

    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2 - 40;

    // Animated scan circle
    int r = 40 + (app->scan_progress % 30);
    set_color(ctx, COL_ACCENT);
    for (int i = 0; i < 360; i += 5) {
        float rad = i * 3.14159f / 180.0f;
        int x = cx + (int)(r * cosf(rad));
        int y = cy + (int)(r * sinf(rad));
        SDL_Rect dot = {x - 2, y - 2, 4, 4};
        SDL_RenderFillRect(ctx->renderer, &dot);
    }

    ui_draw_text_centered(ctx, "Scanning local network...", 0, cy + 80, SCREEN_WIDTH, 18, COL_TEXT_DIM);
    char progress[64];
    snprintf(progress, sizeof(progress), "%d / 254 IPs checked", app->scan_progress);
    ui_draw_text_centered(ctx, progress, 0, cy + 110, SCREEN_WIDTH, 14, COL_TEXT_DIM);

    // Progress bar
    UIRect pb = {cx - 200, cy + 150, 400, 8};
    ui_progress_bar(ctx, pb, (float)app->scan_progress / 254.0f);

    ui_bottom_bar(ctx, "B=Cancel");
    ui_present(ctx);
}

static void draw_server_list(AppContext* app) {
    UIContext* ctx = &app->ui;
    ui_clear(ctx, COL_BG);
    ui_top_bar(ctx, "NAS Servers", app->server_count > 0 ? "" : "No servers found");

    int y_start = TOP_BAR_H + 20;
    int card_w = SCREEN_WIDTH - 2 * PADDING;
    int card_h = 80;
    int visible = (SCREEN_HEIGHT - BOT_BAR_H - y_start - 40) / (card_h + 10);

    if (app->server_count == 0) {
        ui_draw_text_centered(ctx, "No NAS devices found", 0, SCREEN_HEIGHT / 2 - 20, SCREEN_WIDTH, 24, COL_TEXT_DIM);
        ui_draw_text_centered(ctx, "Press Y to scan network again", 0, SCREEN_HEIGHT / 2 + 20, SCREEN_WIDTH, 18, COL_TEXT_DIM);
    } else {
        for (int i = 0; i < visible && i + app->scroll_offset < app->server_count; i++) {
            int idx = i + app->scroll_offset;
            SavedServer* s = &app->servers[idx];
            bool sel = (idx == app->selected_server);
            char subtitle[128];
            snprintf(subtitle, sizeof(subtitle), "%s  Port: %d", s->ip, s->port);
            ui_card(ctx, (UIRect){PADDING, y_start + i * (card_h + 10), card_w, card_h},
                    s->name, subtitle, s->online ? "Online" : NULL, sel);
        }
    }

    ui_bottom_bar(ctx, "A=Connect  Y=Scan  X=Add Manually  B=Back");
    ui_present(ctx);
}

static void draw_file_browser(AppContext* app) {
    UIContext* ctx = &app->ui;
    ui_clear(ctx, COL_BG);
    ui_top_bar(ctx, "File Browser", app->current_url);

    int y_start = TOP_BAR_H + 20;
    int card_w = SCREEN_WIDTH - 2 * PADDING;
    int card_h = 60;
    int visible = (SCREEN_HEIGHT - BOT_BAR_H - y_start - 10) / (card_h + 6);

    if (app->file_count == 0) {
        ui_draw_text_centered(ctx, "Loading or no files found", 0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, 18, COL_TEXT_DIM);
    } else {
        for (int i = 0; i < visible && i + app->scroll_offset < app->file_count; i++) {
            int idx = i + app->scroll_offset;
            FileEntry* e = &app->files[idx];
            bool sel = (idx == app->selected_file);

            u32 bg = sel ? COL_CARD_HL : COL_CARD;
            if (sel) ui_fill_rounded_rect(ctx, PADDING - 2, y_start + i * (card_h + 6) - 2, card_w + 4, card_h + 4, 12, COL_ACCENT_D);
            ui_fill_rounded_rect(ctx, PADDING, y_start + i * (card_h + 6), card_w, card_h, 10, bg);

            // File icon
            set_color(ctx, e->is_dir ? COL_WARNING : COL_ACCENT);
            SDL_Rect icon = {PADDING + 16, y_start + i * (card_h + 6) + 20, 24, 24};
            SDL_RenderFillRect(ctx->renderer, &icon);

            // Name
            char display[300];
            const char* prefix = e->is_dir ? "[DIR]  " : "[FILE] ";
            snprintf(display, sizeof(display), "%s%s", prefix, e->name);
            ui_draw_text(ctx, display, PADDING + 52, y_start + i * (card_h + 6) + 18, 18, COL_TEXT);

            if (!e->is_dir) {
                char sz[32];
                format_size(e->size, sz, sizeof(sz));
                ui_draw_text(ctx, sz, PADDING + 52, y_start + i * (card_h + 6) + 40, 14, COL_TEXT_DIM);
            }
        }

        // Scroll indicator
        if (app->file_count > visible) {
            char info[64];
            snprintf(info, sizeof(info), "%d / %d files", app->selected_file + 1, app->file_count);
            ui_draw_text(ctx, info, SCREEN_WIDTH - 150, SCREEN_HEIGHT - BOT_BAR_H - 30, 14, COL_TEXT_DIM);
        }
    }

    ui_bottom_bar(ctx, "A=Open/Install  B=Back  Y=Refresh  L/R=Page");
    ui_present(ctx);
}

static void draw_install(AppContext* app) {
    UIContext* ctx = &app->ui;
    ui_clear(ctx, COL_BG);
    ui_top_bar(ctx, "Installing", app->install.filename);

    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2 - 40;

    // Status text
    const char* stage = "Idle";
    u32 stage_color = COL_TEXT_DIM;
    switch (app->install.status) {
        case INSTALL_DOWNLOADING: stage = "Downloading..."; stage_color = COL_ACCENT; break;
        case INSTALLING: stage = "Installing..."; stage_color = COL_WARNING; break;
        case INSTALL_DONE: stage = "Complete!"; stage_color = COL_SUCCESS; break;
        case INSTALL_ERROR: stage = "Error"; stage_color = COL_ERROR; break;
    }
    ui_draw_text_centered(ctx, stage, 0, cy - 60, SCREEN_WIDTH, 28, stage_color);

    // Progress bar
    UIRect pb = {cx - 250, cy, 500, 20};
    ui_progress_bar(ctx, pb, app->install.progress);

    char pct[32];
    snprintf(pct, sizeof(pct), "%.1f%%", app->install.progress * 100);
    ui_draw_text_centered(ctx, pct, 0, cy + 30, SCREEN_WIDTH, 24, COL_TEXT);

    char sz_str[64], total_str[64];
    format_size(app->install.speed, sz_str, sizeof(sz_str));
    format_size(app->install.total_size, total_str, sizeof(total_str));
    char info[256];
    snprintf(info, sizeof(info), "Speed: %s/s    Total: %s", sz_str, total_str);
    ui_draw_text_centered(ctx, info, 0, cy + 80, SCREEN_WIDTH, 14, COL_TEXT_DIM);

    ui_bottom_bar(ctx, app->install.status == INSTALL_DONE || app->install.status == INSTALL_ERROR ? "B=Back" : "Please wait...");
    ui_present(ctx);
}

static void handle_scan(AppContext* app, u64 key) {
    if (key & HidNpadButton_B) {
        app->state = STATE_SERVER_LIST;
    }
}

static void handle_server_list(AppContext* app, u64 key) {
    if (key & HidNpadButton_Y) {
        app->state = STATE_SCAN;
        app->scan_progress = 0;
        return;
    }
    if (key & HidNpadButton_X) {
        app->state = STATE_ADD_SERVER;
        return;
    }
    if (app->server_count == 0) {
        if (key & HidNpadButton_B) app->exit_app = true;
        return;
    }
    if (key & HidNpadButton_Down) {
        if (app->selected_server < app->server_count - 1) {
            app->selected_server++;
            if (app->selected_server >= app->scroll_offset + 6)
                app->scroll_offset = app->selected_server - 5;
        }
    }
    if (key & HidNpadButton_Up) {
        if (app->selected_server > 0) {
            app->selected_server--;
            if (app->selected_server < app->scroll_offset)
                app->scroll_offset = app->selected_server;
        }
    }
    if (key & HidNpadButton_A) {
        strncpy(app->current_url, app->servers[app->selected_server].url, MAX_URL_LEN - 1);
        strcpy(app->current_path, "/");
        app->file_count = 0;
        http_list_files(app->current_url, "/", app->files, &app->file_count, MAX_FILES);
        app->selected_file = 0;
        app->scroll_offset = 0;
        app->state = STATE_FILE_BROWSER;
    }
    if (key & HidNpadButton_B) app->exit_app = true;
}

static void handle_file_browser(AppContext* app, u64 key) {
    if (app->file_count == 0) {
        if (key & HidNpadButton_B) { app->state = STATE_SERVER_LIST; app->selected_server = 0; }
        return;
    }
    if (key & HidNpadButton_Down) {
        if (app->selected_file < app->file_count - 1) {
            app->selected_file++;
            if (app->selected_file >= app->scroll_offset + 8)
                app->scroll_offset = app->selected_file - 7;
        }
    }
    if (key & HidNpadButton_Up) {
        if (app->selected_file > 0) {
            app->selected_file--;
            if (app->selected_file < app->scroll_offset)
                app->scroll_offset = app->selected_file;
        }
    }
    if (key & HidNpadButton_R) {
        app->selected_file += 8;
        if (app->selected_file >= app->file_count) app->selected_file = app->file_count - 1;
        app->scroll_offset = app->selected_file;
    }
    if (key & HidNpadButton_L) {
        app->selected_file -= 8;
        if (app->selected_file < 0) app->selected_file = 0;
        app->scroll_offset = app->selected_file;
    }
    if (key & HidNpadButton_Y) {
        app->file_count = 0;
        http_list_files(app->current_url, app->current_path, app->files, &app->file_count, MAX_FILES);
        app->selected_file = 0;
        app->scroll_offset = 0;
    }
    if (key & HidNpadButton_A) {
        FileEntry* e = &app->files[app->selected_file];
        if (e->is_dir) {
            snprintf(app->current_path, MAX_URL_LEN, "%s%s/", app->current_path, e->name);
            app->file_count = 0;
            http_list_files(app->current_url, app->current_path, app->files, &app->file_count, MAX_FILES);
            app->selected_file = 0;
            app->scroll_offset = 0;
        } else {
            const char* ext = strrchr(e->name, '.');
            if (ext && (strcasecmp(ext, ".nsp") == 0 || strcasecmp(ext, ".nsz") == 0)) {
                char url[1024];
                snprintf(url, sizeof(url), "%s%s%s", app->current_url, app->current_path, e->name);
                install_init(&app->install);
                install_start(&app->install, url, e->name, e->size);
                app->state = STATE_INSTALL_PROGRESS;
            }
        }
    }
    if (key & HidNpadButton_B) {
        app->state = STATE_SERVER_LIST;
        app->selected_server = 0;
        app->scroll_offset = 0;
    }
}

static void handle_install(AppContext* app, u64 key) {
    install_update(&app->install);
    if (key & HidNpadButton_B) {
        if (app->install.status == INSTALL_DONE || app->install.status == INSTALL_ERROR) {
            install_cleanup(&app->install);
            app->state = STATE_FILE_BROWSER;
        }
    }
}

int main(int argc, char* argv[]) {
    AppContext app;
    memset(&app, 0, sizeof(app));

    if (!ui_init(&app.ui)) {
        consoleInit(NULL);
        printf("UI init failed!\n");
        consoleUpdate(NULL);
        svcSleepThread(3000000000);
        consoleExit(NULL);
        return 1;
    }

    socketInitializeDefault();
    nsInitialize();
    ncmInitialize();
    psmInitialize();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    config_load(app.servers, &app.server_count, MAX_SERVERS);
    app.state = STATE_SERVER_LIST;
    app.selected_server = 0;
    app.scroll_offset = 0;

    // Auto-scan on first launch if no saved servers
    if (app.server_count == 0) {
        app.state = STATE_SCAN;
    }

    while (appletMainLoop() && !app.exit_app) {
        u64 key = pad_get_keys(&app.ui.pad);

        if (key & HidNpadButton_Plus) {
            if (app.state == STATE_SERVER_LIST || app.state == STATE_SCAN) {
                app.exit_app = true;
                break;
            }
        }

        // Handle scan state (blocking scan)
        if (app.state == STATE_SCAN) {
            // Draw scanning animation
            app.scan_progress = (app.scan_progress + 1) % 255;
            draw_scan(&app);

            // Do actual scan after a few frames of animation
            if (app.scan_progress == 254) {
                app.server_count = discover_scan(app.servers, MAX_SERVERS);
                // Also load saved servers
                SavedServer saved[MAX_SERVERS];
                int saved_count = 0;
                config_load(saved, &saved_count, MAX_SERVERS);
                for (int i = 0; i < saved_count && app.server_count < MAX_SERVERS; i++) {
                    bool found = false;
                    for (int j = 0; j < app.server_count; j++) {
                        if (strcmp(app.servers[j].url, saved[i].url) == 0) { found = true; break; }
                    }
                    if (!found) app.servers[app.server_count++] = saved[i];
                }
                app.state = STATE_SERVER_LIST;
                app.selected_server = 0;
                app.scroll_offset = 0;
            }

            if (key & HidNpadButton_B) {
                app.state = STATE_SERVER_LIST;
            }
            svcSleepThread(10000000); // 10ms
            continue;
        }

        switch (app.state) {
            case STATE_SERVER_LIST:
                handle_server_list(&app, key);
                draw_server_list(&app);
                break;
            case STATE_FILE_BROWSER:
                handle_file_browser(&app, key);
                draw_file_browser(&app);
                break;
            case STATE_INSTALL_PROGRESS:
                handle_install(&app, key);
                draw_install(&app);
                break;
            default:
                app.state = STATE_SERVER_LIST;
                break;
        }
    }

    config_save(app.servers, app.server_count);
    curl_global_cleanup();
    psmExit();
    ncmExit();
    nsExit();
    socketExit();
    ui_exit(&app.ui);
    return 0;
}

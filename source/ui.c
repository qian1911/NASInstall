#include "ui.h"
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

void set_color(UIContext* ctx, u32 c) {
    SDL_SetRenderDrawColor(ctx->renderer, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, (c >> 24) & 0xFF ? (c >> 24) & 0xFF : 0xFF);
}

bool ui_init(UIContext* ctx) {
    memset(ctx, 0, sizeof(UIContext));
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) return false;
    if (TTF_Init() < 0) return false;
    romfsInit();
    plInitialize(PlServiceType_User);

    if (SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_FULLSCREEN, &ctx->window, &ctx->renderer) < 0)
        return false;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    PlFontData font_data;
    if (R_FAILED(plGetSharedFontByType(&font_data, PlSharedFontType_Standard))) return false;

    ctx->font = TTF_OpenFontRW(SDL_RWFromMem(font_data.address, font_data.size), 0, 18);
    ctx->font_small = TTF_OpenFontRW(SDL_RWFromMem(font_data.address, font_data.size), 0, 14);
    ctx->font_large = TTF_OpenFontRW(SDL_RWFromMem(font_data.address, font_data.size), 0, 24);
    if (!ctx->font || !ctx->font_small || !ctx->font_large) return false;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&ctx->pad);
    ctx->needs_redraw = true;
    return true;
}

void ui_exit(UIContext* ctx) {
    if (ctx->font) TTF_CloseFont(ctx->font);
    if (ctx->font_small) TTF_CloseFont(ctx->font_small);
    if (ctx->font_large) TTF_CloseFont(ctx->font_large);
    plExit();
    romfsExit();
    TTF_Quit();
    SDL_Quit();
}

void ui_clear(UIContext* ctx, u32 color) {
    set_color(ctx, color);
    SDL_RenderClear(ctx->renderer);
}

void ui_fill_rect(UIContext* ctx, int x, int y, int w, int h, u32 color) {
    set_color(ctx, color);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(ctx->renderer, &r);
}

void ui_fill_rounded_rect(UIContext* ctx, int x, int y, int w, int h, int r, u32 color) {
    set_color(ctx, color);
    SDL_Rect rects[] = {
        {x + r, y, w - 2*r, r},
        {x + r, y + h - r, w - 2*r, r},
        {x, y + r, w, h - 2*r},
        {x, y + r, r, h - 2*r},
        {x + w - r, y + r, r, h - 2*r},
    };
    SDL_RenderFillRects(ctx->renderer, rects, 5);
    for (int dy = 0; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r*r - dy*dy));
        SDL_RenderDrawLines(ctx->renderer, (SDL_Point[]){
            {x + r - dx, y + r - dy}, {x + r + dx, y + r - dy}
        }, 2);
        SDL_RenderDrawLines(ctx->renderer, (SDL_Point[]){
            {x + r - dx, y + h - r + dy}, {x + r + dx, y + h - r + dy}
        }, 2);
    }
}

static TTF_Font* get_font(UIContext* ctx, int size) {
    if (size <= 14) return ctx->font_small;
    if (size >= 24) return ctx->font_large;
    return ctx->font;
}

void ui_draw_text(UIContext* ctx, const char* text, int x, int y, int size, u32 color) {
    if (!text || !text[0]) return;
    TTF_Font* font = get_font(ctx, size);
    if (!font) return;
    SDL_Color c = {(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 0xFF};
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text, c);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ctx->renderer, s);
    if (t) {
        SDL_Rect d = {x, y, s->w, s->h};
        SDL_RenderCopy(ctx->renderer, t, NULL, &d);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

void ui_draw_text_centered(UIContext* ctx, const char* text, int x, int y, int w, int size, u32 color) {
    TTF_Font* font = get_font(ctx, size);
    if (!font || !text) return;
    int tw, th;
    TTF_SizeUTF8(font, text, &tw, &th);
    ui_draw_text(ctx, text, x + (w - tw) / 2, y, size, color);
}

void ui_draw_text_wrapped(UIContext* ctx, const char* text, int x, int y, int max_w, int size, u32 color) {
    TTF_Font* font = get_font(ctx, size);
    if (!font || !text) return;
    int line_h = TTF_FontHeight(font) + 2;
    char buf[1024];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* line = strtok(buf, "\n");
    int yy = y;
    while (line) {
        int tw, th;
        TTF_SizeUTF8(font, line, &tw, &th);
        if (tw <= max_w) {
            ui_draw_text(ctx, line, x, yy, size, color);
            yy += line_h;
        } else {
            char current[768] = "";
            char* tok = strtok(line, " ");
            while (tok) {
                char test[1024];
                snprintf(test, sizeof(test), "%s %s", current, tok);
                TTF_SizeUTF8(font, test, &tw, &th);
                if (tw > max_w && current[0]) {
                    ui_draw_text(ctx, current, x, yy, size, color);
                    yy += line_h;
                    strncpy(current, tok, sizeof(current) - 1);
                } else {
                    if (current[0]) strncat(current, " ", sizeof(current) - strlen(current) - 1);
                    strncat(current, tok, sizeof(current) - strlen(current) - 1);
                }
                tok = strtok(NULL, " ");
            }
            if (current[0]) { ui_draw_text(ctx, current, x, yy, size, color); yy += line_h; }
        }
        line = strtok(NULL, "\n");
    }
}

int ui_text_width(UIContext* ctx, const char* text, int size) {
    TTF_Font* font = get_font(ctx, size);
    if (!font || !text) return 0;
    int w, h;
    TTF_SizeUTF8(font, text, &w, &h);
    return w;
}

void ui_present(UIContext* ctx) { SDL_RenderPresent(ctx->renderer); }

bool ui_button(UIContext* ctx, UIRect r, const char* text, bool selected) {
    u32 bg = selected ? COL_ACCENT : COL_CARD;
    if (selected) ui_fill_rounded_rect(ctx, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 12, COL_ACCENT_D);
    ui_fill_rounded_rect(ctx, r.x, r.y, r.w, r.h, 10, bg);
    ui_draw_text_centered(ctx, text, r.x, r.y + (r.h - 18) / 2, r.w, 18, COL_TEXT);
    return selected;
}

bool ui_card(UIContext* ctx, UIRect r, const char* title, const char* subtitle, const char* status, bool selected) {
    u32 bg = selected ? COL_CARD_HL : COL_CARD;
    if (selected) ui_fill_rounded_rect(ctx, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 14, COL_ACCENT_D);
    ui_fill_rounded_rect(ctx, r.x, r.y, r.w, r.h, 12, bg);

    // Server icon
    int icon_r = 18;
    int cx = r.x + 30 + icon_r;
    int cy = r.y + r.h / 2;
    set_color(ctx, COL_ACCENT);
    SDL_Rect icon = {cx - icon_r, cy - icon_r, icon_r * 2, icon_r * 2};
    SDL_RenderFillRect(ctx->renderer, &icon);
    // NAS icon (simple server stack)
    set_color(ctx, COL_TEXT);
    SDL_RenderDrawRect(ctx->renderer, &(SDL_Rect){cx - 8, cy - 6, 16, 5});
    SDL_RenderDrawRect(ctx->renderer, &(SDL_Rect){cx - 8, cy - 1, 16, 5});
    SDL_RenderDrawRect(ctx->renderer, &(SDL_Rect){cx - 8, cy + 4, 16, 5});

    ui_draw_text(ctx, title, r.x + 66, r.y + 14, 18, COL_TEXT);
    if (subtitle && subtitle[0])
        ui_draw_text(ctx, subtitle, r.x + 66, r.y + 40, 14, COL_TEXT_DIM);

    // Status badge
    if (status && status[0]) {
        int sw = ui_text_width(ctx, status, 14);
        ui_fill_rounded_rect(ctx, r.x + r.w - sw - 40, r.y + r.h / 2 - 14, sw + 24, 28, 14, COL_SUCCESS);
        ui_draw_text(ctx, status, r.x + r.w - sw - 28, r.y + r.h / 2 - 7, 14, COL_TEXT);
    }
    return selected;
}

void ui_progress_bar(UIContext* ctx, UIRect r, float progress) {
    ui_fill_rounded_rect(ctx, r.x, r.y, r.w, r.h, 4, COL_CARD);
    int fill_w = (int)(r.w * progress);
    if (fill_w > 0) ui_fill_rounded_rect(ctx, r.x, r.y, fill_w, r.h, 4, COL_ACCENT);
}

void ui_top_bar(UIContext* ctx, const char* title, const char* subtitle) {
    ui_fill_rect(ctx, 0, 0, SCREEN_WIDTH, TOP_BAR_H, COL_BG);
    ui_fill_rect(ctx, 0, TOP_BAR_H - 2, SCREEN_WIDTH, 2, COL_ACCENT);
    ui_draw_text(ctx, title, PADDING, (TOP_BAR_H - 24) / 2, 24, COL_TEXT);
    if (subtitle && subtitle[0])
        ui_draw_text(ctx, subtitle, PADDING + ui_text_width(ctx, title, 24) + 20, (TOP_BAR_H - 18) / 2 + 4, 14, COL_TEXT_DIM);
}

void ui_bottom_bar(UIContext* ctx, const char* text) {
    int y = SCREEN_HEIGHT - BOT_BAR_H;
    ui_fill_rect(ctx, 0, y, SCREEN_WIDTH, BOT_BAR_H, COL_CARD);
    ui_fill_rect(ctx, 0, y, SCREEN_WIDTH, 1, COL_CARD_HL);
    ui_draw_text(ctx, text, PADDING, y + (BOT_BAR_H - 14) / 2, 14, COL_TEXT_DIM);
}

void ui_status_badge(UIContext* ctx, int x, int y, bool online) {
    set_color(ctx, online ? COL_SUCCESS : COL_TEXT_DIM);
    SDL_Rect dot = {x, y, 8, 8};
    SDL_RenderFillRect(ctx->renderer, &dot);
}

bool rect_contains(UIRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

u64 pad_get_keys(PadState* pad) {
    padUpdate(pad);
    return padGetButtonsDown(pad);
}

void format_size(u64 size, char* out, size_t out_size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double s = (double)size;
    while (s >= 1024.0 && u < 4) { s /= 1024.0; u++; }
    if (u == 0) snprintf(out, out_size, "%.0f %s", s, units[u]);
    else snprintf(out, out_size, "%.2f %s", s, units[u]);
}

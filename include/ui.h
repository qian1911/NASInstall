#ifndef UI_H
#define UI_H

#include <switch.h>

// Draw a header bar with title and subtitle
void ui_draw_header(const char* title, const char* subtitle);

// Draw a menu item with selection highlight
void ui_draw_menu_item(int row, int index, bool selected, const char* text);

// Draw status bar at the bottom
void ui_draw_status_bar(const char* text);

// Format file size to human-readable string
void format_size(u64 size, char* out, size_t out_size);

// Helper for variable argument string formatting (simple version)
#define va(fmt, ...) ({ char _buf[256]; snprintf(_buf, sizeof(_buf), fmt, ##__VA_ARGS__); _buf; })

#endif // UI_H

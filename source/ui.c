#include "ui.h"
#include <stdio.h>
#include <string.h>

extern int consoleGetDefaultWidth(void);

void ui_draw_header(const char* title, const char* subtitle) {
    int width = 80; // default console width

    printf("\x1b[1;1H");  // move to top-left
    printf("\x1b[47m\x1b[30m");  // white bg, black text

    // Title line
    printf("  %-*s", width - 2, "");
    printf("\x1b[2;1H");
    printf("  %s", title);
    int title_len = strlen(title);
    printf("%*s", width - 2 - title_len, "");

    printf("\x1b[0m"); // reset

    // Subtitle
    if (subtitle && subtitle[0]) {
        printf("\x1b[3;1H");
        printf("  \x1b[36m%s\x1b[0m\n", subtitle);
    }

    printf("\x1b[4;1H");
    printf("  %s\n", "──────────────────────────────────────────────────────────────────────────────");
}

void ui_draw_menu_item(int row, int index, bool selected, const char* text) {
    printf("\x1b[%d;1H", row + 4); // offset by 4 for header

    if (selected) {
        printf("  \x1b[46m\x1b[37m> %-*s\x1b[0m", 76, text);
    } else {
        printf("    %-*s", 76, text);
    }
}

void ui_draw_status_bar(const char* text) {
    int width = 80;
    printf("\x1b[35;1H"); // row 35
    printf("\x1b[47m\x1b[30m"); // white bg, black text
    printf("  %-*s", width - 2, text);
    printf("\x1b[0m");
}

void format_size(u64 size, char* out, size_t out_size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double s = (double)size;

    while (s >= 1024.0 && unit < 4) {
        s /= 1024.0;
        unit++;
    }

    if (unit == 0) {
        snprintf(out, out_size, "%.0f %s", s, units[unit]);
    } else {
        snprintf(out, out_size, "%.2f %s", s, units[unit]);
    }
}

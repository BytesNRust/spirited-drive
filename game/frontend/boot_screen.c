/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see boot_screen.h. */
#include "game/frontend/boot_screen.h"
#include "game/sys/game_name.h"

#include <stdio.h>

static void put(BootLine *out, int *n, BootSize size, int gap, const char *text)
{
    if (*n >= BOOT_LINES_MAX) return;
    snprintf(out[*n].text, sizeof out[*n].text, "%s", text);
    out[*n].size = size, out[*n].gap = gap;
    (*n)++;
}

int boot_screen_lines(const char *version, BootLine out[BOOT_LINES_MAX])
{
    int n = 0;
    put(out, &n, BOOT_HEADLINE, 0, "Bytes & Rust presents");
    put(out, &n, BOOT_TITLE, 12, GAME_NAME);
    put(out, &n, BOOT_SMALL, 40, "An unofficial restomod of The Need for Speed SE for PC (1996)");
    put(out, &n, BOOT_SMALL, 28, "(c) 2026 Bytes & Rust - GPL-3.0");
    put(out, &n, BOOT_SMALL, 6, "Original game (c) 1995-96 Electronic Arts. Not affiliated with EA");
    if (version != NULL && version[0] != '\0') {
        char line[BOOT_LINE_CHARS];
        snprintf(line, sizeof line, "v%s", version);
        put(out, &n, BOOT_SMALL, 28, line);
    }
    return n;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see boot_screen.h. */
#include "game/frontend/boot_screen.h"

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
    put(out, &n, BOOT_TITLE, 12, "TNFS Restomod");
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

int credits_extra_lines(int page, const char *version, BootLine out[BOOT_LINES_MAX])
{
    int n = 0;
    if (page == 0) {   /* the dedication: the words are a draft until the owner's own */
        put(out, &n, BOOT_HEADLINE, 0, "This is their game.");
        put(out, &n, BOOT_TITLE, 12, "We restored it.");
        put(out, &n, BOOT_SMALL, 44, "For the people at EA Canada and Pioneer Productions who made it");
        put(out, &n, BOOT_SMALL, 6, "and for everyone who still plays it");
    } else if (page == 1) {
        put(out, &n, BOOT_HEADLINE, 0, "TNFS Restomod");
        put(out, &n, BOOT_SMALL, 28, "Original game: EA Canada with Road & Track 1994-1996");
        put(out, &n, BOOT_SMALL, 6, "Restomod: Bytes & Rust and contributors");
        put(out, &n, BOOT_SMALL, 20, "Code: GPL-3.0-or-later   Our data: CC BY-SA 4.0");
        put(out, &n, BOOT_SMALL, 6, "Made with SDL3");
        put(out, &n, BOOT_SMALL, 20, "An unofficial project. Not affiliated with EA");
        if (version != NULL && version[0] != '\0') {
            char line[BOOT_LINE_CHARS];
            snprintf(line, sizeof line, "v%s", version);
            put(out, &n, BOOT_SMALL, 20, line);
        }
    }
    return n;
}

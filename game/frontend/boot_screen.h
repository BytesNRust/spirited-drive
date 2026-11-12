/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: our own boot screen, shown once after EA's intro and title movies: who presents it, the game's banner, what it is and whose game it is, over
 * a red relief in the style of TITLE.QFS (boot_relief.h), as the owner approved it (i030). The words and their places (this file's layout, pure) and the
 * drawing (boot_screen_glue.c) are apart so the words can be tested. */
#ifndef TNFS_BOOT_SCREEN_H
#define TNFS_BOOT_SCREEN_H

#include "game/frontend/screen_hold.h"

enum {
    BOOT_LINES_MAX = 8,
    BOOT_LINE_CHARS = 96,
    /* the banner (data/logo/spirited_logo.png, RGBA, drawn 1:1): where it goes and the size it must have */
    BOOT_BANNER_X = 30, BOOT_BANNER_Y = 150, BOOT_BANNER_W = 580, BOOT_BANNER_H = 140,
    BOOT_HOLD_TICKS = 500   /* how long it stays between its fades, on the timer (100 a second): five seconds */
};

typedef enum {
    BOOT_HOUSE,      /* "Bytes & Rust": graver18 twice its size, red */
    BOOT_PRESENTS,   /* graver18 at 4/5, white, letters spread */
    BOOT_SUBTITLE,   /* graver18 at 4/5, white */
    BOOT_SMALL,      /* graver18 at 3/5, white */
    BOOT_NAME        /* the game's name in the banner's place, graver18 three times its size, yellow: only when there is no banner */
} BootStyle;

typedef struct {
    char text[BOOT_LINE_CHARS];
    BootStyle style;
    int y;   /* the top of the line on the 640 x 480 screen; centred across it */
} BootLine;

/* The screen's lines, top to bottom, for `version` (NULL or empty: no version line); how many (never more than BOOT_LINES_MAX). The BOOT_NAME line is
 * among them; the drawing leaves it out when it has the banner. */
int boot_screen_lines(const char *version, BootLine out[BOOT_LINES_MAX]);

/* What skips it: any key held (`key`), any of the pointer's buttons, any button of a pad; each a press of its own (screen_hold.h). */
void boot_screen_sources(int key, int buttons, int pad, HoldSource out[HOLD_SOURCES]);

/* port (boot_screen_glue.c): shows it, fades in and out, BOOT_HOLD_TICKS between; any key, click or pad button ends it early. Once per run: later
 * calls do nothing. */
void boot_screen_show(void);

#endif

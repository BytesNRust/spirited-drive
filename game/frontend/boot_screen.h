/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: our own boot screen, shown once after EA's intro and title movies: who made the restomod, what it is, and whose game it is. The words (this file's
 * layout, pure) and the drawing (boot_screen_glue.c) are apart so the words can be tested. */
#ifndef TNFS_BOOT_SCREEN_H
#define TNFS_BOOT_SCREEN_H

enum { BOOT_LINES_MAX = 8, BOOT_LINE_CHARS = 96 };

typedef enum { BOOT_TITLE, BOOT_HEADLINE, BOOT_SMALL } BootSize;

typedef struct {
    char text[BOOT_LINE_CHARS];
    BootSize size;
    int gap;   /* blank pixels above the line */
} BootLine;

/* The screen's lines, top to bottom, for `version` (NULL or empty: no version line); how many (never more than BOOT_LINES_MAX). */
int boot_screen_lines(const char *version, BootLine out[BOOT_LINES_MAX]);

/* port (boot_screen_glue.c): shows it, fades in and out, about 3 seconds, any key or click ends it early. Once per run: later calls do nothing. */
void boot_screen_show(void);

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the pages our credits add after the original's five (credits.fsh, from the player's own CD): the homage, then ours, set as the original pages set theirs
 * (headers underlined across a column, the names centred under them) over the same relief without its text (credits_relief.h). The words and where they stand
 * (this file, pure) and the drawing (credits_pages_glue.c) are apart so the words can be tested. */
#ifndef TNFS_CREDITS_PAGES_H
#define TNFS_CREDITS_PAGES_H

enum { CREDITS_PAGES = 2, CREDITS_ITEMS_MAX = 24, CREDITS_ITEM_CHARS = 80, CREDITS_COLUMN_W = 196 };

typedef enum {
    CREDITS_BIG,      /* graver18 at twice its size: the homage's two lines */
    CREDITS_TITLE,    /* graver18 at its size: our name over our column */
    CREDITS_LINE,     /* graver18 at 5/4: the homage's dedication */
    CREDITS_NAME,     /* graver18 at 4/5: a name under a header, as the original pages set theirs */
    CREDITS_HEADER,   /* a header as the original pages draw theirs (text_draw_header, 2/3), underlined across its column */
    CREDITS_NOTE      /* graver18 at 3/5: the legal lines and the version at the foot */
} CreditsKind;

typedef enum { CREDITS_CREAM, CREDITS_YELLOW } CreditsColour;   /* a header is always in the headers' tan */

typedef struct {
    char text[CREDITS_ITEM_CHARS];
    CreditsKind kind;
    CreditsColour colour;
    int cx, y;   /* centred on cx (a header: its column is CREDITS_COLUMN_W wide around cx); y the text's top, on the 640x480 screen */
} CreditsItem;

/* page (from 0) for `version` (NULL or empty: no version line) into out; the item count, 0 when there is no such page */
int credits_page(int page, const char *version, CreditsItem out[CREDITS_ITEMS_MAX]);

/* port (credits_pages_glue.c): shows the pages one after another, each faded in and out, until a key or a click (or about 12 seconds); `tick` (may be NULL)
 * is called every frame, for the credits' music. Over the relief when it can be composed, over black when it cannot. */
void credits_pages_show(const char *version, void (*tick)(void));

#endif

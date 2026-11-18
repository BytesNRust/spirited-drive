/* port (dep59 showcase, PR B): an added car's gallery page as the glue and its drawing share it (added_gallery_glue.c runs the page,
 * added_gallery_draw.c draws what changes over its still part). */
#ifndef TNFS_ADDED_GALLERY_PAGE_H
#define TNFS_ADDED_GALLERY_PAGE_H

#include "game/frontend/added_gallery.h"
#include "game/frontend/boot_screen.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/text_wrap.h"

#include <stdint.h>

enum {
    KEPT = 18,                                       /* Control Central's last entries, kept for the pointer */
    TEXT_FIRST = 256 - KEPT - BOOT_TEXT_COLOURS,     /* the text's colours (BOOT_TEXT_RGB's order) */
    SWATCH = TEXT_FIRST - 1,                         /* the Color swatch's entry */
    ROW_EXACT = 8, ROW_FIRST = SWATCH - ROW_EXACT,   /* the bottom row's most used colours, the CD art's own; its others the nearest */
    STILL_COLOURS = ROW_FIRST,                       /* the still part's: 0 .. ROW_FIRST - 1 */
    WHITE = TEXT_FIRST, CREAM = TEXT_FIRST + 1, YELLOW = TEXT_FIRST + 2, BLACK = TEXT_FIRST + 5, DIM = TEXT_FIRST + 6,
    LEAN_NUM = 2, LEAN_DEN = 9,                      /* the name's slant: 2 px across for 9 up */
    HISTORY_LINES = 64                               /* the description's lines kept (its head and the text) */
};

/* one line of the History tile: a run of `text` */
typedef struct {
    const char *text;
    int len;
} HistoryLine;

typedef struct {
    CarPieceCard card;
    GalleryState state;
    int car;
    int warrior;                     /* the Warrior's page: its card from the CD, its look fixed */
    uint8_t *still;                  /* the still part in the page's palette, GL_W x GL_H */
    uint8_t palette[768];
    uint8_t screen[768];             /* Control Central's palette (the kept entries, the Warrior's card) */
    uint8_t row_map[64];             /* the bottom row's colours' entries (GL_ROW_COLOURS) */
    int shown;                       /* the big photo the still part shows */
    int panel_fill;                  /* the SETUP panel's fill: the still part's nearest entry to a dark sepia */
    char head[200];                  /* "Name (year) - by Author - vX", in the small face's codes */
    char body[MOD_INI_DESCRIPTION_MAX + 1];   /* the description, in the small face's codes */
    HistoryLine lines[HISTORY_LINES];
    int line_count;
} Page;

/* the page's words and figures over its still part, as `s` says (focus, photo n of m, the Color tile's two rows, the description from its
 * scroll); the bottom row is the glue's */
void added_gallery_draw_tiles(const Page *p, uint8_t *to, int tw, int th);
/* the name big and leant, "Car Showcase" under it */
void added_gallery_draw_title(const Page *p);
/* the description's lines made for the tile (the head, then the text), and how many the tile shows */
void added_gallery_history_make(Page *p);
int added_gallery_history_visible(void);

#endif

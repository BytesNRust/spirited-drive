/* port (dep59 showcase, PR B): an added car's gallery page as the glue and its drawing share it (added_gallery_glue.c runs the page,
 * added_gallery_draw.c draws what changes over its still part). */
#ifndef TNFS_ADDED_GALLERY_PAGE_H
#define TNFS_ADDED_GALLERY_PAGE_H

#include "game/frontend/added_gallery_setup.h"
#include "game/frontend/added_gallery.h"
#include "game/frontend/boot_screen.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/mech_figures.h"
#include "game/frontend/text_wrap.h"

#include <stdint.h>

enum {
    KEPT = 18,                                       /* Control Central's last entries, kept for the pointer */
    TEXT_FIRST = 256 - KEPT - BOOT_TEXT_COLOURS,     /* the text's colours (BOOT_TEXT_RGB's order) */
    SKIN_SWATCH = TEXT_FIRST - 1, SWATCH = TEXT_FIRST - 2,   /* the SETUP panel's Skin and Color previews' entries */
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
    uint8_t tile[GL_COL_W * GL_COL_H];   /* the closed SETUP tile's picture in the page's entries (tile_ok) */
    int tile_ok;
    uint8_t skin_rgb[CAR_PIECE_SKINS_MAX + 1][3], default_rgb[3];   /* the previews' colours: each skin's, the car as it comes */
    uint8_t tint[3];                 /* the background's tint (the mod's first paint), has_tint */
    int has_tint;
    SetupArt *art;                   /* the panel's stone (NULL: not made); art->ok 0 -> the flat fill */
    int moving;                      /* the panel is opening or closing: frame `frame` of SETUP_FRAMES is drawn */
    int frame;
    int panel_fill;                  /* the SETUP panel's fill: the still part's nearest entry to a dark sepia */
    char foot[200];                  /* "vX - by Author", in the small face's codes: the History text's last line */
    char body[MOD_INI_DESCRIPTION_MAX + 1];   /* the description, in the small face's codes */
    HistoryLine lines[HISTORY_LINES];
    MechFigures mech;                /* Mechanical's power, weight and ratio in the player's units (mech_figures.h) */
    int line_count;
} Page;

/* the previews' colours and the background's tint of the car (added_gallery_tints.c) */
void added_gallery_tints_make(Page *p);

/* the page's words and figures over its still part, as `s` says (focus, photo n of m, the Color tile's two rows, the description from its
 * scroll); the bottom row is the glue's */
void added_gallery_draw_tiles(const Page *p, uint8_t *to, int tw, int th);
/* the name big and leant, "Car Showcase" under it */
void added_gallery_draw_title(const Page *p);
/* the name alone, big and leant (the RECORDS screen's own heading goes where "Car Showcase" is) */
void added_gallery_draw_name(const Page *p);
/* a General or Mechanical value cut with "..." to fit `room` px in the figures' face ("-" for none; "" when not even the dots fit) */
void added_gallery_figure_fit(const char *value, int room, char *out, int size);
/* the description's lines made for the tile (the text, a blank line and the foot "vX - by Author"; no heading) */
void added_gallery_history_make(Page *p);
/* the History text's line height and the window's height in pixel rows (the glue's lines op: the scroll is in pixel rows) */
int added_gallery_history_line_px(void);
int added_gallery_history_window(void);

#endif

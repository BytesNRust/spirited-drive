/* port: the big titles of the port's own screens ("Customize Game", "Graphics Detail") in the letters of the original
 * screens' burnt-in titles, which have serifs graver18 lacks. No EA pixels live in the repository: the table holds
 * only where each letter stands on which CD screen and that screen's title colour; the letters are cut out at run
 * time as a coverage mask (0..15, text_soft.h), independent of their hue, and drawn red with the port's own look
 * (text_draw_mask). */
#ifndef TNFS_TITLE_ART_H
#define TNFS_TITLE_ART_H

#include "game/frontend/config_bgnd.h"

#include <stdint.h>

typedef enum {
    TITLE_ART_CUSTOMIZE_GAME,
    TITLE_ART_GRAPHICS_DETAIL,
    TITLE_ART_GAME_MODE,   /* Game Mode's own title, uncut: the yardstick the look is compared against */
    TITLE_ART_COUNT
} TitleArtId;

typedef enum {
    TITLE_ART_GAMEMODE_QFS,   /* control/gamemode.qfs */
    TITLE_ART_CENTRAL_QFS,    /* control/central.qfs */
    TITLE_ART_TRMTSTND_QFS,   /* stats/trmtstnd.qfs */
    TITLE_ART_GRAPHICS_QFS,   /* option/graphics.qfs */
    TITLE_ART_SOURCES
} TitleArtSource;

enum {
    TITLE_ART_SCREEN_W = 640, TITLE_ART_SCREEN_H = 480,
    /* where the originals' titles stand: the first letter's left column, the capitals' top row and the baseline */
    TITLE_ART_X = 9, TITLE_ART_TOP = 66, TITLE_ART_BASELINE = 84,
    TITLE_ART_LETTER_GAP = 4,   /* empty columns between two letters, as the titles set them (3..6, mostly 4) */
    TITLE_ART_WORD_GAP = 23,    /* empty columns between two words */
    TITLE_ART_PAD = 3,          /* round the letters in the mask: the rim up-left, the shadow's 2 px down-right */
    TITLE_ART_H = TITLE_ART_BASELINE - TITLE_ART_TOP + 1 + 2 * TITLE_ART_PAD,
    TITLE_ART_MAX_W = 400,
    /* the mask's (0, 0) on the screen */
    TITLE_ART_LEFT = TITLE_ART_X - TITLE_ART_PAD, TITLE_ART_ROW0 = TITLE_ART_TOP - TITLE_ART_PAD,
    /* where the port's screens put the font's title line (TITLE_X, TITLE_Y of extras_menu.c, graphics_own.c) */
    TITLE_ART_FONT_X = 8, TITLE_ART_FONT_Y = 56,
    /* the cut-out ramp: a pixel's part of the title colour (its chroma projected on the title's) below LOW is not
     * letter, above HIGH all letter; in thousandths. The shadows, the pale rim and the textured background fall below */
    TITLE_ART_LOW = 300, TITLE_ART_HIGH = 700
};

/* one CD screen: its 640x480 "bgnd" as palette indices, and the palette */
typedef struct {
    const uint8_t *pixels;
    const uint8_t *palette;
} TitleArtScreen;

/* The letters in the box x0..x1, y0..y1 (inclusive) of `pixels` (pw wide, indices into `palette`) as coverage 0..15
 * into `mask` ((x1 - x0 + 1) x (y1 - y0 + 1)): each pixel's colour less its grey, projected on the title colour
 * `face` less its grey, ramped from TITLE_ART_LOW to TITLE_ART_HIGH. */
void title_art_mask_from_screen(const uint8_t *pixels, int pw, const uint8_t palette[768], const uint8_t face[3],
                                int x0, int y0, int x1, int y1, uint8_t *mask);

/* a screen's "bgnd" (pointing into `data`, `size` bytes of a CD file) and its palette into `palette` and `s`. 1 on
 * success. */
int title_art_screen(uint8_t *data, size_t size, uint8_t palette[768], TitleArtScreen *s);

/* Title `id` set from its letters on the screens (only its own sources are read; the others may be NULL) into
 * `mask` (room for TITLE_ART_MAX_W x TITLE_ART_H), w x TITLE_ART_H with its width in *w; its (0, 0) stands at
 * TITLE_ART_LEFT, TITLE_ART_ROW0 on the screen. 1 on success, 0 when a source it needs is missing. */
int title_art_compose(TitleArtId id, const TitleArtScreen screens[TITLE_ART_SOURCES], uint8_t *mask, int *w);

/* The small capital z, which no screen has, made from the small e (its top bar mirrored, its bottom bar) and a
 * diagonal of the e's stroke width: ew x eh coverage in, the same size out. */
void title_art_small_z(const uint8_t *e, int ew, int eh, uint8_t *z);

/* Reads title `id`'s sources through `read` and composes it into `mask` (as title_art_compose). 1 on success. */
int title_art_load(ConfigBgndRead read, TitleArtId id, uint8_t *mask, int *w);

/* the title drawn for `text`, TITLE_ART_COUNT when there is none */
TitleArtId title_art_for(const char *text);

#endif

/* port: the port's item-size labels in the letters of the original menu items (item_art.h's Game Mode items), built at
 * run time from the CD as a hue-independent coverage mask (0..15, text_soft.h), drawn cream or yellow with the port's
 * own look (text_draw_item_mask). No EA pixels live in the repository: the table holds only where each letter stands
 * in which CD shape or screen; the letters are cut out at run time. */
#ifndef TNFS_ITEM_ART_H
#define TNFS_ITEM_ART_H

#include "game/frontend/config_bgnd.h"
#include "game/frontend/title_art.h"

#include <stdint.h>

typedef enum {
    ITEM_ART_CUSTOMIZE_GAME,   /* Game Mode's Modem slot, "Customize game" */
    ITEM_ART_COUNT
} ItemArtId;

enum {
    ITEM_ART_PAD = 3,          /* round the letters in the mask, as title_art.h's TITLE_ART_PAD: the shadow's 2 px */
    ITEM_ART_CAP_H = 19,       /* the capitals' rows, the items' and the titles' alike; the small capitals are 15 */
    ITEM_ART_H = ITEM_ART_CAP_H + 2 * ITEM_ART_PAD,
    ITEM_ART_MAX_W = 400,
    /* the items' spacing, measured on Single player (the word gap) and the letters of all four items (the rest) */
    ITEM_ART_LETTER_GAP = 5,   /* empty columns between two letters (3..7, mostly 4..6) */
    ITEM_ART_WORD_GAP = 26     /* empty columns between two words: SINGLE PLAYER's */
};

/* One of Game Mode's items as the CD draws it twice: the dark shape (cream letters) and the lit one (yellow). Both
 * share the frame, the background, the shadow and the palette, so only the letters differ between them. */
typedef struct {
    const uint8_t *dark, *lit;
    int w, h;
    const uint8_t *palette;   /* RGB, 256 entries: the shapes' palette */
} ItemArtShape;

/* the CD's pictures the letters are cut from: the three items of control/gamemode.qfs and the two screens whose
 * titles give the capital and the one small letter the items lack (their pixels as title_art.h's screens) */
typedef struct {
    ItemArtShape single, modem, network;   /* "sind"/"sinl", "modd"/"modl", "netd"/"netl" */
    TitleArtScreen central, trmtstnd;
} ItemArtSources;

/* The letters in the box x0..x1, y0..y1 (inclusive) of a dark and a lit shape (pw wide, indices into `palette`) as
 * coverage 0..15 into `mask` ((x1 - x0 + 1) x (y1 - y0 + 1)): each pixel's difference between the two shapes,
 * projected on the difference of the cream and the yellow fill, so the background and the shadow cancel. */
void item_art_mask_from_pair(const uint8_t *dark, const uint8_t *lit, int pw, const uint8_t palette[768], int x0,
                             int y0, int x1, int y1, uint8_t *mask);

/* The item `id` set from its letters (only its own sources are read; the others may be NULL) into `mask` (room for
 * ITEM_ART_MAX_W x ITEM_ART_H), w x ITEM_ART_H with its width in *w. 1 on success, 0 when a source it needs is
 * missing. */
int item_art_compose(ItemArtId id, const ItemArtSources *sources, uint8_t *mask, int *w);

/* Reads the item's sources through `read` and composes it into `mask` (as item_art_compose). 1 on success. */
int item_art_load(ConfigBgndRead read, ItemArtId id, uint8_t *mask, int *w);

/* the item drawn for `text`, ITEM_ART_COUNT when there is none */
ItemArtId item_art_for(const char *text);

#endif

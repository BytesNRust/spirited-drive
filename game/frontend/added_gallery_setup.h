/* port (dep59 showcase, PR C): the art of the SETUP panel's opening, from the CD's own load-list animation (option/loadanim.qfs: the stone
 * slabs opn1 .. opn7 with a hole growing from a slit, the game's own opening of its lists, sound 0x49): the stone cut to the panel's size,
 * its borders mirrored (the slab's bevel left out), and each frame's mask, all mapped to the page's palette (the nearest entry, as the text
 * is). No drawing here. */
#ifndef TNFS_ADDED_GALLERY_SETUP_H
#define TNFS_ADDED_GALLERY_SETUP_H

#include "game/frontend/gallery_layout.h"

#include <stdint.h>

enum { SETUP_FRAMES = 7, TILE_SRC_W = 152, TILE_SRC_H = 62, TILE_SRC_X = 82 + 51, TILE_SRC_Y = 326 + 3 };   /* a piece of the Mechanical picture (82,326 254x87; its black top 3 rows left out, owner 2026-11-19) in the bank's background, its top middle: the label "MECHANICAL" is baked in its foot */   /* opn1 .. opn7; the panel open is the eighth */

typedef struct {
    int ok;                                              /* the art was read: else the panel is a flat fill and opens at once */
    uint8_t slab[GL_SETUP_W * GL_SETUP_H];               /* the stone as it is, in the page's entries */
    uint8_t body[GL_SETUP_W * GL_SETUP_H];               /* the stone darkened, the panel's ground */
    uint8_t stone[SETUP_FRAMES][GL_SETUP_W * GL_SETUP_H]; /* 1 where frame k's slab is, 0 where its hole is */
} SetupArt;

/* the art from the CD's file (`file`, `size`) for the page palette `pal` (8 bits a channel) with `usable` entries; 1 when it was all there */
int added_gallery_setup_art(SetupArt *art, const uint8_t *file, size_t size, const uint8_t pal[768], const uint8_t usable[256]);

/* the closed SETUP tile's picture (owner 2026-11-18: the Supra's engine, the Mechanical tile's place in the background of the bank of
 * FRONTEND/SHOW/SUPRA.QFS, 254 x 87) scaled to GL_COL_W x GL_COL_H, each pixel the page palette's nearest entry (`usable` as above); 1 when it was there */
/* the RECORDS tile's picture: the chequered flag of FRONTEND/ART/WINNER.QFS (`file`, `size`), its upper part as RGB at the Video tile's size; 0 when the bank is not that */
int added_gallery_flag_rgb(uint8_t out[GL_VID_W * GL_VID_H * 3], const uint8_t *file, size_t size);
int added_gallery_setup_tile(uint8_t out[GL_COL_W * GL_COL_H], const uint8_t *file, size_t size, const uint8_t pal[768], const uint8_t usable[256]);

#endif

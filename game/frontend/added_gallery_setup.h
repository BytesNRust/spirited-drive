/* port (dep59 showcase, PR C): the art of the SETUP panel's opening, from the CD's own load-list animation (option/loadanim.qfs: the stone
 * slabs opn1 .. opn7 with a hole growing from a slit, the game's own opening of its lists, sound 0x49): the stone cut to the panel's size,
 * its borders mirrored (the slab's bevel left out), and each frame's mask, all mapped to the page's palette (the nearest entry, as the text
 * is). No drawing here. */
#ifndef TNFS_ADDED_GALLERY_SETUP_H
#define TNFS_ADDED_GALLERY_SETUP_H

#include "game/frontend/gallery_layout.h"

#include <stdint.h>

enum { SETUP_FRAMES = 7 };   /* opn1 .. opn7; the panel open is the eighth */

typedef struct {
    int ok;                                              /* the art was read: else the panel is a flat fill and opens at once */
    uint8_t slab[GL_SETUP_W * GL_SETUP_H];               /* the stone as it is, in the page's entries */
    uint8_t body[GL_SETUP_W * GL_SETUP_H];               /* the stone darkened, the panel's ground */
    uint8_t stone[SETUP_FRAMES][GL_SETUP_W * GL_SETUP_H]; /* 1 where frame k's slab is, 0 where its hole is */
} SetupArt;

/* the art from the CD's file (`file`, `size`) for the page palette `pal` (8 bits a channel) with `usable` entries; 1 when it was all there */
int added_gallery_setup_art(SetupArt *art, const uint8_t *file, size_t size, const uint8_t pal[768], const uint8_t usable[256]);

#endif

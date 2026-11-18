/* port (dep59 step 5, T7e2): Best Times' art for an added track, which the CD does not have: Coastal's (COASTBST, the stage layout) with
 * its title taken out. The box of the title is covered with the page's texture from ADDED_BEST_SOURCE_SHIFT rows below (the grid of indices is the
 * CD's own palette's, not a ramp: nothing can be blended); the
 * track's name is drawn over it by best_times.c, in the game's text. Nothing else of the bank changes: its palette, DONE, the arrows,
 * Best times' own lit and dim pictures stay as the CD's. */
#ifndef TNFS_ADDED_BEST_ART_H
#define TNFS_ADDED_BEST_ART_H

#include <stddef.h>
#include <stdint.h>

enum {
    ADDED_BEST_W = 640, ADDED_BEST_H = 480,
    ADDED_BEST_BOX_X0 = 6, ADDED_BEST_BOX_X1 = 330, ADDED_BEST_BOX_Y0 = 8, ADDED_BEST_BOX_Y1 = 50,   /* the title's box, inclusive */
    ADDED_BEST_SOURCE_SHIFT = 108   /* the rows copied in: the box's own plus this (rows 116..158: below the red column heads, which end at row 114) */
};

/* `coast` (`size` bytes, Coastal's decompressed bank) made into the bank in `*out` (malloc'd, `*out_size`): 0 with a reason in `why` when it
 * is not a bank with a 640 x 480 bgnd */
int added_best_art_make(const uint8_t *coast, size_t size, uint8_t **out, size_t *out_size, char *why, size_t why_size);

/* (added_best_art_bank.c) the bank for the game's language put as a bank the menus draw from; 0 when it cannot be made (logged): the caller
 * shows Coastal's own */
int32_t added_best_art_bank(void);

#endif

/* The texture a car mask belongs to (dep59 car editor E3): read from the car's CFM with the walk the game's livery code uses (formats/car_bank_walk.h), as RGB through the
 * bank's own palette ('!PAL'), to show under the mask. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_MASK_TEX_H
#define TNFS_TRACK_EDITOR_MASK_TEX_H

#include <stddef.h>
#include <stdint.h>

enum { MASK_TEX_LIST = 32, MASK_TEX_NAME = 8 };

typedef struct {
    int level;                    /* 0 the high detail bank, 1 the low one */
    char name[MASK_TEX_NAME];     /* the texture's 4-letter name, as the car file spells it */
    char file[32];                /* the mask's file name: "<name>_mask.png", "<name>_lo_mask.png" for the low bank */
    int width, height;
} MaskTexInfo;

/* the bitmap textures of both banks of the CFM at `cfm` (up to `cap`), in file order: how many; 0 for a file that is not a car's CFM */
int mask_tex_list(const char *cfm, MaskTexInfo *out, int cap);

/* One texture as RGB (width x height x 3, free with free()). 1, or 0 when the file has no such texture. A palette of 6 bits a channel (type 0x22) is scaled to 8. */
int mask_tex_rgb(const char *cfm, int level, const char *name, int *width, int *height, uint8_t **rgb);

#endif

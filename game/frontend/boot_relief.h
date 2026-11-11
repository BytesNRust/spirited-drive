/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the boot screen's red relief (i030), as the owner approved it (out7 r5): TITLE.QFS's red relief with pieces of three installer screens
 * pasted over it (the owner's collage), the Diablo picture Control Central shows turned into relief in the top left, and the seams blended away. No EA
 * pixels live in the repository: the pieces and the picture are taken from the player's CD at run time, and the region map (data/boot/boot_bg_map.png,
 * which source each pixel takes) is ours. The blend, pure, here; the reading in boot_screen_glue.c.
 * The blend: the luminance solved again with the gradients across a seam dropped (a screened Poisson solve), so each piece keeps its detail and the steps
 * between them go; where no source on the CD is clean (the installers' title rule, TITLE's logo, the zone the owner left) a fill: thin runs mirrored from
 * the rows above and below, the rest the edge's level with a calm grain; the picture embossed with the collage's light (top left); the colour from the
 * collage's own luminance -> colour table with each pixel's own tint, carried over the picture and the fill from their edge; then the owner's marks: the top
 * band smoothed across, the title-rule band smoothed down, the fill darker at the top and lighter under the banner. */
#ifndef TNFS_BOOT_RELIEF_H
#define TNFS_BOOT_RELIEF_H

#include <stdint.h>

enum {
    BOOT_RELIEF_W = 640,
    BOOT_RELIEF_H = 480,
    BOOT_RELIEF_SCREENS = 3,                 /* the installer screens the pieces come from */
    BOOT_MAP_TITLE = 0,                      /* the map's values: TITLE.QFS's own pixel */
    BOOT_MAP_FILL = 1,                       /* no clean source: filled */
    BOOT_MAP_PHOTO = 2,                      /* the picture */
    BOOT_MAP_PIECE = 3,                      /* 3.. a piece: BOOT_RELIEF_PIECE[value - 3] */
    BOOT_MAP_PIECES = 7,
    /* the picture: control/dia1.qfs's "0000", this crop of it, placed here */
    BOOT_PHOTO_CROP_X = 65, BOOT_PHOTO_CROP_Y = 13, BOOT_PHOTO_W = 204, BOOT_PHOTO_H = 45,
    BOOT_PHOTO_X = 73, BOOT_PHOTO_Y = 41
};

/* a piece: which installer screen, shifted (the screen's pixel at (x + dx, y + dy) goes to (x, y)) */
typedef struct {
    int screen, dx, dy;
} BootReliefPiece;

extern const char *const BOOT_RELIEF_SCREEN_NAMES[BOOT_RELIEF_SCREENS];   /* "iart/instunin.qfs" ... */
extern const BootReliefPiece BOOT_RELIEF_PIECE[BOOT_MAP_PIECES];

typedef struct {
    const uint8_t *title;                              /* TITLE.QFS's bgnd, RGB, W x H x 3 */
    const uint8_t *screens[BOOT_RELIEF_SCREENS];       /* the installer screens' bgnd, RGB */
    const uint8_t *photo;                              /* the picture's crop, RGB, BOOT_PHOTO_W x BOOT_PHOTO_H x 3 */
    const uint8_t *map;                                /* W x H of BOOT_MAP_* */
} BootReliefSources;

/* The relief, RGB (W x H x 3). 1 on success, 0 when there is no memory for the work or the map has a value it does not know. */
int boot_relief_compose(const BootReliefSources *in, uint8_t rgb[BOOT_RELIEF_W * BOOT_RELIEF_H * 3]);

/* port (boot_relief_glue.c): the sources read off the player's CD (title.qfs, the installer screens, control/dia1.qfs through central.qfs's palette)
 * and the map from the port's data, composed. 0 (and a line in the log) when one is missing. */
int boot_relief_load(uint8_t rgb[BOOT_RELIEF_W * BOOT_RELIEF_H * 3]);

#endif

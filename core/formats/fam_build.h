/* A track's art file (FAM) built from pictures: the terrain textures of a stage, its horizon and its scenery textures. The file is the
 * "wwww" tree of formats/chunk.h: child 0 the terrain (a table with one SHPI bank per texture id, the bank n filling the slots n*3+L),
 * child 1 the scenery (a table, one bank per scenery texture), child 2 the horizon (one SHPI bank with its "horz" picture), child 3 the
 * scenery objects (an empty table). Every bank is laid out by shpi_build (formats/shpi_build.h) and holds the stage's palette as its
 * "!pal" entry; the palette is shared, one 256-colour palette quantised from every picture (fam_image.h). A terrain bank has the
 * letters A, B and C, each with the variants 000 to 007 at the sizes of the CD's own stage (fam_build.c); the variants are the one
 * texture resampled, so the far ones are small. A stage needs two terrain banks at least: with one the game takes it for a circuit. */
#ifndef TNFS_FAM_BUILD_H
#define TNFS_FAM_BUILD_H

#include "formats/exchange.h"
#include "formats/fam_image.h"
#include "formats/fam_spec.h"   /* FAM_LETTER_* */

#include <stddef.h>
#include <stdint.h>

enum { FAM_VARIANTS = 8, FAM_LETTERS = 3 };

typedef struct {
    const FamPicture *terrain;   /* terrain[i] is texture id i */
    size_t terrain_count;
    const FamPicture *scenery;   /* scenery[i] is the scenery texture i; may be empty */
    size_t scenery_count;
    const FamPicture *horizon;   /* the sky and the background: required */
    const unsigned *terrain_letters;   /* terrain_letters[i]: the FAM_LETTER_ bits (fam_spec.h) bank i holds; NULL: all three for every bank */
} FamSource;

/* The FAM bytes (malloc'd, free with free()). 0 with the reason in `err`: no horizon, fewer than 2 terrain textures, more than the
 * game holds (terrain 48, scenery 63, both 96: validate limits, formats/limits.h), or a file over the 0x162400 bytes the game reads. */
int fam_build(const FamSource *src, uint8_t **out, size_t *out_len, ExchangeError *err);

#endif

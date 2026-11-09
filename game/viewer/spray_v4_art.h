/* port (spray v4c): the sprites of each material, each its own look, made once from code (no art files). Every sprite is a SPRAY_V4_ART_SIZE square of 8-bit alpha, nothing on the border; the compositor tints it with the
 * material's colour. A family has SPRAY_V4_ART_VARIANTS sprites: for the clouds, sizes or shapes; for the clusters, sizes. Every sprite is a billboard: drawn square, facing the camera, never squashed.
 *   DROP      the water's spray cluster: a loose cluster of tiny speckles, denser at the core, a ragged edge
 *   MIST      the water's mist: a soft round puff
 *   EARTH     the desert's dust: a cottony cloud of round lobes, the core the densest (as the game's own smoke)
 *   SNOW      the snow's powder: fine, mottled grain with loose bright sparks and frayed edges
 *   CLUMP     a small lump of snow
 *   STONE     a gravel stone: an angular, irregular stone, with a glint (spray_v4_art_glint) of its own
 *   FINE_DUST the gravel's light dust: finer and paler than the earth's */
#ifndef TNFS_SPRAY_V4_ART_H
#define TNFS_SPRAY_V4_ART_H

#include <stdint.h>

enum {
    SPRAY_V4_ART_SIZE = 64,
    SPRAY_V4_ART_VARIANTS = 5
};

typedef enum {
    SPRAY_V4_ART_DROP = 0,
    SPRAY_V4_ART_MIST,
    SPRAY_V4_ART_EARTH,
    SPRAY_V4_ART_SNOW,
    SPRAY_V4_ART_CLUMP,
    SPRAY_V4_ART_STONE,
    SPRAY_V4_ART_FINE_DUST,
    SPRAY_V4_ART_FAMILIES
} SprayV4Art;

/* the alpha of a family's variant, SPRAY_V4_ART_SIZE^2 bytes (NULL out of range) */
const uint8_t *spray_v4_art(int family, int variant);

/* the stone's glint: 255 where the stone catches the light (a pixel or two), 0 elsewhere; NULL for the other families */
const uint8_t *spray_v4_art_glint(int family, int variant);

#endif

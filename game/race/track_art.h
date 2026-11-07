/* Track textures from a FAM file (0044c36c, terrain part).
 *
 * The race keeps a table of 144 texture slots (0050eec4), 0x60 bytes per
 * texture id: three slots (letters A, B, C), each of 8 variants named by the
 * last character '0'..'7'. A segment's texture byte (track_geometry.h) is a
 * slot number, id * 3 + letter.
 *   - Circuits (FAM terrain child holds one SHPI): slot id*3+L is named
 *     "%02d%c0" (id, 'A'+L), e.g. "01A0", "01A4".
 *   - Stages (one SHPI per id, 12 to 42 of them): SHPI n fills slots n*3+L
 *     from "%c000", e.g. "A000".."A007".
 * A missing variant reuses the previous one (0044c318); names are tried upper
 * case, then lower case (0044c1fc). Bitmaps holding colour 0xFF are drawn
 * with it transparent (0044c290).
 *
 * Scenery textures (005100c4), indexed by the object descriptor's resource
 * id: "%02d00" in the single SHPI of a circuit, or entry "0000" of the n-th
 * SHPI on a stage. */
#ifndef TNFS_TRACK_ART_H
#define TNFS_TRACK_ART_H

#include "formats/chunk.h"
#include "formats/fsh.h"

#include <stdint.h>

enum {
    TRACK_TEXTURE_IDS = 48,
    TRACK_TEXTURE_SLOTS = TRACK_TEXTURE_IDS * 3,  /* 0x90 */
    TRACK_TEXTURE_VARIANTS = 8,
    TRACK_SCENERY_TEXTURES = 64,  /* 005100c4, 0x10 bytes each */
    TRACK_TEXTURE_TRANSPARENT_TYPE = 0x45  /* 0044c290 retypes such bitmaps */
};

typedef struct {
    int present;       /* 0: no bitmap for this slot */
    int transparent;   /* holds colour 0xFF (0044c290) */
    FshEntry bitmap;
} TrackTexture;

typedef struct {
    TrackTexture slots[TRACK_TEXTURE_SLOTS][TRACK_TEXTURE_VARIANTS];
    uint8_t palette[768];  /* "!pal" of the first terrain SHPI, 8-bit RGB */
    int has_palette;
    TrackTexture scenery[TRACK_SCENERY_TEXTURES];  /* by descriptor resource id / 4 */
    FshEntry horizon;      /* "horz" of the horizon child */
    int has_horizon;
} TrackArt;

/* root: the FAM file's top "wwww" chunk; the bitmaps point into its data,
 * which must outlive the TrackArt. Returns 0 when the layout is unexpected. */
int track_art_load(TrackArt *art, const Chunk *root);

/* port (snow_weather): the snow textures laid over the road's: slot `from` takes the bitmaps of slot `to` (all its variants) for every pair in the map (`map[from]` is to + 1, 0 none), so the polygons that
 * used the one are drawn with the other. No pixel is new: both are the track's own art. A pair whose `to` slot has no bitmap is left alone. Returns how many slots were taken over. */
int track_art_remap_slots(TrackArt *art, const uint8_t map[TRACK_TEXTURE_SLOTS]);

/* 0044c36c's "ga%d0" on a circuit (one terrain SHPI): with id the TRI header's short +0x28 / 3, slot id * 3 + k
 * (k 0..2) takes "ga<k>0".."ga<k>7" over what track_art_load gave it, when "ga<k>0" is there. Nothing on a stage. */
void track_art_load_animated(TrackArt *art, const Chunk *root, const uint8_t *tri_header);

#endif

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
    TRACK_SCENERY_TEXTURES = 64   /* 005100c4, 0x10 bytes each */
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

#endif

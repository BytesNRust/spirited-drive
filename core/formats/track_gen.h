/* Track generator: builds a point-to-point TRI from a text layout, with a template TRI (any CD track) supplying
 * everything the layout does not describe: header scalars, scenery object descriptors, the road cross-section of
 * the terrain strips, per-node verge/barrier/flag bytes and the brightness entry. The scenery placements are
 * dropped (they name nodes of the template). Texture ids stay valid because the FAMs of the template are kept.
 *
 * Layout, one directive per line, '#' starts a comment:
 *   section SEGMENTS [turn=DEG] [grade=PERCENT] [bank=DEG] [width=VERGE]
 * SEGMENTS is the length in terrain segments (4 road nodes each). turn is the total heading change over the
 * section, positive to the right; grade, bank and width are the values reached at the end of the section, the
 * generator ramps linearly from the previous section's end (0, 0 and the template's width at the start).
 * The track has 8..TRACK_GEN_MAX_SEGMENTS segments in all. Heights and headings are stored the way the game
 * reads them (docs/modding/inventory.md §5): 14-bit angles (0x4000 = full turn), 16.16 positions. */
#ifndef TNFS_TRACK_GEN_H
#define TNFS_TRACK_GEN_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum {
    TRACK_GEN_MIN_SEGMENTS = 8,
    TRACK_GEN_MAX_SEGMENTS = 599,   /* the closing node must still fit the 2400-node array */
    TRACK_GEN_NODE_LENGTH = 395000  /* distance between consecutive nodes, as on the CD tracks */
};

/* 1 and *out (malloc'd, *out_len bytes) on success; 0 with err naming the layout line otherwise. */
int track_generate(const char *layout, size_t layout_len, const uint8_t *template_tri, size_t template_size,
                   uint8_t **out, size_t *out_len, ExchangeError *err);

#endif

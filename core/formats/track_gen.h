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
 * reads them (docs/modding/inventory.md §5): 14-bit angles (0x4000 = full turn), 16.16 positions.
 *
 * Underneath, a track is its nodes: track_generate_nodes builds the TRI from explicit nodes (a position, a heading, a grade, a bank and a
 * width each), and a layout is one producer of them (track_layout_nodes). An editor makes its own nodes from a drawn trace. */
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

/* A node of the track as the generator writes it: the shape the game reads (headings 14-bit, positions 16.16) before it is packed. The first node
 * is the start; the last is the closing node (a track of N segments has 4N + 1). */
typedef struct {
    double heading;       /* 14-bit units (0x4000 = full turn), 0 along +z, positive to the right */
    double grade;         /* percent, +-30 */
    double bank;          /* degrees, +-20 */
    double width;         /* the half road width in the node's verge byte, 0..255 (16 terrain units each); a layout asks for 8..120 */
    int32_t position[3];  /* x, y, z in 16.16; consecutive nodes 1..20 units apart */
} TrackGenNode;

enum { TRACK_GEN_MAX_NODES = TRACK_GEN_MAX_SEGMENTS * 4 + 1 };

/* The nodes of a layout (`start_width` the width it starts from: the template's), positions integrated along the headings: 4 per segment and
 * the closing node in *nodes (malloc'd, *count). 0 with err naming the layout line otherwise. */
int track_layout_nodes(const char *layout, size_t layout_len, double start_width, TrackGenNode **nodes, int *count, ExchangeError *err);

/* The TRI of explicit nodes: `count` is 4 per segment and one more, 8..599 segments; each node finite, its width (0..255), grade and bank in range, its
 * heading at most 6 degrees from the node before, its distance from it 1..20 units. 1 and *out (malloc'd, *out_len bytes); 0 with err naming
 * the node otherwise. */
int track_generate_nodes(const TrackGenNode *nodes, int count, const uint8_t *template_tri, size_t template_size, uint8_t **out,
                         size_t *out_len, ExchangeError *err);

/* track_generate_nodes with the template's terrain rows turned to follow the road: the TRI keeps a row's points in world axes, and the
 * template's cross-section is taken in its own nodes' axes (track_frame.h) and laid along each new node's, so on a curve, a climb or a bank
 * the ground lies across the road and not across the world. The width is added across the road. On a flat straight at heading 0, with a template
 * whose model strip is flat at heading 0 too (our own, track_template.h), it is track_generate_nodes' bytes; elsewhere only the rows differ (to a rounding of the turn: 1/64 of a unit on a point). The layouts of
 * `track new` keep track_generate_nodes, so the pieces already made keep their bytes and their prints. */
int track_generate_nodes_turned(const TrackGenNode *nodes, int count, const uint8_t *template_tri, size_t template_size, uint8_t **out,
                                size_t *out_len, ExchangeError *err);

/* 1 and *out (malloc'd, *out_len bytes) on success; 0 with err naming the layout line otherwise. */
int track_generate(const char *layout, size_t layout_len, const uint8_t *template_tri, size_t template_size,
                   uint8_t **out, size_t *out_len, ExchangeError *err);

#endif

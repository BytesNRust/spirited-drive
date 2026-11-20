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

/* The most a node may turn from the node before, in degrees. Two limits, because the terrain is not laid the same way:
 *  - TRACK_GEN_TURN_MAX_DEGREES, for the nodes of the editor (track_generate_nodes_turned): the terrain rows follow the road and, on the inside of
 *    the curve, are held inside TRACK_GEN_TERRAIN_REACH of the curve's radius (the CD holds both sides: Alpine 3 keeps its terrain within 0.32..0.44 of the
 *    radius in its 16-degree curve, node 625, notes/dep59-editor-limits-vs-cd.md; the outside is left alone here (but for an S's easing) because the generic template has no
 *    scenery to hide the hole a cut would leave there). The cap is eased along the road, by a node length a row at most, so the edge of the
 *    terrain does not jump at a curve's entry or at an S's inflection (terrain_caps). The editor shows red what is over it, amber what is over
 *    TRACK_GEN_TURN_WARN_DEGREES (track_trace.h).
 *  - TRACK_GEN_LAYOUT_TURN_MAX_DEGREES, for a layout (`track new`, track_generate): its rows are the template's, not turned, and over 8 degrees
 *    the game draws the terrain of the inner side crossing itself (measured, same note). */
#define TRACK_GEN_TURN_MAX_DEGREES 16.5   /* the CD's most is 16.2 (TR6 turns 16.2 in a step, Alpine 3's 15.8); the half degree over 16 is for the editor's spline, which at the joint of a straight and a curve drawn at 16 gives 16.1 */
#define TRACK_GEN_TURN_WARN_DEGREES 8.0
#define TRACK_GEN_LAYOUT_TURN_MAX_DEGREES 8.0
/* The bank: the editor's nodes (track_generate_nodes_turned, whose rows turn with the node's frame) up to 35 degrees, the CD's own most (TR4 banks 34.5;
 * the editor's 25 is a warning: notes/dep59-banking-spike.md); a layout's and track_generate_nodes' (rows not turned) 20. */
#define TRACK_GEN_BANK_MAX_DEGREES 35.0
#define TRACK_GEN_LAYOUT_BANK_MAX_DEGREES 20.0
#define TRACK_GEN_TERRAIN_REACH 0.4   /* the furthest the terrain of a turned node goes across the road on the inside of the curve, as a part of the curve's radius there */

enum {
    TRACK_GEN_MIN_SEGMENTS = 8,
    TRACK_GEN_MAX_SEGMENTS = 599,   /* the closing node must still fit the 2400-node array */
    TRACK_GEN_NODE_LENGTH = 395000  /* distance between consecutive nodes, as on the CD tracks */
};

/* A side of a node (its index in TrackGenNode.side): what the road has beyond its edge on that side. `has` says which values the node sets; a side all zero is
 * the template's, so a node all zero is the road of before, byte for byte. The plan's step 2b sets the shoulder's surface; step 3 the shoulder's width,
 * the wall's distance and kind, the fence. */
enum { TRACK_GEN_LEFT = 0, TRACK_GEN_RIGHT = 1 };
enum {                                  /* TrackGenSide.has bits */
    TRACK_GEN_SIDE_TARMAC = 1 << 0,     /* `tarmac` (step 2b) */
    TRACK_GEN_SIDE_SHOULDER = 1 << 1,   /* `shoulder`: the row's second point of the side (the shoulder's end) is that far past the road edge */
    TRACK_GEN_SIDE_BARRIER = 1 << 2,    /* `barrier`: the wall is that far past the road edge (the node's barrier byte is the verge byte plus it) */
    TRACK_GEN_SIDE_WALL = 1 << 3,       /* `wall`: its kind, the side's nibble of node byte 5 (the left's high, the right's low) */
    TRACK_GEN_SIDE_FENCE = 1 << 4       /* `fence`: 1 a fence on that side, 0 none; a segment (4 nodes) takes its first node's (TRI_FENCE_LEFT / _RIGHT) */
};
enum { TRACK_GEN_WALL_HARD = 0, TRACK_GEN_WALL_SOFT = 1 };   /* hard: the game's hit, with its sound and a race note; soft: a glancing one */
/* lanes (step 4), the CD's three (the survey): one lane each way, two own lanes (the extra one on the right), two oncoming (on the left); a lane is 5 m */
enum { TRACK_GEN_LANES_1_1 = 0x11, TRACK_GEN_LANES_1_2 = 0x12, TRACK_GEN_LANES_2_1 = 0x21, TRACK_GEN_LANE = 40 };
/* The fence's look on a node (the fence step): the game has one per segment for both sides, its height and its picture (the fence byte's bit 5 and low 5
 * bits); a segment takes its first node's. `has` says which the node sets; all zero is the template's. */
enum { TRACK_GEN_FENCE_HEIGHT = 1 << 0, TRACK_GEN_FENCE_SLOT = 1 << 1 };
enum { TRACK_GEN_FENCE_SLOTS = 32 };   /* the picture is slot 32 + slot, slot 0..31 */
typedef struct {
    unsigned has;   /* TRACK_GEN_FENCE_* bits */
    int tall;       /* TRACK_GEN_FENCE_HEIGHT: 1 tall (2 m), 0 low (0.9 m) */
    int slot;       /* TRACK_GEN_FENCE_SLOT: 0..TRACK_GEN_FENCE_SLOTS - 1 */
} TrackGenFence;
#define TRACK_GEN_SIDE_MAX 160.0   /* the most a shoulder or a wall's distance may be, in the verge byte's 1/8 m: 20 m */
typedef struct {
    unsigned has;         /* TRACK_GEN_SIDE_* bits */
    int tarmac;           /* TRACK_GEN_SIDE_TARMAC: 1 the shoulder beyond the verge edge is the road's own surface (the game's row 0: that side's nibble of the node's
                           * flags[2], node +6, is 0), 0 the template's verge */
    double shoulder;      /* 1/8 m past the road edge, 0..TRACK_GEN_SIDE_MAX (the generic template's is 24: 3 m) */
    double barrier;       /* 1/8 m past the road edge, 0..TRACK_GEN_SIDE_MAX, and the verge byte plus it at most 255 (the generic template's is 24) */
    int wall;             /* TRACK_GEN_WALL_HARD or _SOFT */
    int fence;            /* 0 or 1 */
} TrackGenSide;

/* A node of the track as the generator writes it: the shape the game reads (headings 14-bit, positions 16.16) before it is packed. The first node
 * is the start; the last is the closing node (a track of N segments has 4N + 1). */
typedef struct {
    double heading;       /* 14-bit units (0x4000 = full turn), 0 along +z, positive to the right */
    double grade;         /* percent, +-30 */
    double bank;          /* degrees, +-TRACK_GEN_BANK_MAX_DEGREES turned, +-TRACK_GEN_LAYOUT_BANK_MAX_DEGREES otherwise */
    double width;         /* the half road width in the node's verge byte, 0..255 (16 terrain units each); a layout asks for 8..120 */
    int32_t position[3];  /* x, y, z in 16.16; consecutive nodes 1..20 units apart */
    TrackGenSide side[2]; /* [TRACK_GEN_LEFT], [TRACK_GEN_RIGHT]; all zero: the template's */
    int lanes;            /* step 4: flags[0] (node +4), oncoming << 4 | own: TRACK_GEN_LANES_1_1, _1_2 or _2_1; 0 the template's. A segment's strips take its first node's */
    double width_r;       /* step 4: the right half width in the verge byte, 0..255; 0 the same as `width` (the left one) */
    TrackGenFence fence;  /* the fence's height and picture, both sides'; all zero: the template's */
} TrackGenNode;

enum { TRACK_GEN_MAX_NODES = TRACK_GEN_MAX_SEGMENTS * 4 + 1 };

/* The nodes of a layout (`start_width` the width it starts from: the template's), positions integrated along the headings: 4 per segment and
 * the closing node in *nodes (malloc'd, *count). 0 with err naming the layout line otherwise. */
int track_layout_nodes(const char *layout, size_t layout_len, double start_width, TrackGenNode **nodes, int *count, ExchangeError *err);

/* The TRI of explicit nodes: `count` is 4 per segment and one more, 8..599 segments; each node finite, its width (0..255), grade and bank in range, its
 * heading at most TRACK_GEN_LAYOUT_TURN_MAX_DEGREES (8) from the node before, its distance from it 1..20 units. 1 and *out (malloc'd, *out_len bytes); 0 with err naming
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

/* What track_generate_nodes_turned refuses before it writes a byte (the node count, each node, the template, the barriers' bytes), in its words:
 * 1 when it would build these nodes. Cheap enough for every edit (the editor's Problems shows a refusal before Build). */
int track_generate_check_turned(const TrackGenNode *nodes, int count, const uint8_t *template_tri, size_t template_size, ExchangeError *err);

/* 1 and *out (malloc'd, *out_len bytes) on success; 0 with err naming the layout line otherwise. */
int track_generate(const char *layout, size_t layout_len, const uint8_t *template_tri, size_t template_size,
                   uint8_t **out, size_t *out_len, ExchangeError *err);

#endif

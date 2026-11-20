/* Track trace: a centre line drawn as control points becomes the nodes track_generate_nodes writes (the editor's road, phase 1a of
 * notes/dep59-track-editor-design.md). Pure: no files, no window.
 *
 * The points (x, z in game units, 1 unit about 1 m; the road starts at the first point heading towards the second) are joined by a
 * uniform Catmull-Rom curve, which goes through every point, and the curve is resampled by arc length: 4 nodes per segment and one
 * more, the spacing being the CD's (TRACK_GEN_NODE_LENGTH, about 6 units). Heights and banks of the points are not used yet: every node is level
 * and unbanked. The width of the road is the project's, or the road keys' (a TraceKey on a control point, the plan's step 1): between two keys it is
 * a smoothstep over the arc length.
 *
 * The limits the editor shows (track_trace_flags), in two levels (notes/dep59-editor-limits-vs-cd.md). Up to 8 degrees a node (a radius of 43 units) nothing is
 * marked. AMBER, a warning that the road is built: over TRACK_GEN_TURN_WARN_DEGREES, or under TRACE_RADIUS_WARN; the CD has such curves (Alpine 3
 * turns 16 degrees a node) and the generator holds the terrain on the inside of the curve (TRACK_GEN_TERRAIN_REACH), but the game's own drawing and the AI
 * have to be checked on the spot. RED, where the road is not built: over TRACK_GEN_TURN_MAX_DEGREES, or under TRACE_RADIUS_MIN. The node count
 * and the road's length are checked by track_trace_nodes. */
#ifndef TNFS_TRACK_TRACE_H
#define TNFS_TRACK_TRACE_H

#include "formats/exchange.h"
#include "formats/track_gen.h"

#include <stddef.h>
#include <stdint.h>

#define TRACE_TURN_MAX_DEGREES TRACK_GEN_TURN_MAX_DEGREES   /* red: the most a node turns from the node before (track_gen's own limit) */
#define TRACE_RADIUS_MIN 20.9        /* red, units: a curve tighter than this is not built (the radius of TRACK_GEN_TURN_MAX_DEGREES on the 6.03-unit spacing) */
#define TRACE_TURN_WARN_DEGREES TRACK_GEN_TURN_WARN_DEGREES   /* amber: over this a node turns more than ours drew clean without the terrain held */
#define TRACE_RADIUS_WARN 43.0       /* amber, units: under this (8 degrees a node) */
/* One track unit is one metre: the game shows a car's speed as its 16.16 whole part x 2.24 in mph and x 3.6 in km/h (game/frontend/stat_text.c), so a
 * speed unit is a metre a second, and its gravity is 9.826 units/s2 (game/race/car_vertical.c, GRAVITY), the real 9.81 m/s2 to 0.2 per cent
 * (notes/dep59-editor-limits-vs-cd.md). Every distance the editor shows is TRACE_METRES(units): this is the only place the factor lives. */
#define TRACE_METRES_PER_UNIT 1.0
#define TRACE_METRES(units) ((units) * TRACE_METRES_PER_UNIT)
#define TRACE_STEP_MIN 0.5           /* units; two control points closer than this are the same point */
#define TRACE_COORD_MAX 20000.0      /* units from the origin: inside the 16.16 positions with room for the sums */

typedef struct {
    double x, z;
} TracePoint;

enum {
    TRACE_FLAG_TURN = 1,         /* red: the node turns more than TRACE_TURN_MAX_DEGREES from the node before */
    TRACE_FLAG_RADIUS = 2,       /* red: the curve's radius at the node is under TRACE_RADIUS_MIN */
    TRACE_FLAG_TURN_WARN = 4,    /* amber: it turns more than TRACE_TURN_WARN_DEGREES (and no more than the red limit) */
    TRACE_FLAG_RADIUS_WARN = 8,  /* amber: its radius is under TRACE_RADIUS_WARN (and not under the red one) */
    TRACE_FLAG_WIDTH_WARN = 16   /* amber: the road's width changes by more than TRACE_WIDTH_STEP_WARN_M from the node before */
};

/* The road width in the editor's units: the byte track_gen writes is the distance of each road edge from the centre in 1/8 m, so the road edge to edge is
 * byte / 4 metres (the template's 40 is 10 m). The editor's range is 2..30 m (8..120). */
#define TRACE_WIDTH_PER_METRE 4.0
#define TRACE_ROAD_MIN_M 2.0
#define TRACE_ROAD_MAX_M 30.0
#define TRACE_WIDTH_STEP_WARN_M 1.25   /* amber: the road's width changes by more than this (5 bytes) from one node to the next */

/* A road key: values a control point sets for the road from there on (the plan's step 1 does the width; the bits are the room for the next steps:
 * bank, lanes, surface, shoulder, pace, each its own bit and its own field, so a file or a list with only some of them reads as before). A value not
 * set (its bit clear) is not the key's: the point is inheriting. Keys are kept by point, ascending. */
enum { TRACE_KEY_WIDTH = 1 };
typedef struct {
    int point;          /* the control point it is on, 0..count-1 */
    unsigned has;       /* TRACE_KEY_* bits: which values this key sets */
    double width_m;     /* TRACE_KEY_WIDTH: the road's width at the point, edge to edge, in metres (TRACE_ROAD_MIN_M..MAX_M) */
} TraceKey;

/* The length of the centre line through `points` (the curve, not the chords), in units; 0 for fewer than 2 points. */
double track_trace_length(const TracePoint *points, int count);

/* The segments (4 nodes each) a road of `length` units has: the nearest to length / (4 x the node spacing), at least the generator's
 * minimum. The node count is 4 x this + 1. */
int track_trace_segments(double length);

/* The nodes of the centre line through `points`: a node per spacing along the curve, each heading along it, level, unbanked, `width` wide
 * (the verge byte of track_gen). *nodes is malloc'd, *node_count its length. 0 with err naming the fault otherwise: fewer than 2 points,
 * a point not finite or too far from the origin, two points on top of each other, or a road over TRACK_GEN_MAX_SEGMENTS segments. */
int track_trace_nodes(const TracePoint *points, int count, double width, TrackGenNode **nodes, int *node_count, ExchangeError *err);

/* The same with road keys: `width` (the byte) is the road's width at the start, a key with TRACE_KEY_WIDTH sets it at its point, and between two of them
 * (the start counting as one at point 0 when no key is there) the width is a smoothstep over the arc length, so the step from node to node stays small
 * (at most 1.5 times the average: D bytes over L metres on 6.25 m nodes steps 1.5 x D x 6.25 / L); after the last key it holds. With no keys it is
 * track_trace_nodes. A key off the points, twice on a point or outside TRACE_ROAD_MIN_M..MAX_M fails with err naming it. */
int track_trace_nodes_keyed(const TracePoint *points, int count, double width, const TraceKey *keys, int key_count, TrackGenNode **nodes,
                            int *node_count, ExchangeError *err);

/* The road's width at each control point in metres, as the keys make it: the key's own where a point has one, else what the smoothstep gives there (the
 * Points table shows those dimmed). `metres` has `count` values; 0 values when the curve cannot be made. */
void track_trace_point_widths(const TracePoint *points, int count, double width, const TraceKey *keys, int key_count, double *metres);

/* The key list's edits: `*keys` is malloc'd, `*count` long, `*cap` big. Set the width of the key on `point` (a new key keeps the list ascending, an old
 * one is updated; 0 when out of memory); clear it (the key goes when no other value is set); the position of the key on `point` or -1; and keep the keys on
 * their points when a point is inserted (those from `index` on move up) or removed (its key goes, the later ones move down). */
int track_trace_key_set_width(TraceKey **keys, int *count, int *cap, int point, double metres);
void track_trace_key_clear_width(TraceKey *keys, int *count, int point);
int track_trace_key_find(const TraceKey *keys, int count, int point);
void track_trace_keys_insert_point(TraceKey *keys, int count, int index);
void track_trace_keys_remove_point(TraceKey *keys, int *count, int index);

/* Per node (flags[i], for i < count): the TRACE_FLAG_* bits that node breaks, a warning bit only where its red one is not set. flags[0] is always 0 (the start has no node before it). */
void track_trace_flags(const TrackGenNode *nodes, int count, uint8_t *flags);

/* A node that breaks a limit, as the Problems panel lists it. `flags` are its TRACE_FLAG_* bits, `red` is set when any of them is red,
 * `turn` is its turn from the node before in degrees (unsigned) and `radius` the curve's radius there in units (0 on a straight). */
typedef struct {
    int node;
    int red;
    uint8_t flags;
    double turn, radius;
    double width_step;   /* TRACE_FLAG_WIDTH_WARN: how much the road's width changes from the node before, in metres (unsigned; 0 otherwise) */
} TraceProblem;

/* The problem nodes: every node whose flags are not 0, red ones first and each group by node number. Fills out[0..cap) and returns how many
 * nodes there are in all (it may be more than cap). `flags` are track_trace_flags'. */
int track_trace_problems(const TrackGenNode *nodes, int count, const uint8_t *flags, TraceProblem *out, int cap);

/* One row's text in English, the limits written from the constants: "node 80: turns 9.4° (more than 8°), may not look right", or with both rules
 * "node 80: turns 9.4° (more than 8°) and radius 38 units (under 43 units), may not look right"; a red row ends "can't be built". */
void track_trace_problem_text(const TraceProblem *p, char *out, size_t cap);

/* The summary of a road's flags, one line per rule some node breaks (a rule no node breaks has none), red lines first: "2 nodes turn more
 * than 16.5° per node: the track can't be built", "26 nodes turn more than 8° per node: they may not look right in the game", and the same for
 * the radius ("1 node is on a curve of radius under 43 units: it may not look right in the game"), and the width, last
 * ("3 nodes change width by more than 1.25 m per node: they may not look right in the game"). */
enum { TRACE_SUMMARY_LINES = 5, TRACE_SUMMARY_TEXT = 128, TRACE_PROBLEM_TEXT = 192 };   /* TRACE_PROBLEM_TEXT: room for a row with all three amber clauses */
typedef struct {
    int red;
    int nodes;
    char text[TRACE_SUMMARY_TEXT];
} TraceSummaryLine;
int track_trace_summary(const uint8_t *flags, int count, TraceSummaryLine lines[TRACE_SUMMARY_LINES]);

/* Metres from node 0 to each node along the road (the sum of the distances between consecutive nodes: m[0] = 0), `count` values. */
void track_trace_distances(const TrackGenNode *nodes, int count, double *metres);

/* The turn of node i from the node before it, in degrees: positive to the right, negative to the left (0 for node 0). */
double track_trace_signed_turn(const TrackGenNode *nodes, int i);

/* The distance in metres from node i-1 to node i (0 for node 0). */
double track_trace_step(const TrackGenNode *nodes, int i);

/* Metres from the start of the curve to each of the `count` control points, along the curve (not the chords); 0 values when it cannot be made. */
void track_trace_point_distances(const TracePoint *points, int count, double *metres);

/* A signed turn as the editor writes it: "R 5.3°" to the right, "L 3.1°" to the left, "-" under a twentieth of a degree. */
void track_trace_turn_text(double degrees, char *out, size_t cap);

/* The signed turn from `from` to `to` (14-bit headings), in degrees, wrapped to -180..180. */
double track_trace_turn_degrees(double from, double to);

#endif

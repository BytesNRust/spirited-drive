/* Track trace: a centre line drawn as control points becomes the nodes track_generate_nodes writes (the editor's road, phase 1a of
 * notes/dep59-track-editor-design.md). Pure: no files, no window.
 *
 * The points (x, z in game units, 1 unit about 1 m; the road starts at the first point heading towards the second) are joined by a
 * uniform Catmull-Rom curve, which goes through every point, and the curve is resampled by arc length: 4 nodes per segment and one
 * more, the spacing being the CD's (TRACK_GEN_NODE_LENGTH, about 6 units). Heights, banks and widths of the points are not used yet:
 * every node is level, unbanked and of the same width.
 *
 * The limits the editor shows (track_trace_flags): a node turns more than TRACE_TURN_MAX_DEGREES from the node before, or the curve's
 * radius there is under TRACE_RADIUS_MIN. The node count and the road's length are checked by track_trace_nodes. */
#ifndef TNFS_TRACK_TRACE_H
#define TNFS_TRACK_TRACE_H

#include "formats/exchange.h"
#include "formats/track_gen.h"

#include <stdint.h>

#define TRACE_TURN_MAX_DEGREES 6.0   /* the most a node turns from the node before (track_gen's limit) */
#define TRACE_RADIUS_MIN 60.0        /* units; a curve tighter than this is not admitted (spike: the CD's tightest sustained curve is ~36) */
#define TRACE_STEP_MIN 0.5           /* units; two control points closer than this are the same point */
#define TRACE_COORD_MAX 20000.0      /* units from the origin: inside the 16.16 positions with room for the sums */

typedef struct {
    double x, z;
} TracePoint;

enum {
    TRACE_FLAG_TURN = 1,      /* the node turns more than TRACE_TURN_MAX_DEGREES from the node before */
    TRACE_FLAG_RADIUS = 2     /* the curve's radius at the node is under TRACE_RADIUS_MIN */
};

/* The length of the centre line through `points` (the curve, not the chords), in units; 0 for fewer than 2 points. */
double track_trace_length(const TracePoint *points, int count);

/* The segments (4 nodes each) a road of `length` units has: the nearest to length / (4 x the node spacing), at least the generator's
 * minimum. The node count is 4 x this + 1. */
int track_trace_segments(double length);

/* The nodes of the centre line through `points`: a node per spacing along the curve, each heading along it, level, unbanked, `width` wide
 * (the verge byte of track_gen). *nodes is malloc'd, *node_count its length. 0 with err naming the fault otherwise: fewer than 2 points,
 * a point not finite or too far from the origin, two points on top of each other, or a road over TRACK_GEN_MAX_SEGMENTS segments. */
int track_trace_nodes(const TracePoint *points, int count, double width, TrackGenNode **nodes, int *node_count, ExchangeError *err);

/* Per node (flags[i], for i < count): the TRACE_FLAG_* bits that node breaks. flags[0] is always 0 (the start has no node before it). */
void track_trace_flags(const TrackGenNode *nodes, int count, uint8_t *flags);

/* The signed turn from `from` to `to` (14-bit headings), in degrees, wrapped to -180..180. */
double track_trace_turn_degrees(double from, double to);

#endif

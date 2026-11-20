/* The track editor's Add curve and Add straight (dep88): the points of an arc or a straight, from (0, 0) along +z, which pts_paste (point_ops.h)
 * then puts in after a point, going on along the road's heading there. Every point lies on the arc; the turn is to the right or left as the
 * view shows it (z up). Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_ARC_BLOCK_H
#define TNFS_TRACK_EDITOR_ARC_BLOCK_H

#include "formats/track_trace.h"

#define ARC_DEGREES_MAX 180.0
#define ARC_POINTS_MAX 40
#define ARC_CHORD_MAX_M 40.0      /* the default count keeps each chord this short ... */
#define ARC_CHORD_TURN_MAX 15.0   /* ... and turning this little */
#define STRAIGHT_CHORD_MAX_M 100.0
#define STRAIGHT_LENGTH_MAX_M 2000.0

/* out[0..count]: (0, 0), then `count` points along an arc turning `right` (1) or left (0) by `degrees` (0..ARC_DEGREES_MAX] with a radius of
 * `radius_m` metres, as many degrees apart. 0 (out untouched) when an argument is out of range (count 1..ARC_POINTS_MAX). */
int arc_block(int right, double degrees, double radius_m, int count, TracePoint *out);
/* out[0..count]: (0, 0), then `count` points `length_m` / count apart along +z (length 0..STRAIGHT_LENGTH_MAX_M]. 0 when out of range. */
int straight_block(double length_m, int count, TracePoint *out);

/* an arc's heading at its end, as a unit vector (it starts along +z): what pts_paste_tangents wants */
TracePoint arc_end_heading(int right, double degrees);
/* the fewest points with chords no longer than ARC_CHORD_MAX_M and turns no more than ARC_CHORD_TURN_MAX (STRAIGHT_CHORD_MAX_M on a straight) */
int arc_default_count(double degrees, double radius_m);
int straight_default_count(double length_m);
/* the arc's length in metres */
double arc_length_m(double degrees, double radius_m);

#endif

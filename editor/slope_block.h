/* The track editor's slope tool (dep95 step 9): the heights of a run of control points that climbs or falls by a grade or a rise, with the grade eased in from
 * the road's own grade at the first point and out to the one it has after the last. The grade along the run is
 *     g(s) = g0 (1 - in(s)) + g1 (1 - out(s)) + G (in(s) + out(s) - 1),     in(s) = smoothstep(s / ease_in), out(s) = smoothstep((L - s) / ease_out)
 * (a smoothstep of a zero length is 1), so it is g0 at the start, g1 at the end and G between the easings; the height is its integral. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_SLOPE_BLOCK_H
#define TNFS_TRACK_EDITOR_SLOPE_BLOCK_H

#define SLOPE_GRADE_MAX_PCT 100.0   /* a grade beyond this is refused (the generator's own limit is lower: the preview says so) */
#define SLOPE_RISE_MAX_M 500.0      /* the trace's own height limit */

/* out[0..count-1]: the heights at the distances dist[0..count-1] (metres along the road, strictly increasing; the first is the run's start) over a run starting at
 * start_h. g0_pct and g1_pct are the road's grade before and after the run. value is the grade G in percent (by_rise 0) or the total rise in metres (by_rise 1: G
 * follows, and is written to *grade_pct when it is not NULL). 1 when made; 0 (out untouched) with fewer than 2 points, distances that do not rise, an easing that is
 * negative or longer than the run together, a grade or rise beyond its maximum or a number that is not finite. */
int slope_block(const double *dist, int count, double start_h, double g0_pct, double g1_pct, int by_rise, double value, double ease_in_m, double ease_out_m, double *out,
                double *grade_pct);

#endif

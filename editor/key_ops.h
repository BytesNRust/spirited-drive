/* The road profile keys of a run of control points, for point_ops.h's operations (dep87 1b): copied with the points, pasted with them, mirrored
 * (sides swapped) and reversed (travelled the other way), each value still covering the same stretch of road. A value is ramped (the width, the
 * bank, the shoulder and barrier distances: a smoothstep from key to key) or held (the shoulder surface, the wall's kind, the fence, the lanes:
 * from its key to the next). Keys pin what the keys made (for the values some key sets) on the run's first point (all values) and last
 * (ramped), on the points just before and after it (ramped) and on the tail's first segment (held). Outside the run every segment keeps its held
 * values, and the keyed points and those two neighbours their ramped ones; a ramp running into the run from a key further out takes a new shape
 * (the points between move, staying between its ends). Each result is a new malloc'd key list, ascending; the input is left alone. Pure C. */
#ifndef TNFS_TRACK_EDITOR_KEY_OPS_H
#define TNFS_TRACK_EDITOR_KEY_OPS_H

#include "project.h"

#define KEY_RAMPED (TRACE_KEY_WIDTH | TRACE_KEY_BANK | TRACE_KEY_SIDE(TRACE_SIDE_SHOULDER, 0) | TRACE_KEY_SIDE(TRACE_SIDE_SHOULDER, 1) | \
                    TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, 0) | TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, 1))
#define KEY_HELD (TRACE_KEY_SHOULDER_SURFACE | TRACE_KEY_SIDE(TRACE_SIDE_WALL, 0) | TRACE_KEY_SIDE(TRACE_SIDE_WALL, 1) | \
                  TRACE_KEY_SIDE(TRACE_SIDE_FENCE, 0) | TRACE_KEY_SIDE(TRACE_SIDE_FENCE, 1) | TRACE_KEY_LANES | TRACE_KEY_FENCE_HEIGHT | \
                  TRACE_KEY_FENCE_ART | TRACE_KEY_BARRIER_CUSTOM(0) | TRACE_KEY_BARRIER_CUSTOM(1))

/* what the keys make at every control point, every value (track_trace_point_*) */
typedef struct {
    int count;
    unsigned used;           /* the bits some key sets, each side's with its partner's */
    double *width_m, *bank_deg;
    double (*side)[TRACE_SIDE_FIELDS][2];
    TraceSurface *surface[2];
    int *lanes;
    TraceFencePoint *fence;  /* the fence's height and picture (the road's) and each side's barrier Custom */
} KeyState;

int key_state_make(const Trace *t, KeyState *s);   /* 0 when memory is out */
void key_state_free(KeyState *s);
/* a key on point i setting the values of `mask` as they are there (a width is left out where a lane stretch would ignore it) */
TraceKey key_state_at(const KeyState *s, int i, unsigned mask);

/* k seen from the other side: left and right swapped (a barrier's Custom with its distance: a side whose Custom is off ignores it), the bank's sign
 * turned, lanes 1+2 and 2+1 swapped; the fence's height and picture are the road's and stay */
TraceKey key_swapped(TraceKey k);
/* whether a and b set the same values of `mask` (both set or both not, and equal where set) */
int key_same(const TraceKey *a, const TraceKey *b, unsigned mask);

/* the keys of points s..e, on 0..e-s: the first pins every value used, the last pins the ramped ones (its held ones would be the tail's) */
int keys_clip(const Trace *t, int s, int e, TraceKey **out, int *count);
/* t's keys for pts_paste(t, anchor, block of m): the clip's on anchor..anchor+m-1 (over the anchor's own), the tail's moved on and its held
 * values pinned where it now starts */
int keys_paste(const Trace *t, int anchor, const TraceKey *clip, int clip_count, int m, TraceKey **out, int *count);
/* t's keys for pts_mirror_*(t, s, e): the run's sides swapped (front/back too: the road is travelled the same way, its sides change over) */
int keys_mirror(const Trace *t, int s, int e, TraceKey **out, int *count);
/* t's keys for pts_reverse(t, s, e): the run's values on the mirror-image points, sides swapped */
int keys_reverse(const Trace *t, int s, int e, TraceKey **out, int *count);

#endif

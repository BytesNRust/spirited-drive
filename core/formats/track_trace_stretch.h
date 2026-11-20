/* Colour by (dep95, the Viewport and the height profile): what each node of the road is, in one mode, as a small class number, and the runs of nodes that
 * share a class (a stretch). Pure: it reads the nodes the generator made, so the curve, the bank and the widths are the road's own. The colours are the
 * editor's (editor/stretch_colours.h); this file only says which class a node is in. */
#ifndef TNFS_FORMATS_TRACK_TRACE_STRETCH_H
#define TNFS_FORMATS_TRACK_TRACE_STRETCH_H

#include "formats/track_gen.h"

#include <stdint.h>

/* TRACE_COLOUR_LIMITS is the editor's own colouring (red and amber over a limit): its nodes are all class 0. A mode added later (GRADE) goes before
 * TRACE_COLOUR_MODES and takes its classes in track_trace_stretch.c and its colours in editor/stretch_colours.h, nothing else. */
typedef enum {
    TRACE_COLOUR_LIMITS = 0,
    TRACE_COLOUR_CURVE,      /* TRACE_CURVE_* */
    TRACE_COLOUR_BANK,       /* 0 flat, 1..3 the bank's degrees up in three steps, 4..6 the same the other way (the node's own sign) */
    TRACE_COLOUR_WIDTH,      /* TRACE_WIDTH_BANDS bands of the road's width, narrow first */
    TRACE_COLOUR_LANES,      /* 0 one each way (or the template's), 1 one lane out and two back (TRACK_GEN_LANES_1_2), 2 the other (_2_1) */
    TRACE_COLOUR_SHOULDER,   /* bit 0 the left shoulder is tarmac, bit 1 the right one */
    TRACE_COLOUR_FENCE,      /* 0 the template's fence, else 1 + 2 * picture (TRACE_FENCE_*) + tall */
    TRACE_COLOUR_MODES
} TraceColourBy;

enum { TRACE_CURVE_STRAIGHT = 0, TRACE_CURVE_LEFT, TRACE_CURVE_RIGHT, TRACE_CURVE_CLASSES };
enum { TRACE_WIDTH_BANDS = 6, TRACE_BANK_CLASSES = 7, TRACE_FENCE_CLASSES = 7 };

#define TRACE_STRAIGHT_RADIUS_M 2000.0   /* a curve of at least this radius is a straight */
#define TRACE_BANK_FLAT_DEG 1.0          /* under this a node is flat; each step of the bank is TRACE_BANK_STEP_DEG more */
#define TRACE_BANK_STEP_DEG 10.0

/* how many classes a mode has (they are 0 .. n - 1); 1 for TRACE_COLOUR_LIMITS */
int track_trace_class_count(TraceColourBy mode);

/* a class's words for a tooltip ("straight", "left", "right", "banked 1-10 deg one way", ...); "" for a class out of range */
const char *track_trace_class_name(TraceColourBy mode, int klass);

/* out[k] is node k's class in `mode`. The curve is read over a few nodes either side (the headings are 14-bit, one step is a hair off a straight):
 * its radius from the heading change and the road length between them. */
void track_trace_node_classes(const TrackGenNode *nodes, int count, TraceColourBy mode, uint8_t *out);

/* a run of nodes first..last (both in it) of one class */
typedef struct {
    int first, last, klass;
} TraceStretch;

/* the runs of equal class, in order; returns how many there are, of which the first `cap` are written (cap 0 or out NULL just counts) */
int track_trace_stretches(const uint8_t *classes, int count, TraceStretch *out, int cap);

/* the run that holds `node`: first and last set, 1; 0 when node is out of 0..count-1 */
int track_trace_stretch_of(const uint8_t *classes, int count, int node, int *first, int *last);

#endif

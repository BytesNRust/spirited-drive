/* The editor's top view: world (x, z) to window pixels and back, with x to the right and z up (north is +z, the road's heading 0). */
#ifndef TNFS_TRACK_EDITOR_VIEW_H
#define TNFS_TRACK_EDITOR_VIEW_H

#include "formats/track_trace.h"

typedef struct {
    double cx, cz;        /* the world point at the centre of the window */
    double scale;         /* pixels per unit */
    int width, height;    /* the window, in pixels */
} View;

enum { VIEW_MARGIN = 60, VIEW_MIN_SPAN = 100 };
#define VIEW_ZOOM_STEP 1.15
#define VIEW_SCALE_MIN 0.05
#define VIEW_SCALE_MAX 40.0
#define VIEW_FOCUS_SCALE 1.0   /* pixels per metre the Distance strip's double-click brings the view in to, at least */

/* the view that shows every point of the trace, with a margin */
void view_fit(View *v, const TracePoint *points, int count, int width, int height);

void view_to_screen(const View *v, double x, double z, double *sx, double *sy);
void view_to_world(const View *v, double sx, double sy, double *x, double *z);

/* the window centred on world (x, z), zoomed in to at least `min_scale` (a view already closer keeps its zoom; never past VIEW_SCALE_MAX) */
void view_centre_on(View *v, double x, double z, double min_scale);

/* the point nearest to the window point (sx, sy) within `reach` pixels; -1 when none is that near */
int view_pick(const View *v, const TracePoint *points, int count, double sx, double sy, double reach);

#endif

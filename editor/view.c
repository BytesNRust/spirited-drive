#include "view.h"

#include <math.h>

void view_fit(View *v, const TracePoint *points, int count, int width, int height)
{
    double min_x = 0, max_x = 0, min_z = 0, max_z = 0;
    for (int i = 0; i < count; ++i) {
        if (i == 0 || points[i].x < min_x) min_x = points[i].x;
        if (i == 0 || points[i].x > max_x) max_x = points[i].x;
        if (i == 0 || points[i].z < min_z) min_z = points[i].z;
        if (i == 0 || points[i].z > max_z) max_z = points[i].z;
    }
    const double span_x = fmax(max_x - min_x, VIEW_MIN_SPAN), span_z = fmax(max_z - min_z, VIEW_MIN_SPAN);
    const double room_x = fmax(width - 2 * VIEW_MARGIN, 1), room_z = fmax(height - 2 * VIEW_MARGIN, 1);
    v->width = width;
    v->height = height;
    v->cx = (min_x + max_x) / 2;
    v->cz = (min_z + max_z) / 2;
    v->scale = fmin(room_x / span_x, room_z / span_z);
    v->scale = fmin(fmax(v->scale, VIEW_SCALE_MIN), VIEW_SCALE_MAX);
}

void view_to_screen(const View *v, double x, double z, double *sx, double *sy)
{
    *sx = v->width / 2.0 + (x - v->cx) * v->scale;
    *sy = v->height / 2.0 - (z - v->cz) * v->scale;
}

void view_to_world(const View *v, double sx, double sy, double *x, double *z)
{
    *x = v->cx + (sx - v->width / 2.0) / v->scale;
    *z = v->cz - (sy - v->height / 2.0) / v->scale;
}

int view_pick(const View *v, const TracePoint *points, int count, double sx, double sy, double reach)
{
    int best = -1;
    double best_d = reach;
    for (int i = 0; i < count; ++i) {
        double px, py;
        view_to_screen(v, points[i].x, points[i].z, &px, &py);
        const double d = hypot(px - sx, py - sy);
        if (d <= best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

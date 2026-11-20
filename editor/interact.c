#include "interact.h"

#include "app_select.h"

void interact_nudge(App *a, double dx, double dz)
{
    app_sel_move(a, dx, dz);   /* every selected point (dep87) */
}

void interact_zoom(App *a, double sx, double sy, double factor)
{
    double before_x, before_z, after_x, after_z;
    view_to_world(&a->view, sx, sy, &before_x, &before_z);
    const double scale = a->view.scale * factor;
    a->view.scale = scale < VIEW_SCALE_MIN ? VIEW_SCALE_MIN : scale > VIEW_SCALE_MAX ? VIEW_SCALE_MAX : scale;
    view_to_world(&a->view, sx, sy, &after_x, &after_z);
    a->view.cx += before_x - after_x;
    a->view.cz += before_z - after_z;
}

void interact_fit(App *a)
{
    view_fit(&a->view, a->trace.points, a->trace.count, a->view.width, a->view.height);
}

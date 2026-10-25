/* port (render scale): the viewport of a frame s times as large (ported_view.h). */
#include "game/viewer/ported_view.h"

PortedViewport ported_view_scaled(const PortedViewport *v, int s)
{
    PortedViewport out = *v;
    if (s < 1) s = 1;
    out.left *= s, out.top *= s, out.width *= s, out.height *= s;
    out.sky_left *= s, out.sky_top *= s, out.sky_width *= s, out.sky_height *= s;
    out.render_scale = v->render_scale * s;
    return out;
}

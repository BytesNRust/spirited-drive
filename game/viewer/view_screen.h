/* the frame the 3D view draws into and where the view sits on it (view_screen.c); ported_view.c reads them, ported_view_set_screen and
 * ported_view_set_viewport (ported_view.h) set them */
#ifndef TNFS_VIEW_SCREEN_H
#define TNFS_VIEW_SCREEN_H

#include "viewer/ported_view.h"

#include <stdint.h>

/* the frame and the chase views' field the 3D view starts from (ported_view.c and view_screen.c) */
enum { WIDTH = 640, HEIGHT = 480, FOV = 96 /* 004b78a8, view 1 */ };

extern int32_t view_screen_w, view_screen_h;   /* the frame: 640 x 480, or a wide screen's */
extern PortedViewport view_viewport;           /* the camera row's box and fov; the whole frame at fov 96 by default */

#endif

/* port (gauge_wheel): the part of the steering wheel that is in front of the gauges. The dash's light at night (drive_wide.c compose_classes) marks the pixels of the
 * two gauge boxes' ellipses as lit (the dials shine); the wheel passing in front of them is drawn by the 't' and 's' frames of the steering frame (cockpit_frame.c gauges),
 * over the dials, and took the same mark, so a wheel that is dark outside the gauges shone inside them. These tell which pixels those frames drew. */
#ifndef TNFS_WHEEL_FRONT_H
#define TNFS_WHEEL_FRONT_H

#include "game/race/cockpit.h"

#include <stdint.h>

/* the 't' (which 0) or 's' (which 1) frame drawn over the gauges for the steering frame `shown` (the signed frame hud 004f9a20 keeps), as cockpit_frame picks it:
 * the left ones when the steering is negative on a plain track or positive on a mirrored one. NULL when the frame is out of range or missing. */
const uint8_t *wheel_front_shape(const Cockpit *c, int which, int32_t shown, int steering_negative, int mirrored);
/* 1 when the shape draws a pixel (not its see-through 0xff) at (x, y) of the 640 view, where its header places it */
int wheel_front_pixel(const uint8_t *shape, int x, int y);

#endif

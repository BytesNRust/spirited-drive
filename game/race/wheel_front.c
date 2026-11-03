/* port: see wheel_front.h. */
#include "game/race/wheel_front.h"

enum { SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_X = 0xc, SHAPE_Y = 0xe, SHAPE_PIXELS = 0x10, SEE_THROUGH = 0xff };   /* an FSH shape's header (blit.c) */

static int s16(const uint8_t *p) { return (int16_t)(uint16_t)(p[0] | p[1] << 8); }

const uint8_t *wheel_front_shape(const Cockpit *c, int which, int32_t shown, int steering_negative, int mirrored)
{
    const int32_t frame = shown < 0 ? -shown : shown;
    if (c == NULL || frame < 0 || frame >= COCKPIT_FRAMES) return NULL;
    const int left = steering_negative != (mirrored != 0);
    const int row = which == 0 ? (left ? 2 : 4) : (left ? 3 : 5);   /* 't'l, 's'l, 't'r, 's'r */
    return c->frames[row][frame];
}

int wheel_front_pixel(const uint8_t *shape, int x, int y)
{
    if (shape == NULL) return 0;
    const int left = s16(shape + SHAPE_X), top = s16(shape + SHAPE_Y), w = s16(shape + SHAPE_WIDTH), h = s16(shape + SHAPE_HEIGHT);
    if (w <= 0 || h <= 0 || x < left || x >= left + w || y < top || y >= top + h) return 0;
    return shape[SHAPE_PIXELS + (y - top) * w + (x - left)] != SEE_THROUGH;
}

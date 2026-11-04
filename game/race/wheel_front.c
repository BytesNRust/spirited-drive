/* port: see wheel_front.h. */
#include "game/race/wheel_front.h"

#include <string.h>

enum { SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_X = 0xc, SHAPE_Y = 0xe, SHAPE_PIXELS = 0x10, SEE_THROUGH = 0xff };   /* an FSH shape's header (blit.c) */

static int s16(const uint8_t *p) { return (int16_t)(uint16_t)(p[0] | p[1] << 8); }

const uint8_t *wheel_front_shape(const Cockpit *c, int which, int32_t shown, int steering_negative, int mirrored)
{
    const int32_t frame = shown < 0 ? -shown : shown;
    if (c == NULL || frame < 0 || frame >= COCKPIT_FRAMES) return NULL;
    const int left = steering_negative != (mirrored != 0);
    const int row = which == 2 ? (left ? 0 : 1) : which == 0 ? (left ? 2 : 4) : (left ? 3 : 5);   /* 'w'l, 'w'r, then 't'l, 's'l, 't'r, 's'r */
    return c->frames[row][frame];
}

int wheel_front_pixel(const uint8_t *shape, int x, int y)
{
    if (shape == NULL) return 0;
    const int left = s16(shape + SHAPE_X), top = s16(shape + SHAPE_Y), w = s16(shape + SHAPE_WIDTH), h = s16(shape + SHAPE_HEIGHT);
    if (w <= 0 || h <= 0 || x < left || x >= left + w || y < top || y >= top + h) return 0;
    return shape[SHAPE_PIXELS + (y - top) * w + (x - left)] != SEE_THROUGH;
}

/* the shape's colour at (x, y) of the 640 view, -1 where it draws nothing */
static int shape_value(const uint8_t *shape, int x, int y)
{
    return wheel_front_pixel(shape, x, y)
               ? shape[SHAPE_PIXELS + (y - s16(shape + SHAPE_Y)) * s16(shape + SHAPE_WIDTH) + (x - s16(shape + SHAPE_X))]
               : -1;
}

int wheel_front_now(const uint8_t *w, const uint8_t *t, const uint8_t *s, const uint8_t *art, int x, int y)
{
    if (wheel_front_pixel(t, x, y) || wheel_front_pixel(s, x, y)) return 1;
    const int v = shape_value(w, x, y);
    return v >= 0 && art != NULL && v != art[y * 640 + x];
}

void wheel_front_zone(const Cockpit *c, const uint8_t *art, uint8_t *zone)
{
    memset(zone, 0, (size_t)640 * 480);
    for (int row = 0; row < 6; row++)
        for (int frame = 0; frame < COCKPIT_FRAMES; frame++) {
            const uint8_t *shape = c->frames[row][frame];
            if (shape == NULL) continue;
            const int left = s16(shape + SHAPE_X), top = s16(shape + SHAPE_Y), w = s16(shape + SHAPE_WIDTH), h = s16(shape + SHAPE_HEIGHT);
            for (int y = top < 0 ? 0 : top; y < top + h && y < 480; y++)
                for (int x = left < 0 ? 0 : left; x < left + w && x < 640; x++) {
                    const int v = shape_value(shape, x, y);
                    if (v >= 0 && (row >= 2 || v != art[y * 640 + x])) zone[y * 640 + x] = 1;
                }
        }
}

/* port: see tach_dim.h. */
#include "game/race/tach_dim.h"

enum { SHAPE_WIDTH = 4, SHAPE_HEIGHT = 6, SHAPE_X = 0xc, SHAPE_Y = 0xe };   /* an FSH shape's header (blit.c) */

static int s16(const uint8_t *p) { return (int16_t)(uint16_t)(p[0] | p[1] << 8); }

int tach_dim_box(const uint8_t *shape, int box[4])
{
    if (shape == 0) return 0;
    box[0] = s16(shape + SHAPE_X), box[1] = s16(shape + SHAPE_Y), box[2] = s16(shape + SHAPE_WIDTH), box[3] = s16(shape + SHAPE_HEIGHT);
    return box[2] > 0 && box[3] > 0;
}

int tach_dim_face(const uint8_t *shape, int x, int y, uint8_t value, uint8_t needle_colour, uint8_t key)
{
    int box[4];
    if (value == key || value == needle_colour || !tach_dim_box(shape, box)) return 0;
    return x >= box[0] && x < box[0] + box[2] && y >= box[1] && y < box[1] + box[3];
}

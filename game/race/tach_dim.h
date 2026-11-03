/* port (tach_dim): the R&T tachometer of the views outside the car, a little darker with the light of the time of day. The 2D layer of those views is the HUD's
 * whole (none of the tint, drive_wide.c compose_classes), so the dial shines at night; with tach_dim its face takes the dash's half tint and only the needle
 * keeps the full brightness, as if lit. The tach is 'tach' of the dash's bank (hud_needle.h), a shape at its own place. */
#ifndef TNFS_TACH_DIM_H
#define TNFS_TACH_DIM_H

#include <stdint.h>

/* the box of the tach shape on the 640 view: left, top, width, height; 0 when `shape` is NULL */
int tach_dim_box(const uint8_t *shape, int box[4]);
/* 1 when the pixel `value` at (x, y) of the 640 view is the dial's face: inside the shape's box and not the needle's colour (nor `key`, the layer's see-through) */
int tach_dim_face(const uint8_t *shape, int x, int y, uint8_t value, uint8_t needle_colour, uint8_t key);

#endif

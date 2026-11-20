/* A car's wheels made by the build (dep85, notes/dep85-car-mod-flat-source-investigation.md sections 9-12): the four tyre frames from one picture
 * of the wheel, side-on. The race shows tyr1 standing, tyr2 and tyr3 in turn at low speed and tyr4 fast (game/race/car_wheels.c); here they are
 * still (the picture), slow 1 and slow 2 (turned by -/+ half the slow angle, lightly blurred along the turn) and fast (blurred along a wider arc).
 * The tyre band (the outer `tyre_share` of the radius) and the alpha are the picture's in every frame. Integer arithmetic only: the same picture
 * and options give the same bytes on every machine. Text and bytes in and out, no paths (car build reads the kit's files). */
#ifndef TNFS_CAR_WHEELS_GEN_H
#define TNFS_CAR_WHEELS_GEN_H

#include "formats/exchange.h"

#include <stdint.h>

enum { WHEEL_FRAMES = 4, WHEEL_FRAME_MAX_SIDE = 1024 };

typedef enum { WHEEL_FRAME_STILL = 0, WHEEL_FRAME_SLOW_1, WHEEL_FRAME_SLOW_2, WHEEL_FRAME_FAST } WheelFrame;

typedef struct {
    double centre_x, centre_y;   /* pixels from the picture's top-left corner (a pixel's centre is +0.5), when has_centre */
    double radius;               /* pixels, when has_radius */
    int has_centre, has_radius;  /* 0: from the bounding box of the opaque pixels (alpha >= 128) */
    double tyre_share;           /* the outer share of the radius that is tyre, never blurred: 0.32 (the CD's 0.29-0.36) */
    double slow_angle_deg;       /* the turn from slow 1 to slow 2: 20 (the CD's 19-35) */
    double fast_arc_deg;         /* the arc the fast frame is blurred along: 72, about 0.45 of the angular detail left as on the CD; 360 leaves none */
} WheelFrameOptions;

void wheel_frame_options_default(WheelFrameOptions *o);

/* the wheel's circle: the centre and half the larger side of the bounding box of the pixels with alpha >= 128; 0 with `err` when there is none */
int wheel_frame_find_circle(const uint8_t *rgba, int w, int h, double *cx, double *cy, double *radius, ExchangeError *err);

/* The four frames of the w x h RGBA picture `rgba`, each w x h RGBA, malloc'd into frames[WHEEL_FRAME_*] (free each). 0 with `err` and every frame
 * NULL: a size of 0 or past WHEEL_FRAME_MAX_SIDE, no opaque pixel, tyre_share outside 0..1 (both excluded), a radius of 0 or less, an angle outside
 * 0..180 (slow) or 0..360 (fast), out of memory. */
int wheel_frames_make(const uint8_t *rgba, int w, int h, const WheelFrameOptions *o, uint8_t *frames[WHEEL_FRAMES], ExchangeError *err);

#endif

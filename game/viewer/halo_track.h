/* port (lamp_halos): the street lamps' halos kept from one frame to the next. The blobs the lamps leave in the picture come and go (a far lamp is a pixel or two, and the sprite
 * sometimes paints none), and with more lamps than the quota the ones at its edge swap places; drawn as found, the halos blink. Each blob found is matched to the halo it
 * was last frame (by place and colour); a halo whose blob is not found fades over a few frames instead of going out, a new one fades in, the radius follows the blob's
 * smoothly, and a halo already shown keeps its place in the quota against a new one of the same size. Pure: no drawing, no globals. */
#ifndef TNFS_HALO_TRACK_H
#define TNFS_HALO_TRACK_H

#include <stdint.h>

enum {
    HALO_TRACK_MAX = 64,
    HALO_FADE_IN_PERCENT = 34,    /* a frame's step up */
    HALO_FADE_OUT_PERCENT = 25,   /* a frame's step down: a halo lost lasts four frames */
    HALO_EXTRA = 8                /* the fading halos shown over the quota, at most */
};

typedef struct {
    int cx, cy;     /* the blob's centre */
    int area;       /* its pixels */
    int radius;     /* the halo's radius it asks for */
    int value;      /* its colour entry */
    int depth;      /* the camera depth of its board when known (smaller is nearer), 0 unknown */
} HaloCand;

typedef struct {
    int cx, cy, radius;
    int percent;    /* 1..100: how much of the halo shows */
    int value;
    int area;       /* the blob area the halo has been following, smoothed (x16): how far the lamp is */
} HaloDraw;

typedef struct {
    float x, y, vx, vy, radius, alpha, area_avg;
    int value, area, depth, live, shown, seen;   /* shown: drawn in the frame before; seen: its blob was found this frame */
} HaloTrack;

typedef struct {
    HaloTrack t[HALO_TRACK_MAX];
} HaloTracker;

void halo_track_clear(HaloTracker *tracker);

/* One frame: `cands` (n of them, in any order) matched to the halos of the frame before, the nearest first by area (then depth, then place); at most `quota` established halos
 * and HALO_EXTRA fading ones come out in `out` (room for `max`); returns how many. */
int halo_track_frame(HaloTracker *tracker, const HaloCand *cands, int n, int quota, HaloDraw *out, int max);

#endif

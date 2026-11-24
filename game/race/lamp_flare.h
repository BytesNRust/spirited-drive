/* port (port.cfg lamp_flares, the light effects): a soft red halo round a car's lit brake lights. The lit brake light is a palette entry of its own (the emissive
 * lamps', lamp_emissive.h, not fogged or tinted), so the lamp's visible pixels are the pixels of that index: found in the car's screen rectangle, they are exactly what the car's
 * body and what stands in front of it leave visible, and need no model data. Each blob of them gets a dithered halo in two strengths
 * (the lamp's colour at a fifth and at under half), added to each pixel's colour and put back into the palette, so the glow stays in it. */
#ifndef TNFS_LAMP_FLARE_H
#define TNFS_LAMP_FLARE_H

#include <stdint.h>

enum {
    LAMP_FLARE_BLOBS = 8,         /* the most blobs a car gives */
    LAMP_FLARE_MAX_RADIUS = 48,   /* the halo's reach in pixels, at any render scale */
    LAMP_FLARE_LEVELS = 3,        /* the halo's dither levels above none */
    LAMP_FLARE_REACH = 40,        /* the halo's radius in tenths of the lamp blob's own radius: a glow a few times the lamp's size, to read at chase distance */
    LAMP_FLARE_MIN_RADIUS = 6
};

typedef struct {
    int cx, cy;       /* the pixels' centre */
    int area;         /* pixels */
    uint8_t value;    /* the pixel value the blob was found from (a lamp in fog has several: the fog's levels' images of its entry) */
} LampBlob;

/* The 4-connected blobs of pixels whose value is flagged in `member` (256 flags) in the rectangle x0..x1 x y0..y1 (inclusive, clipped by the caller to the surface), at most
 * `max`, largest first; returns how many. */
int lamp_blobs(const uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, const uint8_t member[256], LampBlob *out, int max);

/* port (lamp_halos): the same for a whole frame, where the lamps are many and small: any size of rectangle (the scratch buffer is kept between calls), the `max` largest blobs, largest first. A far lamp is
 * a pixel or two, so the nearest are the ones that get a halo when there are more than `max`. */
int lamp_blobs_frame(const uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, const uint8_t member[256], LampBlob *out, int max);

/* port (lamp_halos): 1 when the blob's centre is within `margin` pixels of one of the `count` screen rectangles {x0, y0, x1, y1} (corners in any order). The street lamps' halos go only to
 * blobs inside a rectangle where a lamp's own sprite was drawn: a traffic sign the reflectors light is painted in the same entries, and is no lamp. */
int lamp_blob_in_rects(const LampBlob *blob, const int (*rects)[4], int count, int margin);

/* port (lamp_halos): the list those rectangles are kept in. A rectangle past LAMP_RECTS_MAX is not kept (it is counted in `dropped`), so a blob that would have been inside it is inside none and
 * gets no halo: what cannot be checked is never lit, so a sign never is. */
enum { LAMP_RECTS_MAX = 512 };
typedef struct { int rects[LAMP_RECTS_MAX][4]; int count, dropped; } LampRects;
void lamp_rects_reset(LampRects *list);
int lamp_rects_push(LampRects *list, int x0, int y0, int x1, int y1);   /* 1 when kept, 0 when the list was full */

/* The halo's radius for a blob of `area` pixels: the blob's own radius times LAMP_FLARE_REACH / 10, at least LAMP_FLARE_MIN_RADIUS, at most LAMP_FLARE_MAX_RADIUS. */
int lamp_flare_radius(int area);

/* The same for a powerful light (the police car's, the headlights): the radius scaled by `percent` (100 = lamp_flare_radius), at most LAMP_FLARE_BIG_RADIUS. */
enum { LAMP_FLARE_BIG_RADIUS = 110 };
int lamp_flare_radius_scaled(int area, int percent);

/* port (traffic_halo): how much of a far car's headlight halo shows, in percent, at camera depth `z` when the near model ends at `far_z`: all of it from `far_z` in, falling
 * in a straight line to nothing at twice `far_z`, so a lamp comes up with the distance and does not appear whole when the car changes model. `far_z` <= 0: all of it. */
int lamp_far_fade(int32_t z, int32_t far_z);

/* port (traffic_halo): 1 when a traffic car's lit lamps get their halo: the setting is on and the car's lamps are lit by the low light (drive_lamps_low: dawn, dusk, night, a tunnel), the
 * racers' own criterion, not the night alone. */
int lamp_flare_traffic_halo(int setting_on, int lamps_low);

/* port (lamp_flares): a tunnel lamp's halo fades. `best` is the brightest of the lamp's own colours at its centre, in percent of its glow (the road's asphalt and paint that share the lamp's
 * colours are 46 %, the lamp's own texels 60..137 % as it moves): the halo's target is 100 once `best` reaches LAMP_FADE_START_PERCENT and stays 100 while it is at least LAMP_FADE_KEEP_PERCENT
 * (`was`: the halo was shown a frame before), else 0. The halo's level (0..100) climbs to the target by at most LAMP_FADE_IN a frame; with a target of 0 it stays LAMP_FADE_HOLD frames (`*miss`, the frames it has not been found) and then falls by at most LAMP_FADE_OUT, so a lamp that shows in one
 * frame and not in the next keeps its halo and a halo comes and goes in steps. A lamp is the same one as last frame's when it is within lamp_fade_match_px(size) pixels: as wide as the lamp, at least
 * LAMP_FADE_MATCH_MIN_PX. */
enum { LAMP_FADE_START_PERCENT = 55, LAMP_FADE_KEEP_PERCENT = 47, LAMP_FADE_IN = 34, LAMP_FADE_OUT = 10, LAMP_FADE_HOLD = 10, LAMP_FADE_MATCH_MIN_PX = 6, LAMP_FADE_MIN_LEVEL = 6 };
int lamp_fade_target(int best_percent, int was);
int lamp_fade_step(int level, int target, int *miss);
int lamp_fade_match_px(int size);

/* The halo's dither level (0 none .. LAMP_FLARE_LEVELS) at (dx, dy) from the centre: the intensity falls off with the square of the distance
 * from 1 at the centre to 0 at `radius`, and a 4 x 4 ordered dither picks the level. */
int lamp_halo_level(int dx, int dy, int radius);

/* The same with the halo shown at `percent` (0..100) of its strength: the intensity scaled before the dither picks the level, so a fading halo thins out and does not shrink. */
int lamp_halo_level_pct(int dx, int dy, int radius, int percent);

/* Adds the halo at (cx, cy): each pixel of level l (1..LAMP_FLARE_LEVELS) becomes map[l - 1][pixel] (a 256-entry table of the frame's own palette:
 * the entry nearest the pixel's colour plus the level's); only inside x0..x1 x y0..y1. */
void lamp_halo_draw(uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, int cx, int cy, int radius,
                    const uint8_t *const map[LAMP_FLARE_LEVELS]);

/* lamp_halo_draw at `percent` (lamp_halo_level_pct). */
void lamp_halo_draw_pct(uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, int cx, int cy, int radius, int percent,
                        const uint8_t *const map[LAMP_FLARE_LEVELS]);

#endif

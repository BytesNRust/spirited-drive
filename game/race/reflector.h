/* port (reflectors): what is reflective at night shines in the player's lamps. A texture may have a mask (one byte a texel, 1 = reflective: a sign's white, a post's band, a stud
 * painted on the road). In the dark (night, or a dark tunnel) the masked texels of whatever lies in the cone of the PLAYER's lamps take their brightness back and above it,
 * far beyond the pool of light the road gets: out to REFLECTOR_REACH_M, brightest on the lamps' axis and falling with the distance and away from it. The rivals' lamps do nothing:
 * the light a reflector gives back returns to its own source. This file is the pure part: the gain at a point, the mask on a texture's smaller variants, and the texels' re-indexing. */
#ifndef TNFS_REFLECTOR_H
#define TNFS_REFLECTOR_H

#include <stddef.h>
#include <stdint.h>

enum {
    REFLECTOR_REACH_M = 200,        /* the farthest a reflector shines */
    REFLECTOR_NEAR_M = 6,           /* nearer than this the shine is full (before the axis and height take their share) */
    REFLECTOR_HALF_WIDTH_PERCENT = 35, /* the sideways reach at a distance, % of it: the cone's edge, where the shine is gone (a sign on the shoulder, 10 m to the side, shines from about 30 m on) */
    REFLECTOR_EYE_CM = 65,          /* the lamps' height above the ground */
    REFLECTOR_UP_FULL_PERCENT = 6,  /* a point this far above the lamps' level (% of the distance) still shines in full ... */
    REFLECTOR_UP_END_PERCENT = 16,  /* ... and none from this */
    REFLECTOR_LEVELS = 6,           /* the steps of shine a polygon takes: 0 none .. REFLECTOR_LEVELS - 1 full */
    REFLECTOR_ENTRIES_MAX = 6       /* the colours a track's masked texels may be kept apart in */
};

/* The shine, 0..256, of a point `forward_m` ahead of the player's lamps, `lateral_m` to the side (either sign) and `height_m` above the ground. 0 behind the lamps, beyond the reach, outside
 * the cone and (`dark` 0) in the light of day. Smooth in all three, so a sign does not blink on at the cone's edge. */
int reflector_gain(double forward_m, double lateral_m, double height_m, int dark);

/* A masked texel shines in one of three families of the entries the night already has lit (no palette entry of its own is spent): white (a grey or white texel), red (a red one), warm (any other
 * saturated colour: yellow, orange, green, blue shine in the lamps' warm tones). */
typedef enum { REFLECTOR_WHITE, REFLECTOR_RED, REFLECTOR_WARM, REFLECTOR_FAMILIES } ReflectorFamily;
ReflectorFamily reflector_family(const uint8_t rgb[3]);

/* the step (0 .. steps - 1) of a ramp of `steps` entries a polygon of `level` (1 .. REFLECTOR_LEVELS - 1) takes: the dimmest at level 1, the brightest at the top level; 0 for steps <= 1 */
int reflector_step(int level, int steps);

/* the step (0..REFLECTOR_LEVELS - 1) of a gain: 0 only for a gain of 0 */
int reflector_level(int gain);

/* The mask of a `sw` x `sh` texture on a `dw` x `dh` one (a smaller variant of it): a texel is reflective there when any texel of its block of the source is. A stud of one texel stays a stud
 * on the smaller bitmaps. Returns the reflective texels of `dst`. */
size_t reflector_mask_scale(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh);

/* The distinct palette indices of the texels `mask` marks in `pixels` (w x h), most used first, at most `max`: `out` gets them, the count is returned. */
int reflector_colours(const uint8_t *pixels, const uint8_t *mask, size_t count, uint8_t *out, int max);

/* Re-indexes the masked texels through `entry` (256 bytes; an index with entry == itself is left): returns the texels changed. Painting again changes nothing. */
size_t reflector_paint(uint8_t *pixels, const uint8_t *mask, size_t count, const uint8_t entry[256]);

#endif

/* port (wipers, dep49): the car's nose seen through the windscreen (dash_mask.h NOSE, cyan), as the sweeping blades are drawn over it. With the wipers on, the dash's own blade
 * (BLADE, red) is not drawn: where it lies on the glass the 3D shows, as before; where it lies on the nose, the nose is made again under it, from the nose round it.
 *
 * Which red pixels are the nose's: each red pixel takes the side of the nearest glass or nose pixel (a chamfer distance through the red, 3 straight and 4 diagonal): the nose's
 * when that pixel is a nose pixel of the art (cyan in the mask, drawn), the glass's when it is glass (nothing drawn). The dash's other art claims nothing, so a blade on the nose
 * that touches the dash is the nose's to its edge; a red pixel neither reaches is the glass's, as before. The hood's edge thus goes on across the blade, and a blade along the
 * edge is split between the two.
 *
 * The fill: the red nose pixels are the solution of Laplace's equation in RGB (each the mean of its four neighbours) whose boundary is the nose's own pixels round them, top,
 * bottom and sides: only nose pixels of the art and other red nose pixels are read, never the glass, the dash's art or the road, so the fill follows the hood's gradient. It is
 * solved for what a plane fitted to the nose round each piece of blade leaves, so a gradient carries on where the blade has the nose on one side only (along the hood's edge),
 * and kept within the colours round it. Each filled pixel is then one of the colours the nose's own pixels have, the nearest or, by an ordered dither, a pair of them, so it
 * stays on the hood's ramp and texture. Pure: no game state. */
#ifndef TNFS_DASH_NOSE_H
#define TNFS_DASH_NOSE_H

#include <stdint.h>

/* The nose of a dash `width` x `height`: `out` gets, per pixel, the art's colour on the nose (cyan in `mask` and drawn in `art`), the filled colour on a red pixel that is
 * the nose's, and `key` elsewhere. `art` is the dash's layer (`key` where nothing is drawn), `mask` its DASH_MASK_* classes, `palette` the 768 bytes the art's indices show
 * through. The nose's pixels (drawn and filled), 0 when the mask has none (`out` all `key`), -1 when it cannot (no memory, or bad arguments). */
int dash_nose_fill(const uint8_t *art, const uint8_t *mask, int width, int height, const uint8_t palette[768], uint8_t key, uint8_t *out);

/* What the fill was made from: a nose is made again when any of it changes (the dash's art, its mask, the palette the body colour sets, the settings' generation of
 * port_follow.h, so a change of mode or of a row with no restart reaches it, and whether the wipers are on). */
typedef struct {
    const void *art_id, *mask_id;
    uint8_t palette[768];
    uint32_t generation;
    int wipers;
} DashNoseKey;

typedef struct {
    DashNoseKey key;
    uint8_t *pixels;   /* width x height, as dash_nose_fill fills it; NULL when none */
    int made, count;
    unsigned builds;   /* how many times it was made: moves on with each new nose (the frame's copy follows it) */
} DashNoseCache;

/* The key of the dash on screen now: its art's and mask's ids, the palette its art shows through, the settings' generation and port_wipers as they are now (port_follow.h's
 * rule: a value kept from the settings follows their every change). The struct is zeroed first, so two keys compare with memcmp. */
DashNoseKey dash_nose_key(const void *art_id, const void *mask_id, const uint8_t palette[768]);

/* The nose for `key` (the art and mask behind its ids), made when the key differs from the cached one's; NULL when the wipers are off, there is no mask or palette, or the mask
 * has no nose. */
const uint8_t *dash_nose_cached(DashNoseCache *c, const DashNoseKey *key, const uint8_t *art, const uint8_t *mask, int width, int height, uint8_t key_colour);

/* Frees the cached nose. */
void dash_nose_cache_free(DashNoseCache *c);

#endif

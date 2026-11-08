/* port (reflectors, game/race/reflector.h): the second pass. A texture with a mask is registered with it; when the player's lamps are lit in the dark, a polygon drawn with that texture is
 * drawn once more from a copy of the texture where the texels the mask leaves out are the see-through index, through a table brightened by the shine at the polygon's place. The copy keeps
 * every colour of the texture (no palette entries are spent), and a polygon with no mask, or out of the cone, costs one registry lookup. */
#ifndef TNFS_REFLECT_PASS_H
#define TNFS_REFLECT_PASS_H

#include "core/formats/fsh.h"
#include "game/race/track_art.h"

#include <stdint.h>

/* forgets every texture and mask */
void reflect_pass_reset(void);

/* registers `bitmap` (any size) with a mask of `mw` x `mh` bytes (1 = reflective); the mask is copied, and resampled any-of when the sizes differ. Returns 1 when registered; 0 when it was not (no room, a mask with no texel set, the bitmap already there). */
int reflect_pass_add(const FshEntry *bitmap, const uint8_t *mask, int mw, int mh);

int reflect_pass_count(void);

/* registers every texture of `art` that `mask_for` gives a mask (a road slot's variants take the base's mask resampled), once per art: asked again for the art it has, it does nothing and
 * returns -1; otherwise it forgets what it had and returns how many it registered. The textures are the art's own bitmaps, so whoever frees or loads an art calls reflect_pass_forget_art: the next
 * track's art may stand at the same address, and the address alone does not say it is the same textures. */
int reflect_pass_register_art(const TrackArt *art, const uint8_t *(*mask_for)(const TrackTexture *t));

/* forgets the art (and every texture) registered; nothing when none is */
void reflect_pass_forget_art(void);

/* the entries the night already has lit (palette indices, 0 = not there): the street lamp's three, dimmest first (NULL: none), the windows', and the cars' lit brake, position, head and reversing lights.
 * A polygon's shine takes one of them by the texel's colour family and its own step; no entry of the pass's own is spent. */
void reflect_pass_set_entries(const uint8_t street[3], uint8_t window, uint8_t brake_lit, uint8_t position, uint8_t headlight, uint8_t reverse);

/* the frame's state: the player's lamps lit (`on`) at the ground place (x, z) in metres, heading the unit vector (hx, hz), and whether it is dark (night, or a dark tunnel). The palette
 * the copies and the tables follow (768 bytes, the world's own colours; a change remakes them). */
void reflect_pass_frame(int on, double x, double z, double hx, double hz, int dark, const uint8_t palette[768]);

/* 1 when anything can shine this frame: a texture registered, the lamps lit, in the dark */
int reflect_pass_active(void);

/* the shine (0..256) of a point at the ground place (x, z), `height` metres above it (reflector_gain from the lamps) */
int reflect_pass_gain(double x, double z, double height);

/* the texture's see-through copy, or NULL when `t` has no mask. Valid until the next reflect_pass_reset or palette change. */
const FshEntry *reflect_pass_copy(const FshEntry *t);

/* the table (256 bytes) a polygon of `level` (1 .. REFLECTOR_LEVELS - 1) takes; valid for the frame */
const uint8_t *reflect_pass_table(int level);

/* the same as `rows` identical tables (the Modern 3D road picks a table by the darkness at each pixel; here there is none) */
const uint8_t (*reflect_pass_rows(int level, int rows))[256];

#endif

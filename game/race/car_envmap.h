/* port (body shine, envmap = body_shine 4): the environment map the cars' paint and glass take their light from, as Gran Turismo's
 * sphere map. A body vertex's reflection of the line of sight about its normal, in camera space, is looked up on a small sphere-map
 * image (u = rx/m + 0.5, v = ry/m + 0.5, m = 2 sqrt(rx^2 + ry^2 + (rz + 1)^2), z toward the viewer). The image is made here at start-up
 * from port-made values only (no EA or Sony pixels): a sky gradient, a sharp horizon line, a dark ground band and one sun spot. */
#ifndef TNFS_CAR_ENVMAP_H
#define TNFS_CAR_ENVMAP_H

#include <stdint.h>

enum { CAR_ENVMAP_SIZE = 128 };   /* the image is CAR_ENVMAP_SIZE squared bytes, row 0 the top (v = 1), 128 the neutral */

/* the sphere-map uv (each in 0..1) of a surface with the unit `normal` seen along the unit `view` (from the eye to the surface), both in
 * camera space (z forward, y up). A surface facing the viewer maps to the centre (0.5, 0.5). */
void car_envmap_uv(const float normal[3], const float view[3], float uv[2]);

/* the uv packed into one int32 (16 bits each), for the raster records' 24 ints, and back */
int32_t car_envmap_pack(const float uv[2]);
void car_envmap_unpack(int32_t packed, float uv[2]);

/* `hard`[level][entry] is 1 where the entries `tables` gives at that level and the next are farther apart in colour than CAR_ENVMAP_HARD_STEP (RGB
 * distance, 8 bits): a palette with few entries in a hue (a red) steps from a red to a pink there, and a dither between the two is noise, not a
 * blend. The lightest level has no step above it. */
#define CAR_ENVMAP_HARD_STEP 48.0f
void car_envmap_hard(const uint8_t palette[768], const uint8_t tables[16][256], uint8_t hard[16][256]);

/* the image, made on the first call (CAR_ENVMAP_SIZE^2 bytes, kept for the run) */
const uint8_t *car_envmap_image(void);

/* the image's byte at uv (nearest texel) */
uint8_t car_envmap_sample(const uint8_t *image, float u, float v);

/* the intensities of the envmap, as the level's share of its range: the paint (mask 0) and the glass and lamps (mask 128) have one each */
#define CAR_ENVMAP_BODY_GAIN 0.9f
#define CAR_ENVMAP_GLASS_GAIN 0.9f

/* the level (car_shine.h's scale, fractional) an image byte asks for at `gain`: the sky and the sun lift, the ground takes it down */
float car_envmap_level(uint8_t sample, float gain);

#endif

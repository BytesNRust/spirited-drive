/* port (body shine, envmap = body_shine 4): the environment map the cars' paint and glass take their light from, as Gran Turismo's
 * sphere map. A body vertex's reflection of the line of sight about its normal is looked up on a small sphere-map image (Heidrich's
 * mapping: u = rx/m + 0.5, v = ry/m + 0.5, m = 2 sqrt(rx^2 + ry^2 + (rz + 1)^2), z toward the viewer). Its axes are the WORLD's: y is the
 * world's up (in camera space), so the sky stays up however the camera tilts or rolls. The image is made here at start-up from port-made
 * values (no EA or Sony pixels): a sky gradient with soft clouds and a sun spot, a dark band below the horizon, and a flat dark ground; or
 * a user's car_envmap.png replaces it. */
#ifndef TNFS_CAR_ENVMAP_H
#define TNFS_CAR_ENVMAP_H

#include <stdint.h>

enum { CAR_ENVMAP_SIZE = 128 };   /* the image is CAR_ENVMAP_SIZE squared bytes, row 0 the top (v = 1), 128 the neutral */

/* the sphere-map uv (each in 0..1) of a surface with the unit `normal` seen along the unit `view` (from the eye to the surface), both in
 * camera space (z forward, y up), and `up` the world's up in camera space (unit). A surface facing the viewer maps to the centre (0.5, 0.5).
 * The uv depends only on the reflection's direction relative to `up`, so a roll of the camera leaves it unchanged. */
void car_envmap_uv(const float normal[3], const float view[3], const float up[3], float uv[2]);

/* the uv packed into one int32 (16 bits each), for the raster records' 24 ints, and back */
int32_t car_envmap_pack(const float uv[2]);
void car_envmap_unpack(int32_t packed, float uv[2]);

/* `hard`[level][entry] is 1 where the entries `tables` gives at that level and the next are farther apart in colour than CAR_ENVMAP_HARD_STEP (RGB
 * distance, 8 bits): a palette with few entries in a hue (a red) steps from a red to a pink there, and a dither between the two is noise, not a
 * blend. The lightest level has no step above it. */
#define CAR_ENVMAP_HARD_STEP 48.0f
void car_envmap_hard(const uint8_t palette[768], const uint8_t tables[16][256], uint8_t hard[16][256]);

/* the image, made on the first call (CAR_ENVMAP_SIZE^2 bytes, kept for the run): car_envmap_build's */
const uint8_t *car_envmap_image(void);

/* the image the lookup chain gives into `image` (CAR_ENVMAP_SIZE squared bytes): the first of TNFS_CAR_ENVMAP (a full path), data/car_envmap.png
 * (the working directory) and TNFS_SOURCE_DIR/data/car_envmap.png that reads (car_envmap_file), else the procedural image */
void car_envmap_build(uint8_t *image);

/* the 8-bit grey PNG at `path` (its red channel where it is coloured), resampled to CAR_ENVMAP_SIZE by nearest neighbour into `image`.
 * Returns 0 when the file is missing or is not a readable PNG (the image is then left as it was). */
int car_envmap_file(const char *path, uint8_t *image);

/* the image's byte at uv (nearest texel) */
uint8_t car_envmap_sample(const uint8_t *image, float u, float v);

/* the intensities of the envmap, as the level's share of its range: the paint (mask 0) and the glass and lamps (mask 128) have one each. The
 * paint is the softer: at the glass's it read too glossy (the owner) */
#define CAR_ENVMAP_BODY_GAIN 0.65f
#define CAR_ENVMAP_GLASS_GAIN 0.8f

/* the most a shade takes the level down, as a fraction of the levels below the neutral: the darkest byte is (1 - this) of the neutral's way
 * to the darkest level, so the paint keeps its colour (it darkens to about 0.7 of its value, never near black). The lift has its own gains. */
#define CAR_ENVMAP_SHADE 0.6f

/* the level (car_shine.h's scale, fractional) an image byte asks for at `gain`: above the neutral byte the lift toward the lightest level, below it
 * the shade toward the darkest, at most CAR_ENVMAP_SHADE of the way */
float car_envmap_level(uint8_t sample, float gain);

#endif

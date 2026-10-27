/* port (body shine, re/TODO.md "Body shine"): the pure parts. A body vertex's shine level comes from its normal reflected about the
 * view, as in an environment map: up-facing panels take a lighter sky band, lower vertical faces stay darker, and soft streaks slide over
 * the panels as the car turns and travels. A level picks one of CAR_SHINE_LEVELS tables, each moving a palette entry to another of its
 * own colour family (8-bit: the screen is indices into one palette), so a red stays a red. */
#ifndef TNFS_CAR_SHINE_H
#define TNFS_CAR_SHINE_H

#include <stdint.h>

/* the levels, 0 darkest .. CAR_SHINE_LEVELS - 1 lightest; CAR_SHINE_NEUTRAL leaves a colour as it is */
enum { CAR_SHINE_LEVELS = 16, CAR_SHINE_NEUTRAL = 4 };

/* `out`[level][entry]: for a palette (768 bytes, 8 bits a channel) the entry that is `entry` seen at that level. The neutral level is the identity;
 * a darker one takes the nearest entry of the same hue (within 8 degrees; a grey takes a grey) to the colour scaled down, a lighter one to
 * the colour scaled up and lifted toward white. The markers 0xfe and 0xff and the near-black entries (trim, tyres, glass) stay. */
void car_shine_tables(const uint8_t palette[768], uint8_t out[CAR_SHINE_LEVELS][256]);

/* whether colours `a` and `b` (8 bits a channel) are of one hue family: within 8 degrees of hue, a grey with a grey (the family the shine tables keep) */
int car_shine_same_family(const uint8_t a[3], const uint8_t b[3]);

/* port (body_shine 4, envmap, car_envmap.h): the tables for the palette's paint texels and for the keep-and-shine ones (glass, lamps), level by
 * level as car_shine_tables, but a highlight goes toward white and loses saturation. The glass (mask 128) keeps its own colour as the base. */
void car_envmap_tables(const uint8_t palette[768], uint8_t paint[CAR_SHINE_LEVELS][256], uint8_t glass[CAR_SHINE_LEVELS][256]);

/* the level (0 .. CAR_SHINE_LEVELS - 1, fractional: the draw dithers between two) of a surface with the unit `normal` (camera space, toward the
 * viewer, the exe's y up) seen along the unit `view` (from the eye to the surface). `up` is the world's up in the camera's space (unit): the
 * reflection is looked up against it, so the horizon is the world's and holds still while the body pitches, rolls and yaws under it, as in
 * Gran Turismo: a bright band above the line, a dark one below, nothing scrolling. `strength` 1 subtle, 2 the default look, 3 strong. */
float car_shine_level(const float normal[3], const float view[3], const float up[3], int strength);

#endif

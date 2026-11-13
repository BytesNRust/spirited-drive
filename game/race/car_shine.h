/* port (body shine): the pure parts of the envmap shine (car_envmap.h). A level picks one of CAR_SHINE_LEVELS tables, each moving a palette
 * entry to another of its own colour family (8-bit: the screen is indices into one palette), so a red stays a red. */
#ifndef TNFS_CAR_SHINE_H
#define TNFS_CAR_SHINE_H

#include <stdint.h>

/* the levels, 0 darkest .. CAR_SHINE_LEVELS - 1 lightest; CAR_SHINE_NEUTRAL leaves a colour as it is */
enum { CAR_SHINE_LEVELS = 16, CAR_SHINE_NEUTRAL = 4 };

/* whether colours `a` and `b` (8 bits a channel) are of one hue family: within 8 degrees of hue, a grey with a grey (the family the shine tables keep) */
int car_shine_same_family(const uint8_t a[3], const uint8_t b[3]);

/* port (body_shine 4, envmap, car_envmap.h): the tables for the palette's paint texels and for the keep-and-shine ones (glass, lamps), level by
 * level, `out`[level][entry] the entry that is `entry` seen at that level (CAR_SHINE_NEUTRAL the identity). A highlight goes toward white and
 * loses saturation. The glass (mask 128) keeps its own colour as the base. */
void car_envmap_tables(const uint8_t palette[768], uint8_t paint[CAR_SHINE_LEVELS][256], uint8_t glass[CAR_SHINE_LEVELS][256]);

/* the colour (8 bits a channel, as floats) the envmap asks for from `rgb` at the lift `t` (the level_t of its level: 0 .. 1; negative for a
 * shade): toward white and less saturated for a lift. car_colours wants the tones a saturated paint's lift asks for. */
void car_envmap_want(const uint8_t rgb[3], float t, float want[3]);

#endif

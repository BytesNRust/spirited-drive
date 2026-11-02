/* port (emissive lamps): the car texels a mask marks as a brake, position or head light (car_mask.h), glowing when the light is low.
 * A lamp texel keeps the palette index the car loader gave it for the day, and in low light is pointed at a palette entry that no tint, light
 * table or fog table darkens: a reserved entry shared by every car for each of the lit brake light, the position light and the headlight. A brake
 * texel shows the position light's entry while the brake is off, the lit one while it is on. The texels are kept as offsets into the car's file, so the toggle needs no mask. */
#ifndef TNFS_LAMP_EMISSIVE_H
#define TNFS_LAMP_EMISSIVE_H

#include <stddef.h>
#include <stdint.h>

typedef enum { LAMP_BRAKE, LAMP_POSITION, LAMP_HEADLIGHT, LAMP_REVERSE } LampKind;

typedef struct {
    uint32_t offset;   /* into the car's file */
    uint8_t kind;      /* LampKind */
    uint8_t day;       /* the index the texel has by day (read after the car's palette mapping) */
} LampTexel;

typedef struct {
    LampTexel *texel;
    int count, capacity;
} LampTexels;

/* the entries of low light: where each kind points (the brake texels: lit while braking, else the position one) */
typedef struct {
    uint8_t brake_lit, position, headlight, reverse;
} LampEntries;

/* what a car's lamp texels show: LAMP_DAY, LAMP_LOW or LAMP_BRAKING (the brake, low light), and with LAMP_REVERSING or'd in the reversing light's texels lit: they glow in any light,
 * the day's too, while the car is in reverse gear */
typedef enum { LAMP_DAY = 0, LAMP_LOW = 1, LAMP_BRAKING = 2, LAMP_REVERSING = 4 } LampState;

/* Appends the texels of `mask` (count classes of car_mask.h) that are lamps, at file offsets base + i. Returns the number added, -1 on no memory. */
int lamp_texels_collect(LampTexels *l, uint32_t base, const uint8_t *mask, size_t count);
void lamp_texels_free(LampTexels *l);

/* Reads each texel's day index from the file as it is now (call after the car's palette is mapped). Offsets past `size` are dropped. */
void lamp_texels_read(LampTexels *l, const uint8_t *file, size_t size);

/* Writes the file's texels for `state`: LAMP_DAY the day indices; LAMP_LOW each kind's entry (the brake texels the position one); LAMP_BRAKING the
 * same with the brake texels on the lit entry (an entry of 0 is none: that kind keeps its day index); the reversing texels show their entry
 * whenever `state` has LAMP_REVERSING and the day's index otherwise. Returns how many bytes changed. */
int lamp_texels_write(const LampTexels *l, uint8_t *file, size_t size, LampState state, const LampEntries *e);

/* The colours of the lamps in low light, 6 bits a channel as the race palette has them: the lit brake light (a red like the exe's lit row), the position light (a red like the brake's lit one, at about
 * half its strength; the BRAKE texels show it too while the brake is off) and the headlight (warm white). */
enum { LAMP_BRAKE_ON = 10,   /* a car's brake above this is lit (car_lights.c) */
       LAMP_REVERSE_GEAR = -2,   /* car->gear in reverse (car_struct.h) */
       LAMP_BRAKE_R = 62, LAMP_BRAKE_G = 5, LAMP_BRAKE_B = 5, LAMP_POSITION_R = 36, LAMP_POSITION_G = 3, LAMP_POSITION_B = 3, LAMP_HEADLIGHT_R = 63, LAMP_HEADLIGHT_G = 58, LAMP_HEADLIGHT_B = 44,
       LAMP_REVERSE_R = 63, LAMP_REVERSE_G = 63, LAMP_REVERSE_B = 63 };   /* the reversing light: white */

/* Whether the light at a car is low: the dawn / dusk tint of the world or the ambient ratio at the car (a tunnel's, a shade's) under 1 on any channel.
 * NULL for a light that is not there. */
int lamp_emissive_low(const float tint[3], const float ratio[3]);

/* port (day): the lamps follow the light at the car with a margin, so a light shade does not light them. A car's state: whether its lamps are lit,
 * and for how many frames the light has been back. They light at once when the world's tint is below 1 (dawn, dusk, night) or the ambient ratio at the
 * car is under LAMP_SHADE_ON_PERCENT on any channel (a tunnel's), and go out only after the ratio has been at LAMP_SHADE_OFF_PERCENT or more for
 * LAMP_SHADE_HOLD frames in a row. Returns the state. */
enum { LAMP_SHADE_ON_PERCENT = 70, LAMP_SHADE_OFF_PERCENT = 80, LAMP_SHADE_HOLD = 12 };
typedef struct { int low, back; } LampLight;
int lamp_light_step(LampLight *s, const float tint[3], const float ratio[3]);

/* Up to `want` entries from `first` to `last` (inclusive) that `used` does not mark, into `out`: the ones whose colour (`rgb`, 256 x 3 at 8 bits) has the
 * nearest other entry of `usable`, nearest first (the highest index first on a tie), so taking them out of the search shifts the world's picks the least.
 * Returns how many it found. */
int lamp_pick_entries(const uint8_t used[256], const uint8_t usable[256], const uint8_t rgb[768], int first, int last, int want, uint8_t *out);   /* `out` holds `want` entries (at most 256) */

#endif

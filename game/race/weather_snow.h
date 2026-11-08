/* port (snow_weather): the rain of a snowy track (its places file's `snow 1`: Alpine 3) is snow. What falls and what hits the camera changes; the road stays wet as in rain (the reflections, the spray
 * and the wet tyre sound are weather_wet's, which the snow keeps). Pure functions of their arguments, like weather.h: a replay or a screenshot at the same tick draws the same flakes. */
#ifndef TNFS_WEATHER_SNOW_H
#define TNFS_WEATHER_SNOW_H

#include <stdint.h>

enum { SNOW_NONE = 0, SNOW_LIGHT = 1, SNOW_HEAVY = 2 };

/* What falls: SNOW_LIGHT for rain and SNOW_HEAVY for the storm on a snowy track (`track_snow`) with the switch (`enabled`); SNOW_NONE for anything else (clear, fog, any track without `snow 1`, the switch off).
 * Callers use it to turn the lightning and the thunder off: snow has none. */
int weather_snow_level(int weather, int track_snow, int enabled);

/* The flakes of a view box_w x box_h pixels: how many (the storm's are SNOW_HEAVY_TIMES the snow's), and flake `i` at `tick` as { x, y, size } in the box, size 1..SNOW_SIZE_MAX in units of the render
 * scale. They fall at SNOW_FALL_MIN..SNOW_FALL_MAX percent of the box's height a second (slower than the rain's streaks), drift sideways with a sway of their own, and the storm adds a wind that
 * leans them all one way. A flake that leaves the box comes in at the other side, so the count in view is steady. */
enum { SNOW_PER_640x480 = 160, SNOW_HEAVY_TIMES = 3, SNOW_MAX = 1400, SNOW_SIZE_MAX = 3, SNOW_FALL_MIN = 9, SNOW_FALL_MAX = 22 };
int weather_snow_count(int32_t box_w, int32_t box_h, int level);
void weather_snow_flake(int flake, uint32_t tick, int level, int32_t box_w, int32_t box_h, int32_t out[3]);

/* port (snow_fx): the flakes with more depth. weather_snow_flake_fx is weather_snow_flake with
 *   - a randomness of its own: each flake's fall speeds up and slows (a little: a few percent of the box); it does not sway from side to side;
 *   - the car's speed (`speed`, 16.16 m/s): the flakes flow away from the vanishing point (`vx`, `vy`, in box pixels) like the rain's streaks, at a smaller angle (at most SNOW_LEAN_PERCENT of the
 *     way to the rain's), and one the lean takes out of the box is not drawn (size 0);
 *   - the storm's wind, which swings: weather_snow_wind (tenths of a percent of the box's width a second, signed) follows a sine of period SNOW_DRIFT_PERIOD_TICKS, so the snowstorm leans one way,
 *     comes upright and leans the other, instead of always from the same side.
 * With the speed at nothing the flakes fall as weather_snow_flake's do, a little less regular, but straight: no there-and-back sway. */
enum { SNOW_DRIFT_PERIOD_TICKS = 1800, SNOW_WIND_MAX = 120, SNOW_LEAN_PERCENT = 30, SNOW_LENS_EVERY = 3 };
int weather_snow_wind(uint32_t tick);
void weather_snow_flake_fx(int flake, uint32_t tick, int level, int32_t box_w, int32_t box_h, int32_t speed, int32_t vx, int32_t vy, int32_t out[3]);

/* port (snow_fx): the soft flake: a round sprite of ramp steps for a flake of `size` (1..SNOW_SIZE_MAX, the far ones tiny and the near ones big, softer and dimmer at the rim) and the render's unit `side`
 * in pixels (clamped 1..SNOW_SIDE_MAX): `out` (SNOW_SPRITE_MAX x SNOW_SPRITE_MAX bytes) gets dim x dim steps, 0 none .. SNOW_RAMP the whitest (the draw maps a step to the rain's brightness tables).
 * Returns dim. Built in code, so at the render scale's own resolution it is not a scaled block. */
enum { SNOW_RAMP = 4, SNOW_SIDE_MAX = 12, SNOW_SPRITE_MAX = 2 * (SNOW_SIDE_MAX * 5 / 2 + 2) + 1 };
int weather_snow_sprite(int size, int side, uint8_t *out);

/* port (snow_fx): the marks the snow leaves on the lens, a third of the rain's drops in number (weather_lens_drop's, every SNOW_LENS_EVERY-th) */
int weather_snow_lens_count(int32_t box_w, int32_t box_h);

/* Where a race's snow comes from: `in_replay` a replay's own record (`replay_snow`, 0 for a file that has none), else `local`, the setting or the host's (port_snow_weather). The snow decides the grip, so a
 * replay is played as it was recorded. */
int snow_race_wanted(int in_replay, int replay_snow, int local);

/* The last race's grip: 1 when it ran on the snow's (a rally on a snowy track with the snow on). Set when the race starts, read by the records glue. */
void snow_grip_set(int on);
int snow_grip_get(void);

/* 1 when this race's grip is not the standard one (a wet race, rain_grip, or a rally on snow): it keeps no time or record (records_glue_route) */
int race_grip_nonstandard(int rain_grip_active, int snow_grip);

#endif

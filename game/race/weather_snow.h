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

/* Where a race's snow comes from: `in_replay` a replay's own record (`replay_snow`, 0 for a file that has none), else `local`, the setting or the host's (port_snow_weather). The snow decides the grip, so a
 * replay is played as it was recorded. */
int snow_race_wanted(int in_replay, int replay_snow, int local);

/* The last race's grip: 1 when it ran on the snow's (a rally on a snowy track with the snow on). Set when the race starts, read by the records glue. */
void snow_grip_set(int on);
int snow_grip_get(void);

/* 1 when this race's grip is not the standard one (a wet race, rain_grip, or a rally on snow): it keeps no time or record (records_glue_route) */
int race_grip_nonstandard(int rain_grip_active, int snow_grip);

#endif

/* port: the weather (re/notes/weather-design.md). Pure functions of the setting and the tick: no RNG of the game's, no clock,
 * so a replay or a screenshot at the same tick draws the same weather. */
#ifndef TNFS_WEATHER_H
#define TNFS_WEATHER_H

#include <stdint.h>

enum { WEATHER_CLEAR = 0, WEATHER_FOG = 1, WEATHER_RAIN = 2, WEATHER_COUNT = 3 };

/* The weather a name stands for ("clear", "fog", "rain", any case), or -1. */
int weather_named(const char *name);
const char *weather_name(int weather);   /* "clear" for a value out of range */

/* The colours the weather moves the view to, in the palette's scale. `base` is the track's horizon-foot fog colour. */
void weather_fog_colour(int weather, const uint8_t base[3], uint8_t out[3]);   /* the land's far end */
void weather_sky_colour(int weather, const uint8_t fog[3], uint8_t out[3]);    /* the sky's, at WEATHER_SKY_LEVEL */
void weather_rain_colour(uint8_t out[3]);                                      /* a streak's, at WEATHER_STREAK_LEVEL */

/* The fog table a sky pixel (x, y) goes through: the sky's own `low` high above the horizon, `high` at it and below, a
 * dither of the two between, so that what the fog has made whole (land and cars past the fog's end) meets the sky without
 * an edge. `up` is the world's up in camera space (x right, y up, z ahead), the view's centre cx, cy and focal in pixels;
 * the horizon is where a ray is level, whatever the pitch or roll. */
int weather_sky_level(int32_t x, int32_t y, int32_t cx, int32_t cy, float focal, const float up[3], int low, int high);

/* The fog's curve: 1 an exponential that takes the same level at 2500 (about 8 of 15) as the line and goes on toward
 * WEATHER_FOG_EXP_CAP of the way without reaching the whole, so the far cars keep a trace of shading (the owner's decision, the
 * default); 0 the line from WEATHER_FOG_START to WEATHER_FOG_END (the fog is whole from there on: a plateau, the cars past it flat),
 * for comparison: TNFS_FOG_CURVE=line. */
int weather_fog_curve(void);

/* With the PS1 light's time of day the 3D's pixels go through the tinted palette whole, which turns the fog into a saturated soup. The fog
 * is made at half the tint instead (as the dash is, the owner's decision): its colour is set so that after the tint it is the day's one with
 * half the tint. */
/* `fog` for a palette that will be multiplied by `tint` (per channel): fog x (1 + tint) / 2 / tint, at most 255 a channel (a tint far
 * under a half cannot be made up for). `out` may be `fog`. */
void weather_fog_for_tint(const uint8_t fog[3], const float tint[3], uint8_t out[3]);
enum { WEATHER_FOG_EXP_SCALE = 2380, WEATHER_FOG_EXP_CAP_PERCENT = 93 };

enum {
    WEATHER_SKY_FADE_TAN = 20,     /* a hundredth of the ray's rise (over its run) where the sky is whole its own: 0.20 */
    WEATHER_SKY_LEVEL = 11,        /* of MODERN_FOG_LEVELS - 1 = 15: how far the sky goes to its colour */
    WEATHER_RAIN_SKY_LEVEL = 8,
    WEATHER_STREAK_LEVEL = 7,
    WEATHER_FOG_END = 4200,        /* the camera depth where the fog is whole (the track's: about 780 a segment drawn) */
    WEATHER_FOG_START = 250,
    WEATHER_RAIN_FOG_END = 12000,  /* rain's haze, only where Modern 3D is on */
    WEATHER_RAIN_FOG_START = 1500
};

/* The streaks of a view box_w x box_h (pixels) at `tick`: how many, and streak i as the line tail (x0, y0) .. head (x1, y1)
 * relative to the box (the ends may lie outside it; the caller clips). The box is in the pixels of the buffer drawn, so at a render
 * scale the same drop is the same fraction of the box and its length grows with the box; only its width is `scale` pixels. */
int weather_rain_count(int32_t box_w, int32_t box_h);
int weather_rain_count_fx(int32_t box_w, int32_t box_h);   /* port (rain_fx): the same box's far fewer streaks of the speed-following rain */
void weather_rain_streak(int drop, uint32_t tick, int32_t box_w, int32_t box_h, int32_t out[4]);
int weather_rain_width(int scale);   /* pixels across */

/* port (rain_fx): the streaks follow the car. A drop's streak at `tick` for a car at `speed` (16.16 metres a second, signed) in a box_w x box_h box whose vanishing point
 * is (vx, vy) (box pixels) and whose lens has `focal` pixels: a standing car's is today's, a falling streak; as the car goes faster it turns to a spoke that leaves the
 * vanishing point outward (all the way at WEATHER_SPOKE_FULL metres a second), longer the faster and the nearer the edge. `out` is tail (x0, y0) .. head (x1, y1)
 * as weather_rain_streak's. A pure function of its arguments: the drop's own numbers do not depend on the speed, so it changes smoothly as the speed does.
 * The streak's brightness `*level` (0 dark .. WEATHER_RAIN_LEVELS - 1 white): by day WEATHER_RAIN_DAY_LEVEL (the phase 1 colour); at night (`night`) by how much of the
 * headlight cone the drop is in (weather_rain_cone_weight at the place the drop is taken to be). */
enum { WEATHER_SPOKE_FULL = 40, WEATHER_SPOKE_TURNED = 12, WEATHER_RAIN_LEVELS = 4, WEATHER_RAIN_DAY_LEVEL = 1 };
float weather_rain_blend(int32_t speed);   /* 0 a standing car's falling streak .. 1 a spoke (smooth in the speed, 16.16 m/s); the drawing fades the streaks in near the vanishing point by it */
void weather_rain_spoke(int drop, uint32_t tick, int32_t speed, int32_t box_w, int32_t box_h, int32_t vx, int32_t vy, float focal, int night, int32_t out[4], int *level);

/* port (rain_fx, rain v3): drops on the lens: a drop lands, grows over a few frames, stays and slides a little (down when the car stands, out from the vanishing point as it goes), then
 * shrinks and fades. WEATHER_LENS_PER_640x480 of them in a 640 x 480 view, each with a life of its own and a place of its own every life. weather_lens_drop gives drop `i` at `tick`: 1 and
 * `out` { x, y, radius (pixels), keep (0..16: how much of it is left, a dither) } in the box, or 0 when it is not there now. A pure function of its arguments. */
enum { WEATHER_LENS_PER_640x480 = 7, WEATHER_LENS_MAX = 40 };
int weather_lens_count(int32_t box_w, int32_t box_h);
int weather_lens_drop(int drop, uint32_t tick, int32_t speed, int32_t box_w, int32_t box_h, int32_t vx, int32_t vy, int32_t out[4]);

/* port (rain_fx): the spray a car at `speed` (16.16 m/s) throws from each rear wheel: WEATHER_SPRAY_PUFFS puffs per wheel strung out behind it, a plume that hangs in the
 * air behind the car as it goes (not left where they were dropped: a chase camera would drive through them). Puff `k` of the wheel on `side` (-1 left, 1 right) at `tick`:
 * `out` its place relative to the wheel in 16.16 metres { behind, to the side (outward), up } and `*size` (0x100 a standard puff, 0 none). It has none below 8 m/s, on the
 * shoulder or in the air (`wet_road` 0). The plume lengthens with the speed and thins out along its length; the same arguments give the same puff. */
enum { WEATHER_SPRAY_PUFFS = 22 };
#define WEATHER_SPRAY_CAP 1.5f   /* a puff grows as it goes from the wheel up to this many times the standard size */
int weather_rain_spray_puff(int k, int side, uint32_t tick, int32_t speed, int wet_road, int32_t out[3], int32_t *size);

/* port (rain_lightning, extras "Rain with storm"): the storm's clock. The lightning's brightness at `tick` (30 a second), 0 (none) .. WEATHER_FLASH_PEAK (the whole sky white): full on the
 * strike's first tick, then decaying over WEATHER_FLASH_TICKS. A pure function of the tick, so a replay or a screenshot at the same tick flashes the same. Every WEATHER_STORM_SLOT ticks
 * (15 s) three slots in four have a strike at a place of its own in the slot, one in three flashing a second, weaker time WEATHER_STORM_DOUBLE ticks after the first; none in the first 5
 * seconds. The sky is painted white by it, and the palette goes toward full bright by it (drive_light_set_flash). */
enum { WEATHER_STORM_SLOT = 450, WEATHER_STORM_DOUBLE = 18, WEATHER_FLASH_TICKS = 10, WEATHER_FLASH_PEAK = 15 };
int weather_rain_flash(uint32_t tick);
void weather_flash_colour(uint8_t out[3]);   /* the lightning's: a cold white-blue */

/* The brightness level (0..WEATHER_RAIN_LEVELS - 1) a spray puff is drawn at: by day white, at night a mid level (it lies behind the car, outside the headlights' cone). */
int weather_rain_spray_level(int night);

/* The fog level (of the 16) of each brightness level's table, from dark to white. */
int weather_rain_level_fog(int level);

/* The headlight cone's weight 0..256 at a point `ahead` of the eye, `lateral` to the right of it and `height` above the road, metres: headlight_cone_weight_local (headlight_pool.h)
 * at that point in the car's own frame (the eye taken EYE_FORWARD_M ahead of the car's centre): the drops are lit where the road is. */
int weather_rain_cone_weight(float ahead, float lateral, float height);

/* A lamp's reflection on a wet road, in percent (0..100): it shows only while the lamp faces the viewer and near it. `dot` is the car's forward vector with the car-to-camera one (> 0 the front
 * faces the camera, < 0 the rear): the weight is smooth in |dot| from 0.3 (none, a side view) to 0.7 (full), and `distance` (metres) is full to WEATHER_REFLECT_NEAR_M and none from WEATHER_REFLECT_FAR_M. */
enum { WEATHER_REFLECT_NEAR_M = 25, WEATHER_REFLECT_FAR_M = 75 };
int weather_reflection_weight(float dot, float distance);

#endif

/* port (weather): what a frame of weather does besides draw the sky and the rain: the wipers' blades (the dash view's, in the 3D buffer's pixels) and the table they are drawn with, the thunder after
 * each strike, the lightning's brightness, and the rain's and the snow's roll voices of the car the camera follows. The caller says what the frame's weather is (WeatherFrame). */
#ifndef TNFS_WEATHER_HOOKS_H
#define TNFS_WEATHER_HOOKS_H

#include "game/race/race_state.h"
#include "game/race/wiper.h"
#include "game/viewer/ported_view.h"

#include <stdint.h>

typedef struct {
    int weather;                 /* WEATHER_* */
    int snow_level;              /* SNOW_NONE .. SNOW_HEAVY: what falls on this race */
    int rain_open;               /* 0..16: how much weather there is where the eye is */
    const RaceState *race;       /* NULL outside a race */
    int32_t anim_tick;           /* the clock when there is no race */
    int (*open_at)(int32_t node);   /* how much weather there is at a road node, 0..16 (a car's spray in a tunnel); NULL: rain_open everywhere */
} WeatherFrame;

/* ported_view_set_wipers / ported_view_set_wiper_glass (ported_view.h) keep the scene here; the number of blades set */
int weather_hooks_wiper_blades(void);

/* the passes in the box's own pixels for this frame, NULL when there are none (no blades, nothing falls) */
const WiperPass *weather_hooks_wiper_pass(WiperPass *out, const int32_t box[4], const WeatherFrame *f);

/* the darkening table the blades are drawn with, made again when the palette changes */
const uint8_t *weather_hooks_wiper_dark(const uint8_t palette[768]);

/* the thunder after each strike since the last call (once a frame) */
void weather_hooks_thunder(const WeatherFrame *f);

/* the rain's layers of the followed car with every car's spray and splashes (weather_rain.c), and the roll on snow of the followed car, once a frame */
void weather_hooks_rain_roll(const WeatherFrame *f);
void weather_hooks_snow_roll(const WeatherFrame *f);

/* the lightning's brightness this frame, 0 none */
int weather_hooks_flash_level(const WeatherFrame *f);

#endif

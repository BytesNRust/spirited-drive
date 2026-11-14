#include "game/race/rain_cars.h"

#include "game/race/rain_roll.h"
#include "game/race/tick_rate.h"

#include <math.h>
#include <string.h>

uint32_t rain_dice(uint32_t *seed)
{
    uint32_t x = *seed != 0 ? *seed : 1;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    return *seed = x;
}

/* a draw in 0..1 */
static double unit(uint32_t *seed) { return (rain_dice(seed) >> 8) / (double)(1u << 24); }

int rain_puddle_millihertz(int32_t speed)
{
    const int64_t v = speed < 0 ? -(int64_t)speed : speed;
    if (v <= RAIN_PUDDLE_FROM) return 0;
    if (v >= RAIN_ROLL_FULL) return RAIN_PUDDLE_TOP_MILLIHERTZ;
    return (int)(RAIN_PUDDLE_TOP_MILLIHERTZ * (v - RAIN_PUDDLE_FROM) / (RAIN_ROLL_FULL - RAIN_PUDDLE_FROM));
}

int rain_puddle_roll(uint32_t *seed, int32_t speed, uint32_t ticks, int clips, int last, RainPuddle *out)
{
    const double chance = 1.0 - exp(-rain_puddle_millihertz(speed) / 1000.0 * ticks / RACE_TICKS_A_SECOND);   /* a Poisson rate: at least one in `ticks` */
    const double hit = unit(seed), clip = unit(seed), gain = unit(seed), pitch = unit(seed);
    if (clips <= 0 || hit >= chance) return 0;
    int c = (int)(clip * (clips > 1 ? clips - 1 : 1));
    if (clips > 1 && c >= last && last >= 0 && last < clips) c++;   /* never the same one twice in a row */
    out->clip = c < clips ? c : clips - 1;
    const double low_db = 20.0 * log10(RAIN_PUDDLE_GAIN_LOW / 1000.0), high_db = 20.0 * log10(RAIN_PUDDLE_GAIN_HIGH / 1000.0);
    out->gain_permille = (int)lround(1000.0 * pow(10.0, (low_db + (high_db - low_db) * gain) / 20.0));
    out->pitch_permille = 1000 - RAIN_PUDDLE_PITCH_SPREAD + (int)lround(2 * RAIN_PUDDLE_PITCH_SPREAD * pitch);
    return 1;
}

void rain_car_voices_clear(RainCarVoices *v)
{
    for (int i = 0; i < RAIN_CARS_VOICES; i++) v->car[i] = -1, v->fresh[i] = 0, v->free_at[i] = INT32_MIN / 2;
    v->last_tick = INT32_MIN / 2;
}

static int among(const int *wanted, int n, int car)
{
    for (int i = 0; i < n; i++)
        if (wanted[i] == car) return 1;
    return 0;
}

void rain_car_voices_assign(RainCarVoices *v, const int *wanted, int n, int32_t tick)
{
    if (tick < v->last_tick) rain_car_voices_clear(v);   /* the clock went back */
    v->last_tick = tick;
    for (int i = 0; i < RAIN_CARS_VOICES; i++) v->fresh[i] = 0;
    const int keep = n < RAIN_CARS_VOICES + 1 ? n : RAIN_CARS_VOICES + 1, take = n < RAIN_CARS_VOICES ? n : RAIN_CARS_VOICES;
    for (int i = 0; i < RAIN_CARS_VOICES; i++)
        if (v->car[i] >= 0 && !among(wanted, keep, v->car[i])) v->car[i] = -1, v->free_at[i] = tick;
    for (int k = 0; k < take; k++) {
        if (among(v->car, RAIN_CARS_VOICES, wanted[k])) continue;
        for (int i = 0; i < RAIN_CARS_VOICES; i++)
            if (v->car[i] < 0 && (int64_t)tick - v->free_at[i] >= RAIN_CARS_REST_TICKS) {
                v->car[i] = wanted[k], v->fresh[i] = 1;
                break;
            }
    }
}

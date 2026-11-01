/* port: see difficulty.h. */
#include "game/race/difficulty.h"

#include <stddef.h>

enum {
    PARAMS = 107,
    PARAM_PACK_LEAD = 6, PARAM_STOPS = 7, PARAM_TRAFFIC_FIRST = 14, TRAFFIC_FACTORS = 4,
    BAND_FIRST = 23, BAND_LAST = 64,        /* the catch-up rows by the gap: 23..43 a race, 44..64 the others (ai_manager.c) */
    EASY_BAND_NUM = 85, EASY_BAND_CAP = 0x14ccc,   /* x0.85, at most 1.3 */
    EASY_LEAD_NUM = 3, EASY_LEAD_DEN = 4,
    EASY_STOPS = 800, HARD_STOPS = 200,     /* the exe's 400: a stop inside the window counts as the second kind */
    EASY_TRAFFIC = 4,
    LEVEL_BITS = 2, STAMP_SHIFT = 2,         /* the stamp's bits in the port's cheat byte, after EXPLODE and NITRO */
    FLAT = 0x10000,
    HARD_BEND_NUM = 13, HARD_BEND_DEN = 10,
    HARD_STOPS_TO_END = 1, EASY_STOPS_TO_END = 2
};

static const int32_t EASY_FACTORS[TRAFFIC_FACTORS] = { 0x10000, 0xf333, 0xd999, 0xe666 };   /* 1, .95, .85, .9 (the exe's 1, .9, .7, .8) */
static const int32_t HARD_FACTORS[TRAFFIC_FACTORS] = { 0xe666, 0xcccc, 0x9999, 0xb333 };    /* .9, .8, .6, .7 */
static const int32_t EASY_ODDS[4] = { 16, 32, 48, 64 };
static const int32_t HARD_ODDS[4] = { 48, 96, 128, 128 };

static int32_t level_of(int32_t axis)
{
    return axis == DIFFICULTY_EASY || axis == DIFFICULTY_HARD ? axis : DIFFICULTY_ORIGINAL;
}

RaceDifficulty difficulty_from_extras(int level)
{
    const int32_t axis = level == 0 ? DIFFICULTY_EASY : level == 2 ? DIFFICULTY_HARD : DIFFICULTY_ORIGINAL;
    const RaceDifficulty d = { axis, axis, axis };
    return d;
}

int difficulty_is_original(const RaceDifficulty *d)
{
    return level_of(d->rivals) == DIFFICULTY_ORIGINAL && level_of(d->police) == DIFFICULTY_ORIGINAL &&
           level_of(d->traffic) == DIFFICULTY_ORIGINAL;
}

RaceDifficulty difficulty_resolve(int extras_level, const RaceDifficulty *replay, int linked)
{
    const RaceDifficulty original = { DIFFICULTY_ORIGINAL, DIFFICULTY_ORIGINAL, DIFFICULTY_ORIGINAL };
    if (linked) return original;
    if (replay != NULL) {
        const RaceDifficulty r = { level_of(replay->rivals), level_of(replay->police), level_of(replay->traffic) };
        return r;
    }
    return difficulty_from_extras(extras_level);
}

unsigned difficulty_stamp(const RaceDifficulty *d)
{
    return ((unsigned)level_of(d->rivals) | (unsigned)level_of(d->police) << LEVEL_BITS | (unsigned)level_of(d->traffic) << 2 * LEVEL_BITS)
           << STAMP_SHIFT;
}

RaceDifficulty difficulty_unstamp(unsigned stamp)
{
    const unsigned s = stamp >> STAMP_SHIFT;
    const RaceDifficulty d = { level_of((int32_t)(s & 3)), level_of((int32_t)(s >> LEVEL_BITS & 3)), level_of((int32_t)(s >> 2 * LEVEL_BITS & 3)) };
    return d;
}

void difficulty_apply_params(const RaceDifficulty *d, int32_t params[PARAMS])
{
    if (d->rivals == DIFFICULTY_EASY) {
        for (int i = BAND_FIRST; i <= BAND_LAST; i++) {
            const int32_t v = (int32_t)((int64_t)params[i] * EASY_BAND_NUM / 100);
            params[i] = v > EASY_BAND_CAP ? EASY_BAND_CAP : v;
        }
        params[PARAM_PACK_LEAD] = params[PARAM_PACK_LEAD] * EASY_LEAD_NUM / EASY_LEAD_DEN;
    } else if (d->rivals == DIFFICULTY_HARD) {
        for (int i = BAND_FIRST; i <= BAND_LAST; i++) params[i] = FLAT;
    }
    if (d->police == DIFFICULTY_EASY) params[PARAM_STOPS] = EASY_STOPS;
    else if (d->police == DIFFICULTY_HARD) params[PARAM_STOPS] = HARD_STOPS;
    if (d->traffic == DIFFICULTY_EASY || d->traffic == DIFFICULTY_HARD)
        for (int i = 0; i < TRAFFIC_FACTORS; i++)
            params[PARAM_TRAFFIC_FIRST + i] = d->traffic == DIFFICULTY_EASY ? EASY_FACTORS[i] : HARD_FACTORS[i];
}

int32_t difficulty_traffic_count(const RaceDifficulty *d, int32_t count)
{
    return d->traffic == DIFFICULTY_EASY && count > EASY_TRAFFIC ? EASY_TRAFFIC : count;
}

int32_t difficulty_police_odds(const RaceDifficulty *d, int kind, int32_t exe)
{
    if (kind < 0 || kind > 3) return exe;
    return d->police == DIFFICULTY_EASY ? EASY_ODDS[kind] : d->police == DIFFICULTY_HARD ? HARD_ODDS[kind] : exe;
}

int32_t difficulty_stops_to_end(const RaceDifficulty *d, int32_t exe)
{
    return d->police == DIFFICULTY_EASY ? EASY_STOPS_TO_END : d->police == DIFFICULTY_HARD ? HARD_STOPS_TO_END : exe;
}

int difficulty_rivals_flat(const RaceDifficulty *d)
{
    return d->rivals == DIFFICULTY_HARD;
}

int32_t difficulty_bend_scale(const RaceDifficulty *d, int32_t exe)
{
    return d->rivals == DIFFICULTY_HARD ? (int32_t)((int64_t)exe * HARD_BEND_NUM / HARD_BEND_DEN) : exe;
}

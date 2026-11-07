/* port: see rally_gravel.h. */
#include "game/race/rally_gravel.h"

enum {
    DUST_FROM = 0x3c0000,     /* skid_marks.c's SKID_FROM: the least skid that lays a mark */
    DUST_BASE = 0x600000,     /* the puffs' size grows with the skid (tyre_smoke.c; the tyres' own tops out at 0x960000): a cloud near a hard slide's */
    DUST_MOVING = 0x50000,    /* 5 metres a second: slower than that the dust does not rise */
    DUST_TOP = 0x1e00000      /* the speed the dust stops growing at */
};

int rally_gravel_kind(int track, int variant_r, int lunar, int enabled, int snow)
{
    if (variant_r && snow) return RALLY_GRAVEL_SNOW;
    if (!enabled || !variant_r) return RALLY_GRAVEL_OFF;
    if (track == 3) return lunar ? RALLY_GRAVEL_REGOLITH : RALLY_GRAVEL_DUST;
    return RALLY_GRAVEL_COMPACT;
}

int32_t rally_gravel_level(int kind, int32_t level)
{
    switch (kind) {
    case RALLY_GRAVEL_COMPACT:
    case RALLY_GRAVEL_SNOW: return level * 3 / 4;
    case RALLY_GRAVEL_DUST: return level * 2 / 3;
    case RALLY_GRAVEL_REGOLITH: return level / 3;
    default: return level;
    }
}

int32_t rally_gravel_crunch(int kind, int32_t speed, uint32_t rand)
{
    enum { MIN_SPEED = 2, TOP_SPEED = 30, SLOWEST = 8, SPEED_PER_STEP = 6, VOLUME_BASE = 0x50, VOLUME_PER_METRE = 2, VOLUME_TOP = 0x7f };
    if (kind == RALLY_GRAVEL_OFF) return 0;
    const int32_t metres = (speed < 0 ? -speed : speed) >> 16;
    if (metres < MIN_SPEED) return 0;
    const int32_t period = SLOWEST - (metres > TOP_SPEED ? TOP_SPEED : metres) / SPEED_PER_STEP;   /* a strike every `period` steps (30 a second) on average: 4 a second slowly, 10 flat out; the exe's own is 1 in 30 */
    if ((int32_t)(((rand & 0xffffu) * (uint32_t)period) >> 16) != 0) return 0;
    int32_t volume = VOLUME_BASE + VOLUME_PER_METRE * (metres > TOP_SPEED ? TOP_SPEED : metres);
    if (volume > VOLUME_TOP) volume = VOLUME_TOP;
    return kind == RALLY_GRAVEL_REGOLITH ? volume / 2 : kind == RALLY_GRAVEL_DUST ? volume * 3 / 4 : volume;
}

int32_t rally_gravel_tyre_skid(int kind, int32_t speed, int32_t skid, int on_ground)
{
    if (kind == RALLY_GRAVEL_OFF || !on_ground || (speed >> 16) < RALLY_TYRE_MOVING) return skid;
    const int32_t roll = (speed >> 16) * RALLY_TYRE_PER_METRE_X2 / 2;
    const int32_t floor = roll > RALLY_TYRE_TOP ? RALLY_TYRE_TOP : roll;
    return skid > floor ? skid : floor;
}

int32_t rally_gravel_roll_distance(int32_t distance)
{
    enum { HEARD_WITHIN = 0x200000 };   /* race_sound_loudness's range */
    if (distance >= HEARD_WITHIN) return distance;
    if (distance < 0) distance = 0;
    return HEARD_WITHIN - ((HEARD_WITHIN - distance) >> 1);
}

int32_t rally_dust_skid(int kind, int32_t speed, int on_ground)
{
    if (!on_ground || speed < DUST_MOVING || (kind != RALLY_GRAVEL_DUST && kind != RALLY_GRAVEL_REGOLITH && kind != RALLY_GRAVEL_SNOW)) return 0;
    const int32_t v = speed > DUST_TOP ? DUST_TOP : speed;
    return kind == RALLY_GRAVEL_DUST || kind == RALLY_GRAVEL_SNOW ? DUST_BASE + v / 2 : DUST_FROM + 0x40000 + v / 16;
}

/* the floor a car at `speed` (16.16 m/s) gets: all of it from 3 m/s, in proportion below, none standing */
static int32_t floor_at(int32_t floor, int32_t speed)
{
    const int64_t sp = speed < 0 ? -(int64_t)speed : speed, full = 3 << 16;
    return sp >= full ? floor : (int32_t)(floor * sp / full);
}

int32_t rally_gravel_shoulder_level(int kind, int32_t level, int32_t speed)
{
    if (kind == RALLY_GRAVEL_OFF) return level;
    const int32_t floor = floor_at(RALLY_SHOULDER_FLOOR, speed);
    return level < floor ? floor : level;
}

int32_t rally_gravel_road_level(int kind, int32_t level, int32_t speed)
{
    if (kind == RALLY_GRAVEL_OFF) return level;
    const int32_t floor = floor_at(RALLY_ROAD_FLOOR, speed);
    return level < floor ? floor : level;
}

/* port: see rally_gravel.h. */
#include "game/race/rally_gravel.h"

enum {
    DUST_FROM = 0x3c0000,     /* skid_marks.c's SKID_FROM: the least skid that lays a mark */
    DUST_BASE = 0x600000,     /* the puffs' size grows with the skid (tyre_smoke.c; the tyres' own tops out at 0x960000): a cloud near a hard slide's */
    DUST_MOVING = 0x50000,    /* 5 metres a second: slower than that the dust does not rise */
    DUST_TOP = 0x1e00000      /* the speed the dust stops growing at */
};

int rally_gravel_kind(int track, int variant_r, int lunar, int enabled)
{
    if (!enabled || !variant_r) return RALLY_GRAVEL_OFF;
    if (track == 3) return lunar ? RALLY_GRAVEL_REGOLITH : RALLY_GRAVEL_DUST;
    return RALLY_GRAVEL_COMPACT;
}

int32_t rally_gravel_level(int kind, int32_t level)
{
    switch (kind) {
    case RALLY_GRAVEL_COMPACT: return level * 3 / 4;
    case RALLY_GRAVEL_DUST: return level * 2 / 3;
    case RALLY_GRAVEL_REGOLITH: return level / 3;
    default: return level;
    }
}

int32_t rally_dust_skid(int kind, int32_t speed, int on_ground)
{
    if (!on_ground || speed < DUST_MOVING || (kind != RALLY_GRAVEL_DUST && kind != RALLY_GRAVEL_REGOLITH)) return 0;
    const int32_t v = speed > DUST_TOP ? DUST_TOP : speed;
    return kind == RALLY_GRAVEL_DUST ? DUST_BASE + v / 2 : DUST_FROM + 0x40000 + v / 16;
}

int32_t rally_gravel_shoulder_level(int kind, int32_t level, int32_t speed)
{
    const int32_t rolling = speed >= 0x10000 || speed <= -0x10000;
    if (kind == RALLY_GRAVEL_OFF || !rolling || level >= RALLY_SHOULDER_FLOOR) return level;
    return RALLY_SHOULDER_FLOOR;
}

int32_t rally_gravel_road_level(int kind, int32_t level, int32_t speed)
{
    const int32_t rolling = speed >= 0x10000 || speed <= -0x10000;
    if (kind == RALLY_GRAVEL_OFF || !rolling || level >= RALLY_ROAD_FLOOR) return level;
    return RALLY_ROAD_FLOOR;
}

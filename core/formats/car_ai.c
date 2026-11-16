/* port (dep59 3b): formats/car_ai.h. The formula is the line the plan measured between MODEL_PACE (004c5ce0) and the eight cars'
 * sixth gear limit: every car lands within 2300 of its own pace (tests/test_car_ai.c prints the table). The levels are four of the cars' pairs:
 * slow ANSX, medium P911, fast DVIPER, top F512TR. */
#include "formats/car_ai.h"

#include <stdio.h>
#include <string.h>

enum { TOP_GEAR_LIMIT = PDN_FACTORS + 4 * 5, PACE_BASE = 39478, PACE_PER_TOP = 289 };

static const struct { const char *name; CarAiTuning tuning; } LEVELS[] = {
    { "slow", { 58982, 4 } }, { "medium", { 60948, 4 } }, { "fast", { 62259, 5 } }, { "top", { 65536, 4 } },
};
enum { LEVEL_COUNT = sizeof LEVELS / sizeof LEVELS[0] };

/* the level's index, -1 for none (NULL or ""), LEVEL_COUNT for a word that is not one */
static int level_index(const char *level)
{
    if (level == NULL || level[0] == '\0') return -1;
    for (int i = 0; i < LEVEL_COUNT; i++)
        if (strcmp(level, LEVELS[i].name) == 0) return i;
    return LEVEL_COUNT;
}

int car_ai_level_ok(const char *level)
{
    return level_index(level) < LEVEL_COUNT;
}

int car_ai_tuning(const uint8_t pdn[PDN_SIZE], const char *level, CarAiTuning *out, char *why, size_t why_cap)
{
    const int at = level_index(level);
    if (at == LEVEL_COUNT) {
        if (why != NULL && why_cap > 0) snprintf(why, why_cap, "ai_level=%s is not slow, medium, fast or top", level);
        return 0;
    }
    if (at >= 0) { *out = LEVELS[at].tuning; return 1; }
    const int64_t top = pdn_i32(pdn, TOP_GEAR_LIMIT);
    int64_t pace = PACE_BASE + ((top * PACE_PER_TOP) >> 16);   /* 64 bits: no top overflows it */
    if (pace < CAR_AI_PACE_MIN) pace = CAR_AI_PACE_MIN;
    if (pace > CAR_AI_PACE_MAX) pace = CAR_AI_PACE_MAX;
    out->pace = (int32_t)pace;
    out->coast = CAR_AI_COAST_DEFAULT;
    return 1;
}

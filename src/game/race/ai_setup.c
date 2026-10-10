#include "game/race/ai_setup.h"

#include "game/race/ai_profiles.h"

enum { TRACK_FLAG_4 = 4 };

/* 00470238 by the first car's model (type 3): 0, 6, 7 -> 2; 1, 4 -> 0; 2, 3, 5 -> 1 */
static const int8_t ROW_BY_MODEL[8] = { 2, 0, 1, 1, 0, 1, 2, 2 };

int32_t ai_profile_row(const RaceState *race, int32_t fallback)
{
    switch ((uint32_t)race->race_type) {
    case 0:
        return 7;
    case 1:
        return race->track_flags & TRACK_FLAG_4 ? 6 : 5;
    case 2:
        if (race->cars[2] == NULL || race->cars[3] == NULL) return fallback & 0xff;  /* port guard */
        return race->cars[2]->model == race->cars[3]->model ? 4 : 3;
    case 3:
        if (race->cars[0] == NULL) return fallback & 0xff;  /* port guard */
        if ((uint32_t)race->cars[0]->model <= 7) return ROW_BY_MODEL[race->cars[0]->model];
        return fallback & 0xff;
    default:
        return fallback & 0xff;
    }
}

int32_t ai_driver_letter(int32_t place, int32_t row)
{
    const uint32_t at = (uint32_t)place + (uint32_t)row * 8;
    if (at >= AI_DRIVER_DATA_SIZE) return -1;  /* port guard: the original reads past the table */
    return AI_DRIVER_DATA[at] - 'A';
}

uint32_t ai_profile_handle(const RaceState *race, int32_t index)
{
    return race->driver_profiles_base + (uint32_t)index * AI_DRIVER_PROFILE_SIZE;
}

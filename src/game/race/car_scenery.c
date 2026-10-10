#include "game/race/car_scenery.h"

#include "game/sys/fixed_math.h"

enum {
    SCENERY_RECORD = 0x10,
    SCENERY_END = -1,
    JOLT_SCALE = 0x23d70,       /* speed to the jolt's units */
    JOLT_PERIOD_TOP = 200,
    JOLT_PERIOD_LEAST = 0x50,
    JOLT_STRENGTH_BASE = 0x46,
    JOLT_STRENGTH_MOST = 100,
    JOLT_FROM_LEFT = 0x13b,
    JOLT_FROM_RIGHT = 0x2d,
    STREAM_NONE = -1,
    RESTREAM = 1                /* render_flags */
};

/* The node word of record `index`; the original reads it unchecked. */
static int32_t record_node(const Tri *tri, int32_t index)
{
    const size_t at = tri->scenery_offset + (size_t)index * SCENERY_RECORD;
    if (index < 0 || at + 4 > tri->size) return SCENERY_END;  /* port guard */
    const uint8_t *p = tri->data + at;
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

int32_t car_scenery_at(Car *car, const Tri *tri, int32_t node)
{
    if (node < car->scenery_node || record_node(tri, car->scenery_index + 1) == SCENERY_END) {
        if (node < car->scenery_node) {
            while (car->scenery_index != 0) {
                if (node == car->scenery_node) return 1;
                car->scenery_index--;
                car->scenery_node = record_node(tri, car->scenery_index);
                if (car->scenery_node < node) return 0;
            }
        }
        return 0;
    }
    do {
        if (node == car->scenery_node) return 1;
        car->scenery_index++;
        car->scenery_node = record_node(tri, car->scenery_index);
    } while (car->scenery_node <= node && record_node(tri, car->scenery_index + 1) != SCENERY_END);
    return 0;
}

void car_scenery_jolt(const Car *car, const RaceState *race, int32_t across)
{
    int32_t speed = race->viewed < race->humans ? car->local_velocity_z : car->ai_speed;
    if (speed < 1) speed = -speed;
    const int32_t v = fx_mul_round(speed, JOLT_SCALE);
    uint32_t period = (uint32_t)(JOLT_PERIOD_TOP - (v >> 16));
    if (period < JOLT_PERIOD_LEAST) period = JOLT_PERIOD_LEAST;
    uint32_t strength = (uint32_t)((v >> 17) + JOLT_STRENGTH_BASE);
    if (strength > JOLT_STRENGTH_MOST) strength = JOLT_STRENGTH_MOST;
    if (race->sounds.tone != NULL)
        race->sounds.tone(race->sounds.user, (int32_t)strength, across < 1 ? JOLT_FROM_LEFT : JOLT_FROM_RIGHT, (int32_t)period);
}

void race_scenery_restream(RaceState *race)
{
    race->scenery_stream_node = STREAM_NONE;
    race->scenery_stream_segment = STREAM_NONE;
    race->render_flags |= RESTREAM;
}

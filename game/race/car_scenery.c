#include "game/race/car_scenery.h"

#include "game/race/car_walls.h"

#include "game/sys/fixed_math.h"

#include <string.h>

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
    RESTREAM = 1,               /* render_flags */
    HEARD_NODES = 9,            /* a hit this near the camera's node sounds */
    HIT_SOUND = 10,
    MINUTE = 0x708              /* race ticks */
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
    race->chase.flags |= RESTREAM;
}

int32_t car_scenery_hit(const Car *car, RaceState *race, const int32_t point[3])
{
    const int32_t yaw = car->yaw >> 8;
    const int32_t c = fx_cos16(yaw), s = fx_sin16(yaw);
    const int32_t dx = (int32_t)((uint32_t)point[0] - (uint32_t)car->x), dz = (int32_t)((uint32_t)point[2] - (uint32_t)car->z);
    const int32_t side = fx_mul_round(c, dx) - fx_mul_round(s, dz);
    const int32_t half_width = car->model_width >> 1;
    if (half_width < side || side < -half_width) return 0;
    const int32_t ahead = fx_mul_round(s, dx) + fx_mul_round(c, dz);
    const int32_t half_length = car->model_length >> 1;
    if (ahead > half_length || ahead < -half_length) return 0;
    if (race->effects != 0) car_scenery_jolt(car, race, side);
    const int32_t near = car->node - race->chase.node;
    if (near <= HEARD_NODES && near > -HEARD_NODES && race->sounds.sound != NULL) {
        int32_t angle, distance;
        const int32_t at[3] = { car->x, car->y, car->z };
        race_sound_position(race, at, &angle, &distance);
        race->sounds.sound(race->sounds.user, -1, 2, HIT_SOUND, 0x10000, distance, angle);
    }
    return 1;
}

/* The int16 at `offset` in scenery record `index`, as the original reads it
 * (a word's high half). */
static int32_t record_half(const Tri *tri, int32_t index, size_t offset)
{
    const size_t at = tri->scenery_offset + (size_t)index * SCENERY_RECORD + offset;
    if (index < 0 || at + 2 > tri->size) return 0;  /* port guard */
    return (int16_t)(tri->data[at] | tri->data[at + 1] << 8);
}

/* A standing record at the car's node knocked over, if the car hits it. */
static int knocks(Car *car, RaceState *race, int32_t node)
{
    const int32_t index = car->scenery_index;
    if (node != car->scenery_node || index < 0 || index >= TRI_SCENERY_MAX ||  /* port guard */
        race->scenery_state[2 * index] != 0)
        return 0;
    TriNode n;
    if (tri_node(race->tri, (uint32_t)node, &n) != TRI_OK) memset(&n, 0, sizeof n);  /* port guard: the original reads on unmasked */
    const int32_t point[3] = {
        (int32_t)((uint32_t)n.position.x + ((uint32_t)record_half(race->tri, index, 0xa) << 8)),
        (int32_t)((uint32_t)n.position.y + ((uint32_t)record_half(race->tri, index, 0xc) << 8)),
        (int32_t)((uint32_t)n.position.z + ((uint32_t)record_half(race->tri, index, 0xe) << 8))
    };
    if (!car_scenery_hit(car, race, point)) return 0;
    race->scenery_state[2 * index] = (uint8_t)(race->tick / MINUTE + 1);
    race_scenery_restream(race);
    return 1;
}

int32_t car_knocks_scenery(Car *car, RaceState *race)
{
    const Tri *tri = race->tri;
    const int32_t node = car->node;
    while (node == car->scenery_node && car->scenery_index != 0) {
        car->scenery_index--;
        car->scenery_node = record_node(tri, car->scenery_index);
    }
    if (node >= car->scenery_node && record_node(tri, car->scenery_index + 1) != SCENERY_END) {
        for (;;) {
            if (knocks(car, race, node)) return 1;
            car->scenery_index++;
            car->scenery_node = record_node(tri, car->scenery_index);
            if (node < car->scenery_node || record_node(tri, car->scenery_index + 1) == SCENERY_END) return 0;
        }
    }
    if (node >= car->scenery_node || car->scenery_index == 0) return 0;
    for (;;) {
        if (knocks(car, race, node)) return 1;
        car->scenery_index--;
        car->scenery_node = record_node(tri, car->scenery_index);
        if (node > car->scenery_node || car->scenery_index == 0) return 0;
    }
}

void race_scenery_keep(RaceState *race)
{
    if (race->unknown_4fbb0c == 0 || race->playback == 1) return;
    uint32_t slots = race->tri != NULL ? race->tri->scenery_slots : 0;
    if (slots > TRI_SCENERY_MAX) slots = TRI_SCENERY_MAX;  /* port guard: the buffer holds 2 x slots bytes */
    for (uint32_t i = 0; i < slots; i++) race->scenery_state[2 * i + 1] = race->scenery_state[2 * i];
}

/* port (body shine): see body_shine.h. */
#include "viewer/body_shine.h"

#include "viewer/object_sink.h"

#include <math.h>
#include <string.h>

enum { SLOTS = 16, CACHED = 2 };
static const float UNIT = 65536.0f;      /* the car's position is 16.16 metres */
static const float TURN_METRES = 40.0f;  /* a streak comes round to where it was after this much travel */
static const float JUMP_METRES = 30.0f;  /* a step longer than this (a reset, a replay's jump) is not travel */

static int enabled;

typedef struct {
    uint8_t palette[768];
    uint8_t tables[CAR_SHINE_LEVELS][256];
    int valid;
} Made;

static Made made[CACHED];
static int next_made;
static ShineTables current;

static struct {
    float odometer;
    float last[3];
    int started;
} travel[SLOTS];

void body_shine_enable(int on) { enabled = on != 0; }
int body_shine_enabled(void) { return enabled; }

static ShineTables tables_for(const uint8_t palette[768])
{
    for (int i = 0; i < CACHED; i++)
        if (made[i].valid && memcmp(made[i].palette, palette, 768) == 0) return made[i].tables;
    Made *m = &made[next_made];
    next_made = (next_made + 1) % CACHED;
    memcpy(m->palette, palette, 768);
    car_shine_tables(palette, m->tables);
    m->valid = 1;
    return m->tables;
}

static void travel_to(int slot, const Car *car)
{
    float now[3];
    for (int k = 0; k < 3; k++) now[k] = (float)car->body_position[k] / UNIT;
    if (travel[slot].started) {
        const float dx = now[0] - travel[slot].last[0], dy = now[1] - travel[slot].last[1], dz = now[2] - travel[slot].last[2];
        const float step = sqrtf(dx * dx + dy * dy + dz * dz);
        if (step < JUMP_METRES) travel[slot].odometer += step;
    }
    memcpy(travel[slot].last, now, sizeof now);
    travel[slot].started = 1;
}

void body_shine_begin(int slot, const Car *car, const uint8_t palette[768])
{
    current = NULL;
    if (!enabled || slot < 0 || slot >= SLOTS || car == NULL || palette == NULL) {
        object_sink_set_shine(NULL, 0);
        return;
    }
    travel_to(slot, car);
    current = tables_for(palette);
    object_sink_set_shine(current, body_shine_phase(slot));
}

void body_shine_end(void)
{
    current = NULL;
    object_sink_set_shine(NULL, 0);
}

ShineTables body_shine_tables(void) { return current; }

float body_shine_phase(int slot) { return slot >= 0 && slot < SLOTS ? travel[slot].odometer / TURN_METRES : 0.0f; }

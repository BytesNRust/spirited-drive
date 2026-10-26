/* port (body shine): see body_shine.h. */
#include "viewer/body_shine.h"

#include "game/race/car_shine.h"
#include "game/sys/fixed_math.h"
#include "viewer/object_sink.h"

#include <math.h>
#include <string.h>

enum { CACHED = 2 };

static int enabled, strength;

typedef struct {
    uint8_t palette[768];
    uint8_t tables[CAR_SHINE_LEVELS][256];
    int valid;
} Made;

static Made made[CACHED];
static int next_made;
static ShineTables current;
static float up_in_camera[3] = { 0, 1, 0 };

void body_shine_enable(int level)
{
    strength = level < 0 ? 0 : level > 3 ? 3 : level;
    enabled = strength != 0;
}

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

/* the world's up taken to the camera's space, as the weather's sky takes it (ported_view.c weather_sky) */
static void world_up(const int32_t camera_matrix[9], float out[3])
{
    const int32_t up_world[3] = { 0, 0x10000, 0 };
    int32_t c[3];
    mat3_apply(up_world, camera_matrix, c);
    const float len = sqrtf((float)c[0] * (float)c[0] + (float)c[1] * (float)c[1] + (float)c[2] * (float)c[2]);
    if (len <= 0) {
        out[0] = 0, out[1] = 1, out[2] = 0;
        return;
    }
    for (int k = 0; k < 3; k++) out[k] = (float)c[k] / len;
}

void body_shine_begin(const uint8_t palette[768], const int32_t camera_matrix[9])
{
    current = NULL;
    if (!enabled || palette == NULL || camera_matrix == NULL) {
        object_sink_set_shine(NULL, NULL);
        return;
    }
    current = tables_for(palette);
    world_up(camera_matrix, up_in_camera);
    object_sink_set_shine_strength(strength);
    object_sink_set_shine(current, up_in_camera);
}

void body_shine_end(void)
{
    current = NULL;
    object_sink_set_shine(NULL, NULL);
}

ShineTables body_shine_tables(void) { return current; }

const float *body_shine_up(void) { return up_in_camera; }

/* port (body shine): see body_shine.h. */
#include "viewer/body_shine.h"

#include "game/race/car_envmap.h"
#include "game/race/car_shine.h"
#include "game/sys/fixed_math.h"
#include "viewer/object_sink.h"

#include <math.h>
#include <string.h>

enum { CACHED = 2, ENVMAP = 4, MAX_STRENGTH = 4 };

static int enabled, strength;

typedef enum { KIND_CLASSIC, KIND_ENVMAP } Kind;

typedef struct {
    uint8_t palette[768];
    Kind kind;
    int valid;
    uint8_t tables[CAR_SHINE_LEVELS][256];                 /* the classic pass, strength 1..3 */
    uint8_t paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256];   /* the envmap, strength 4 */
    uint8_t paint_hard[CAR_SHINE_LEVELS][256], glass_hard[CAR_SHINE_LEVELS][256];
    EnvShine env;
} Made;

static Made made[CACHED];
static int next_made;
static ShineTables current;
static const EnvShine *current_env;
static float up_in_camera[3] = { 0, 1, 0 };

void body_shine_enable(int level)
{
    strength = level < 0 ? 0 : level > MAX_STRENGTH ? MAX_STRENGTH : level;
    enabled = strength != 0;
    if (strength == ENVMAP) car_envmap_image();   /* the image is made at start-up, from port-made values */
}

int body_shine_enabled(void) { return enabled; }

/* the tables for the palette in use, made or found: a palette is cached per kind, so the strength in use decides which set it has */
static Made *made_for(const uint8_t palette[768])
{
    const Kind kind = strength == ENVMAP ? KIND_ENVMAP : KIND_CLASSIC;
    for (int i = 0; i < CACHED; i++)
        if (made[i].valid && made[i].kind == kind && memcmp(made[i].palette, palette, 768) == 0) return &made[i];
    Made *m = &made[next_made];
    next_made = (next_made + 1) % CACHED;
    memcpy(m->palette, palette, 768);
    m->kind = kind;
    if (kind == KIND_ENVMAP) {
        car_envmap_tables(palette, m->paint, m->glass);
        car_envmap_hard(palette, m->paint, m->paint_hard);
        car_envmap_hard(palette, m->glass, m->glass_hard);
        m->env = (EnvShine){ m->paint, m->glass, car_envmap_image(), m->paint_hard, m->glass_hard };
    } else {
        car_shine_tables(palette, m->tables);
    }
    m->valid = 1;
    return m;
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
    current = NULL, current_env = NULL;
    if (!enabled || palette == NULL || camera_matrix == NULL) {
        object_sink_set_shine(NULL, NULL);
        object_sink_set_envmap(NULL);
        return;
    }
    Made *m = made_for(palette);
    world_up(camera_matrix, up_in_camera);
    if (strength == ENVMAP) {
        object_sink_set_shine(NULL, NULL);
        current_env = &m->env;
        object_sink_set_envmap(current_env);
        return;
    }
    current = m->tables;
    object_sink_set_shine_strength(strength);
    object_sink_set_shine(current, up_in_camera);
    object_sink_set_envmap(NULL);
}

void body_shine_end(void)
{
    current = NULL, current_env = NULL;
    object_sink_set_shine(NULL, NULL);
    object_sink_set_envmap(NULL);
}

ShineTables body_shine_tables(void) { return current; }

const EnvShine *body_shine_env(void) { return current_env; }

const float *body_shine_up(void) { return up_in_camera; }

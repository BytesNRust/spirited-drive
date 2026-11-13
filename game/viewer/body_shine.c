/* port (body shine): see body_shine.h. */
#include "viewer/body_shine.h"

#include "game/race/car_envmap.h"
#include "game/race/car_shine.h"
#include "game/sys/fixed_math.h"
#include "viewer/object_sink.h"

#include <math.h>
#include <string.h>

enum { CACHED = 2 };

static int enabled;

typedef struct {
    uint8_t palette[768];
    int valid;
    uint8_t paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256];
    uint8_t paint_hard[CAR_SHINE_LEVELS][256], glass_hard[CAR_SHINE_LEVELS][256];
    EnvShine env;
} Made;

static Made made[CACHED];
static int next_made;
static const EnvShine *current_env;
static float up_in_camera[3] = { 0, 1, 0 };

void body_shine_enable(int level)
{
    enabled = level > 0;
    if (enabled) car_envmap_image();   /* the image is made at start-up, from port-made values */
}

int body_shine_enabled(void) { return enabled; }

/* the envmap for the palette in use, made or found: a palette met again keeps the tables it was made with */
static Made *made_for(const uint8_t palette[768])
{
    for (int i = 0; i < CACHED; i++)
        if (made[i].valid && memcmp(made[i].palette, palette, 768) == 0) return &made[i];
    Made *m = &made[next_made];
    next_made = (next_made + 1) % CACHED;
    memcpy(m->palette, palette, 768);
    car_envmap_tables(palette, m->paint, m->glass);
    car_envmap_hard(palette, m->paint, m->paint_hard);
    car_envmap_hard(palette, m->glass, m->glass_hard);
    m->env = (EnvShine){ m->paint, m->glass, car_envmap_image(), m->paint_hard, m->glass_hard };
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
    current_env = NULL;
    if (!enabled || palette == NULL || camera_matrix == NULL) {
        object_sink_set_shine_up(NULL);
        object_sink_set_envmap(NULL);
        return;
    }
    Made *m = made_for(palette);
    world_up(camera_matrix, up_in_camera);
    object_sink_set_shine_up(up_in_camera);   /* the envmap's sphere map is looked up against the same up */
    current_env = &m->env;
    object_sink_set_envmap(current_env);
}

void body_shine_end(void)
{
    current_env = NULL;
    object_sink_set_shine_up(NULL);
    object_sink_set_envmap(NULL);
}

const EnvShine *body_shine_env(void) { return current_env; }

const float *body_shine_up(void) { return up_in_camera; }

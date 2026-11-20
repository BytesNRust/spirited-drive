/* port (body shine): see body_shine.h. */
#include "viewer/body_shine.h"

#include "game/race/car_envmap.h"
#include "game/race/car_shine.h"
#include "game/sys/palette.h"
#include "raster/shine_pass.h"
#include "viewer/object_sink.h"

#include <string.h>

enum { CACHED = 2 };

static int enabled;

typedef struct {
    uint8_t palette[768];
    int valid;
    ShineTables tables;
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
    shine_tables_make(palette, palette_usable(), palette_emissive(), &m->tables);   /* the screen palette's usable and glowing entries */
    m->env = shine_tables_env(&m->tables, car_envmap_image());
    m->valid = 1;
    return m;
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
    shine_world_up(camera_matrix, up_in_camera);
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

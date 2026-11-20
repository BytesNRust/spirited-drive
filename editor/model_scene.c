#include "model_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raster/car_scene.h"
#include "raster/quad.h"

struct ModelScene {
    CarScene scene;
    uint8_t *file;   /* the CFM's bytes: the scene points into them */
    uint8_t *pixels;
    size_t capacity;
};

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

ModelScene *model_scene_open(const char *path, char *why, size_t why_cap)
{
    size_t size = 0;
    ModelScene *m = calloc(1, sizeof *m);
    if (m == NULL) return NULL;
    m->file = read_file(path, &size);
    if (m->file == NULL) {
        snprintf(why, why_cap, "The car file could not be read: %s", path);
        free(m);
        return NULL;
    }
    if (!car_scene_open(&m->scene, m->file, size)) {
        snprintf(why, why_cap, "%s has no high model or bank this view can draw.", path);
        free(m->file);
        free(m);
        return NULL;
    }
    return m;
}

void model_scene_close(ModelScene *m)
{
    if (m == NULL) return;
    car_scene_close(&m->scene);
    free(m->file);
    free(m->pixels);
    free(m);
}

int32_t model_scene_radius(const ModelScene *m)
{
    return m->scene.radius;
}

int model_scene_max_rows(void)
{
    return QUAD_MAX_ROWS;
}

int model_scene_render(ModelScene *m, const Orbit *o, int w, int h, uint8_t *rgba)
{
    if (w < 1 || h < 1 || h > QUAD_MAX_ROWS) return 0;
    const size_t n = (size_t)w * (size_t)h;
    if (n > m->capacity) {
        uint8_t *bigger = realloc(m->pixels, n);
        if (bigger == NULL) return 0;
        m->pixels = bigger;
        m->capacity = n;
    }
    memset(m->pixels, CAR_SCENE_CLEAR, n);
    car_scene_render(&m->scene, o, m->pixels, w, h);
    car_scene_rgba(&m->scene, m->pixels, n, rgba);
    return 1;
}

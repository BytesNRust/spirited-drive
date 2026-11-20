#include "model_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/police_flash.h"
#include "raster/car_scene.h"
#include "raster/quad.h"
#include "car_preview.h"

enum { NO_LAMP = 0, LAMP_COLOURS = 0xFE };   /* a layer's values: 0 no lamp, 1..LAMP_COLOURS a lit lamp's colour, CAR_SCENE_CLEAR a transparent texel */

/* The lamp texels of the high bank's textures (car_night_texture's own rule): a texture of the same size as the car's, the value a colour of `lamp_rgb` */
typedef struct {
    FshEntry *entries;          /* one a slot of the model's textures */
    const FshEntry **slots;     /* for car_scene_render_layer: the slot's entry, NULL for a slot with no lamp texel */
    uint8_t **texels;
    uint32_t count;
    uint8_t rgb[LAMP_COLOURS + 1][3];
    int colours;
    int lamps;                  /* the switches the layer was made for (lamp_key), -1 none yet */
} LampLayer;

struct ModelScene {
    CarScene scene;
    uint8_t *file;   /* the CFM's bytes: the scene points into them */
    uint8_t *pixels, *layer;
    size_t capacity;
    CarPreview preview;
    int has_preview;
    LampLayer lamp;
    int lamp_pixels;
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

/* the scene on `file` (kept, freed with the scene); `file` is freed here when it cannot be drawn */
static ModelScene *open_bytes(uint8_t *file, size_t size, const char *name, char *why, size_t why_cap)
{
    ModelScene *m = calloc(1, sizeof *m);
    if (m == NULL) {
        free(file);
        return NULL;
    }
    m->file = file;
    m->lamp.lamps = -1;
    if (!car_scene_open(&m->scene, m->file, size)) {
        snprintf(why, why_cap, "%s has no high model or bank this view can draw.", name);
        free(m->file);
        free(m);
        return NULL;
    }
    return m;
}

ModelScene *model_scene_open(const char *path, char *why, size_t why_cap)
{
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    if (file == NULL) {
        snprintf(why, why_cap, "The car file could not be read: %s", path);
        return NULL;
    }
    return open_bytes(file, size, path, why, why_cap);
}

ModelScene *model_scene_open_painted(const PreviewIn *in, char *why, size_t why_cap)
{
    uint8_t *file = NULL;
    size_t size = 0;
    if (!car_preview_painted_cfm(in, &file, &size, NULL, NULL, why, why_cap)) return NULL;
    ModelScene *m = open_bytes(file, size, in->cfm, why, why_cap);
    if (m != NULL) m->has_preview = car_preview_make(in, &m->preview, why, why_cap);   /* without it the car is drawn by day only */
    return m;
}

static void lamp_free(LampLayer *l)
{
    for (uint32_t i = 0; i < l->count; i++) free(l->texels[i]);
    free(l->texels), free(l->entries), free(l->slots);
    l->texels = NULL, l->entries = NULL, l->slots = NULL, l->count = 0, l->colours = 0;
}

void model_scene_close(ModelScene *m)
{
    if (m == NULL) return;
    lamp_free(&m->lamp);
    if (m->has_preview) car_preview_free(&m->preview);
    car_scene_close(&m->scene);
    free(m->file);
    free(m->pixels);
    free(m->layer);
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

const CarPreview *model_scene_preview(const ModelScene *m)
{
    return m->has_preview ? &m->preview : NULL;
}

const uint8_t *model_scene_texels(const ModelScene *m, const char *name, int *w, int *h)
{
    uint32_t index;
    FshEntry e;
    if (strlen(name) != 4 || fsh_find(&m->scene.bank, name, &index) != FSH_OK || fsh_entry(&m->scene.bank, index, &e) != FSH_OK) return NULL;
    if ((size_t)e.width * e.height > e.payload_size) return NULL;
    *w = e.width, *h = e.height;
    return e.payload;
}

/* the Preview's high-bank texture of `e` (its name and size), NULL for none */
static const PreviewTex *preview_of(const ModelScene *m, const FshEntry *e)
{
    for (int i = 0; i < m->preview.count; i++) {
        const PreviewTex *t = &m->preview.tex[i];
        if (t->info.level == 0 && memcmp(t->info.name, e->name, 4) == 0 && t->info.width == e->width && t->info.height == e->height) return t;
    }
    return NULL;
}

/* the value of a lit lamp of colour `rgb` in the layer: a colour seen before keeps its value */
static uint8_t lamp_value(LampLayer *l, const uint8_t *rgb)
{
    for (int c = 0; c < l->colours; c++)
        if (memcmp(l->rgb[1 + c], rgb, 3) == 0) return (uint8_t)(1 + c);
    if (l->colours >= LAMP_COLOURS) return NO_LAMP;
    memcpy(l->rgb[1 + l->colours], rgb, 3);
    return (uint8_t)(1 + l->colours++);
}

/* One texture's lamp layer: car_night_texture with a black palette and no tint, light or flash leaves every texel black but the lit lamps, in their own colours */
static uint8_t *lamp_texels(LampLayer *l, const PreviewTex *t, const CarNightState *lamps_only, int *lit)
{
    static const uint8_t BLACK[768];
    const size_t n = (size_t)t->info.width * (size_t)t->info.height;
    uint8_t *rgba = malloc(4 * n), *out = malloc(n);
    *lit = 0;
    if (rgba == NULL || out == NULL || !car_night_texture(t->index, t->mask, t->info.width, t->info.height, BLACK, lamps_only, rgba)) {
        free(rgba), free(out);
        return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        const uint8_t *px = rgba + 4 * i;
        if (t->index[i] == CAR_SCENE_CLEAR) out[i] = CAR_SCENE_CLEAR;
        else if (px[0] == 0 && px[1] == 0 && px[2] == 0) out[i] = NO_LAMP;
        else out[i] = lamp_value(l, px), *lit += out[i] != NO_LAMP;
    }
    free(rgba);
    return out;
}

/* the layer's key: every switch the lamps' pass reads (lamps_only below), not the race's lamp state, so a rule of car_night_texture's own (the
 * brake lit by day with the state still LAMP_DAY) cannot leave the layer stale */
static int lamp_key(const CarNightState *s)
{
    return (s->night != 0) | (s->lights != 0) << 1 | (s->brake != 0) << 2 | (s->reverse != 0) << 3;
}

/* the layer for the lamps of `night`, made again only when they change */
static void lamp_layer(ModelScene *m, const CarNightState *night)
{
    LampLayer *l = &m->lamp;
    const int lamps = lamp_key(night);
    if (l->lamps == lamps) return;
    lamp_free(l);
    l->lamps = lamps;
    const uint32_t n = m->scene.obj.texture_count;
    if (!m->has_preview || n == 0) return;
    l->entries = calloc(n, sizeof *l->entries), l->slots = calloc(n, sizeof *l->slots), l->texels = calloc(n, sizeof *l->texels);
    if (l->entries == NULL || l->slots == NULL || l->texels == NULL) {
        lamp_free(l);
        return;
    }
    l->count = n;
    CarNightState lamps_only = { 0 };
    lamps_only.night = night->night, lamps_only.lights = night->lights, lamps_only.brake = night->brake, lamps_only.reverse = night->reverse;
    for (uint32_t s = 0; s < n; s++) {
        const FshEntry *e = m->scene.obj.textures[s].texture;
        const PreviewTex *t = e != NULL ? preview_of(m, e) : NULL;
        if (t == NULL || t->index == NULL || t->mask == NULL) continue;
        int lit = 0;
        l->texels[s] = lamp_texels(l, t, &lamps_only, &lit);
        if (l->texels[s] == NULL || lit == 0) continue;
        l->entries[s] = *e;
        l->entries[s].payload = l->texels[s];
        l->entries[s].payload_size = (size_t)e->width * e->height;
        l->slots[s] = &l->entries[s];
    }
}

static int has_lamps(const LampLayer *l)
{
    for (uint32_t s = 0; s < l->count; s++)
        if (l->slots[s] != NULL) return 1;
    return 0;
}

static int grow(ModelScene *m, size_t n)
{
    if (n <= m->capacity) return 1;
    uint8_t *pixels = realloc(m->pixels, n);
    if (pixels == NULL) return 0;
    m->pixels = pixels;
    uint8_t *layer = realloc(m->layer, n);
    if (layer == NULL) return 0;
    m->layer = layer;
    m->capacity = n;
    return 1;
}

/* the picture's colours: the palette as the Preview has it (the race's 6 bits) in the state's light, the lamps over it, the police lights' flash */
static void colour(ModelScene *m, const CarNightState *s, int lamps, size_t n, uint8_t *rgba)
{
    const uint8_t *base = m->has_preview ? m->preview.palette[0] : m->scene.palette;
    uint8_t pal[768], flash[256];
    if (s->tinted || s->lit) car_night_palette(base, s, pal);
    else memcpy(pal, base, sizeof pal);   /* no light on it: the palette as the Preview window shows it by day */
    if (s->police) police_lights_table(NULL, base, s->police_tick, flash);
    m->lamp_pixels = 0;
    for (size_t i = 0; i < n; i++) {
        const uint8_t e = m->pixels[i], k = lamps ? m->layer[i] : NO_LAMP;
        uint8_t *px = rgba + 4 * i;
        if (e == CAR_SCENE_CLEAR) {
            px[0] = px[1] = px[2] = px[3] = 0;
            continue;
        }
        px[3] = 255;
        if (k != NO_LAMP && k != CAR_SCENE_CLEAR && k <= m->lamp.colours) {
            memcpy(px, m->lamp.rgb[k], 3);
            m->lamp_pixels++;
        } else if (s->police && e >= POLICE_LIGHT_FIRST && e < POLICE_LIGHT_FIRST + POLICE_LIGHT_COUNT) {
            memcpy(px, base + 3 * flash[e], 3);   /* the flash is not tinted: the world's light does not reach it */
        } else {
            memcpy(px, pal + 3 * e, 3);
        }
    }
}

int model_scene_render(ModelScene *m, const Orbit *o, int w, int h, const CarNightState *night, uint8_t *rgba)
{
    if (w < 1 || h < 1 || h > QUAD_MAX_ROWS) return 0;
    const size_t n = (size_t)w * (size_t)h;
    if (!grow(m, n)) return 0;
    const CarNightState day = { 0 };
    const CarNightState *s = night != NULL ? night : &day;
    memset(m->pixels, CAR_SCENE_CLEAR, n);
    car_scene_render(&m->scene, o, m->pixels, w, h);
    lamp_layer(m, s);
    const int lamps = has_lamps(&m->lamp);
    if (lamps) {
        memset(m->layer, CAR_SCENE_CLEAR, n);
        car_scene_render_layer(&m->scene, o, m->layer, w, h, m->lamp.slots, NO_LAMP);
    }
    colour(m, s, lamps, n, rgba);
    return 1;
}

int model_scene_lamp_pixels(const ModelScene *m)
{
    return m->lamp_pixels;
}

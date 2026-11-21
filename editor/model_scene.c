#include "model_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/car_brake_lamp.h"
#include "formats/police_flash.h"
#include "raster/car_envmap.h"
#include "raster/car_scene.h"
#include "raster/shine_class.h"
#include "raster/shine_pass.h"
#include "raster/quad.h"
#include "car_preview.h"

enum { NO_LAMP = 0, LAMP_COLOURS = 0xFE };   /* a layer's values: 0 no lamp, 1..LAMP_COLOURS a lit lamp's colour, CAR_SCENE_CLEAR a transparent texel */

/* The lamp texels of the high bank's textures (car_night_texture's own rule): a texture of the same size as the car's, the value a colour of `lamp_rgb` */
typedef struct {
    FshEntry *entries;          /* one a slot of the model's textures */
    const FshEntry **slots;     /* for car_scene_render_layer: the slot's entry, NULL for a slot with no lamp texel */
    uint8_t **texels;
    uint32_t count;
    int lit;                    /* texels of a lit lamp in some slot */
    uint8_t rgb[LAMP_COLOURS + 1][3];
    int colours;
    int lamps;                  /* the switches the layer was made for (lamp_key), -1 none yet */
} LampLayer;

/* the body shine: the class maps (shine_class.h, the process's: kept under a slot of the scene's own, dropped when it closes), the envmap and the
 * tables of the palette last shown */
typedef struct {
    int slot, textures, on;
    int painted;               /* a painted scene: its masks can be drafted (when the shine is first on: slot is 0 until then) */
    uint8_t image[CAR_ENVMAP_SIZE * CAR_ENVMAP_SIZE];
    uint8_t palette[768];
    int made;                  /* the tables are `palette`'s */
    ShineTables tables;
    EnvShine env;
} Shine;

struct ModelScene {
    CarScene scene;
    Shine shine;
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
    if (m != NULL) m->shine.painted = 1;
    return m;
}

/* the slots the scenes' class maps are kept under: far from the race's car slots, one a scene, so a closed scene's are dropped and never read again */
static int next_slot = 0x10000;

typedef struct {
    ModelScene *m;
} Register;

/* a body texture's draft (car_preview_draft): its classes kept for the texels the scene draws, the painted file's texture of that name (level 0) */
static void register_texture(void *user, int level, const char name[4], int width, int height, const uint8_t *pixels, const uint8_t *keep, const uint8_t rgb[768])
{
    (void)pixels, (void)rgb;
    ModelScene *m = ((Register *)user)->m;
    uint32_t index;
    FshEntry e;
    if (level != 0 || fsh_find(&m->scene.bank, name, &index) != FSH_OK || fsh_entry(&m->scene.bank, index, &e) != FSH_OK) return;
    if (e.width != width || e.height != height || (size_t)width * (size_t)height > e.payload_size) return;
    shine_class_add(m->shine.slot, e.payload, width, height, keep);
    m->shine.textures++;
}

static void register_shine(ModelScene *m, const PreviewIn *in)
{
    char why[200];
    Register r = { m };
    m->shine.slot = next_slot++;
    if (car_preview_draft(in, register_texture, &r, why, sizeof why) < 0) m->shine.textures = 0;
}

static void lamp_free(LampLayer *l)
{
    for (uint32_t i = 0; i < l->count; i++) free(l->texels[i]);
    free(l->texels), free(l->entries), free(l->slots);
    l->texels = NULL, l->entries = NULL, l->slots = NULL, l->count = 0, l->colours = 0, l->lit = 0;
}

void model_scene_close(ModelScene *m)
{
    if (m == NULL) return;
    lamp_free(&m->lamp);
    if (m->shine.slot != 0) shine_class_drop_slot(m->shine.slot);   /* the texels' addresses go with the file: none may be found again */
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
    uint8_t pal[768];
    car_night_palette(BLACK, lamps_only, pal);   /* black but the brake lamp's entry, which the palette paints, not the layer */
    const uint8_t *entry = pal + 3 * CAR_BRAKE_LAMP_ENTRY;
    for (size_t i = 0; i < n; i++) {
        const uint8_t *px = rgba + 4 * i;
        if (t->index[i] == CAR_SCENE_CLEAR) out[i] = CAR_SCENE_CLEAR;
        else if ((px[0] == 0 && px[1] == 0 && px[2] == 0) || (t->index[i] == CAR_BRAKE_LAMP_ENTRY && memcmp(px, entry, 3) == 0)) out[i] = NO_LAMP;
        else out[i] = lamp_value(l, px), *lit += out[i] != NO_LAMP;
    }
    free(rgba);
    return out;
}

/* A texture's layer with no lamp: NO_LAMP, transparent where its texel is (the Preview's texels, else the bank's own, else nowhere); NULL only out of memory */
static uint8_t *dark_texels(const PreviewTex *t, const FshEntry *e)
{
    const size_t n = (size_t)e->width * (size_t)e->height;
    const uint8_t *index = t != NULL && t->index != NULL ? t->index : e->payload_size >= n ? e->payload : NULL;
    uint8_t *out = n > 0 ? malloc(n) : NULL;
    for (size_t i = 0; out != NULL && i < n; i++) out[i] = index != NULL && index[i] == CAR_SCENE_CLEAR ? CAR_SCENE_CLEAR : NO_LAMP;
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
    lamps_only.lamp_row = night->lamp_row;
    for (uint32_t s = 0; s < n; s++) {   /* every texture its layer, a dark one where no lamp is: a slot left out draws its own indices, read as lamps (W1) */
        const FshEntry *e = m->scene.obj.textures[s].texture;
        if (e == NULL) continue;
        const PreviewTex *t = preview_of(m, e);
        int lit = 0;
        l->texels[s] = t != NULL && t->index != NULL && t->mask != NULL ? lamp_texels(l, t, &lamps_only, &lit) : NULL;
        if (lit == 0) free(l->texels[s]), l->texels[s] = dark_texels(t, e);
        if (l->texels[s] == NULL) continue;
        l->lit += lit;
        l->entries[s] = *e;
        l->entries[s].payload = l->texels[s];
        l->entries[s].payload_size = (size_t)e->width * e->height;
        l->slots[s] = &l->entries[s];
    }
}

static int has_lamps(const LampLayer *l)
{
    return l->lit > 0;
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
/* the palette the picture is shown in: the Preview's (the race's 6 bits) by day, in the state's light at night */
static void shown_palette(const ModelScene *m, const CarNightState *s, uint8_t pal[768])
{
    const uint8_t *base = m->has_preview ? m->preview.palette[0] : m->scene.palette;
    if (s->tinted || s->lit) {
        car_night_palette(base, s, pal);   /* the brake lamp's entry with it (W2) */
    } else {
        memcpy(pal, base, 768);   /* no light on it: the palette as the Preview window shows it by day */
        if (m->has_preview) car_preview_lamp_entry(&m->preview, s->brake, pal);   /* the brake lamp the race paints, lit with the brake by day too (W2) */
    }
}

static void colour(ModelScene *m, const CarNightState *s, const uint8_t pal[768], int lamps, size_t n, uint8_t *rgba)
{
    const uint8_t *base = m->has_preview ? m->preview.palette[0] : m->scene.palette;
    uint8_t flash[256];
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

int model_scene_shine(ModelScene *m, int on, const PreviewIn *in, const char *envmap)
{
    Shine *sh = &m->shine;
    if (on && sh->painted && sh->slot == 0 && in != NULL) register_shine(m, in);   /* the draft once, the first time it is wanted */
    sh->on = on && sh->textures > 0;
    if (!sh->on) return 0;
    if (envmap == NULL || !car_envmap_file(envmap, sh->image)) car_envmap_make(sh->image);
    return 1;
}

int model_scene_wheels(ModelScene *m, int frame)
{
    return car_scene_wheels(&m->scene, frame);
}

int model_scene_shine_textures(const ModelScene *m)
{
    return m->shine.textures;
}

/* the pass's tables for the palette the picture is shown in, made again when it changes (as the race's body_shine keeps them by palette) */
static const EnvShine *shine_for(ModelScene *m, const uint8_t pal[768])
{
    Shine *sh = &m->shine;
    if (!sh->on) return NULL;
    if (!sh->made || memcmp(sh->palette, pal, sizeof sh->palette) != 0) {
        uint8_t usable[256], emissive[256] = { 0 };
        memset(usable, 1, sizeof usable);
        usable[CAR_SCENE_CLEAR] = 0;   /* the transparent entry */
        for (int i = POLICE_LIGHT_FIRST; i < POLICE_LIGHT_FIRST + POLICE_LIGHT_COUNT; i++) emissive[i] = 1;   /* the police lights' entries flash, they do not shine */
        memcpy(sh->palette, pal, sizeof sh->palette);
        shine_tables_make(pal, usable, emissive, &sh->tables);
        sh->env = shine_tables_env(&sh->tables, sh->image);
        sh->made = 1;
    }
    return &sh->env;
}

int model_scene_render(ModelScene *m, const Orbit *o, int w, int h, const CarNightState *night, uint8_t *rgba)
{
    if (w < 1 || h < 1 || h > QUAD_MAX_ROWS) return 0;
    const size_t n = (size_t)w * (size_t)h;
    if (!grow(m, n)) return 0;
    CarNightState state = { 0 };
    if (night != NULL) state = *night;
    if (m->has_preview) state.lamp_row = m->preview.lamp_row;   /* the car's own brake lamp, whatever the caller's state says */
    const CarNightState *s = &state;
    uint8_t pal[768];
    shown_palette(m, s, pal);
    memset(m->pixels, CAR_SCENE_CLEAR, n);
    car_scene_shine(&m->scene, shine_for(m, pal));
    car_scene_render(&m->scene, o, m->pixels, w, h);
    car_scene_shine(&m->scene, NULL);   /* the lamp layer's values are not colours: no shine on them */
    lamp_layer(m, s);
    const int lamps = has_lamps(&m->lamp);
    if (lamps) {
        memset(m->layer, CAR_SCENE_CLEAR, n);
        car_scene_render_layer(&m->scene, o, m->layer, w, h, m->lamp.slots, NO_LAMP);
    }
    colour(m, s, pal, lamps, n, rgba);
    return 1;
}

int model_scene_lamp_pixels(const ModelScene *m)
{
    return m->lamp_pixels;
}

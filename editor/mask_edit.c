#include "mask_edit.h"

#include "formats/car_mask_codec.h"
#include "formats/png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const MaskClass CLASSES[ME_CLASSES] = {
    { CAR_MASK_PAINT, "Body / paint (repainted)", "paint", { 150, 80, 220 } },
    { CAR_MASK_SHINE, "Glass", "glass", { 60, 120, 255 } },
    { CAR_MASK_BODY, "Body shine, kept", "body shine", { 40, 200, 60 } },
    { CAR_MASK_BLACK, "Fixed black", "fixed black", { 220, 40, 220 } },
    { CAR_MASK_BRAKE, "Brake light", "brake", { 255, 40, 40 } },
    { CAR_MASK_POSITION, "Side light", "side light", { 255, 150, 30 } },
    { CAR_MASK_HEADLIGHT, "Headlight", "headlight", { 255, 235, 40 } },
    { CAR_MASK_REVERSE, "Reverse light", "reverse", { 40, 230, 230 } },
    { CAR_MASK_MATTE, "Matte (plate, badges, plastics)", "matte", { 255, 255, 255 } },
};

const MaskClass *mask_edit_class(int index)
{
    return index >= 0 && index < ME_CLASSES ? &CLASSES[index] : NULL;
}

int mask_edit_class_index(uint8_t value)
{
    for (int i = 0; i < ME_CLASSES; i++)
        if (CLASSES[i].value == value) return i;
    return ME_CLASSES - 1;
}

/* the texel values this editor writes: each one is a class's own value, so a texel of a grey the file has between two (241, say) is read as its class and written as the class's value */
static uint8_t canon(uint8_t v)
{
    return CLASSES[mask_edit_class_index(v)].value;
}

static int read_file(const char *path, uint8_t **out, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    uint8_t *buf = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n == cap) {
            cap = cap ? cap * 2 : 65536;
            uint8_t *bigger = realloc(buf, cap);
            if (bigger == NULL) { free(buf); fclose(f); return 0; }
            buf = bigger;
        }
        const size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    *out = buf;
    *len = n;
    return 1;
}

static void why_set(char *why, size_t cap, const char *fmt, const char *a)
{
    if (why != NULL && cap > 0) snprintf(why, cap, fmt, a);
}

int mask_edit_open(MaskEdit *e, const char *path, int tex_width, int tex_height, const uint8_t *tex_rgb, char *why, size_t why_cap)
{
    memset(e, 0, sizeof *e);
    uint8_t *file = NULL;
    size_t flen = 0;
    if (strlen(path) >= ME_PATH) { why_set(why, why_cap, "The mask's path is too long: %s", path); return 0; }
    if (!read_file(path, &file, &flen)) { why_set(why, why_cap, "Cannot read the mask %s", path); return 0; }
    uint32_t w = 0, h = 0;
    if (png_size(file, flen, &w, &h) != PNG_OK || w == 0 || h == 0 || (size_t)w * h > PNG_MAX_PIXELS) {
        free(file);
        why_set(why, why_cap, "%s is not a mask picture the editor can read", path);
        return 0;
    }
    e->now = malloc((size_t)w * h);
    e->saved = malloc((size_t)w * h);
    e->undo = calloc(ME_UNDO_MAX, sizeof *e->undo);
    e->redo = calloc(ME_UNDO_MAX, sizeof *e->redo);
    if (e->now == NULL || e->saved == NULL || e->undo == NULL || e->redo == NULL || !car_mask_parse(file, flen, (int)w, (int)h, e->now)) {
        free(file);
        mask_edit_free(e);
        why_set(why, why_cap, "%s is not a mask picture the editor can read", path);
        return 0;
    }
    free(file);
    for (size_t i = 0; i < (size_t)w * h; i++) e->now[i] = canon(e->now[i]);
    memcpy(e->saved, e->now, (size_t)w * h);
    snprintf(e->path, sizeof e->path, "%s", path);
    e->width = (int)w;
    e->height = (int)h;
    if (tex_rgb != NULL && tex_width > 0 && tex_height > 0) {
        const size_t n = (size_t)tex_width * tex_height * 3;
        e->tex = malloc(n);
        if (e->tex != NULL) {
            memcpy(e->tex, tex_rgb, n);
            e->tex_width = tex_width;
            e->tex_height = tex_height;
        }
    }
    return 1;
}

static void step_free(MaskStep *s)
{
    free(s->idx);
    free(s->old);
    memset(s, 0, sizeof *s);
}

void mask_edit_free(MaskEdit *e)
{
    if (e->undo != NULL) for (int i = 0; i < ME_UNDO_MAX; i++) step_free(&e->undo[i]);
    if (e->redo != NULL) for (int i = 0; i < ME_UNDO_MAX; i++) step_free(&e->redo[i]);
    free(e->undo);
    free(e->redo);
    free(e->now);
    free(e->saved);
    free(e->tex);
    memset(e, 0, sizeof *e);
}

int mask_edit_size_note(const MaskEdit *e, char *out, size_t cap)
{
    if (e->tex == NULL || (e->tex_width == e->width && e->tex_height == e->height)) return 0;
    snprintf(out, cap, "This mask is %dx%d and its texture is %dx%d: the game scales the mask to the texture (nearest neighbour), so each mask texel covers more or less than one texture texel.",
             e->width, e->height, e->tex_width, e->tex_height);
    return 1;
}

int mask_edit_texture_at(const MaskEdit *e, int x, int y, uint8_t rgb[3])
{
    if (e->tex == NULL || x < 0 || y < 0 || x >= e->width || y >= e->height) return 0;
    /* the game's scaling: the texture's texel (tx, ty) reads the mask's texel (tx * mw / tw, ty * mh / th); the texture texel at the centre of the mask texel is the nearest */
    long tx = ((long)x * 2 + 1) * e->tex_width / (2L * e->width), ty = ((long)y * 2 + 1) * e->tex_height / (2L * e->height);
    if (tx >= e->tex_width) tx = e->tex_width - 1;
    if (ty >= e->tex_height) ty = e->tex_height - 1;
    memcpy(rgb, e->tex + ((size_t)ty * e->tex_width + (size_t)tx) * 3, 3);
    return 1;
}

int mask_edit_class_at(const MaskEdit *e, int x, int y)
{
    if (x < 0 || y < 0 || x >= e->width || y >= e->height) return -1;
    return mask_edit_class_index(e->now[(size_t)y * e->width + x]);
}

/* a new step on the undo stack (the oldest falls off) and the redo stack emptied; NULL when memory is out */
static MaskStep *step_new(MaskEdit *e, uint8_t cls)
{
    for (int i = 0; i < e->redo_count; i++) step_free(&e->redo[i]);
    e->redo_count = 0;
    if (e->undo_count == ME_UNDO_MAX) {
        step_free(&e->undo[0]);
        memmove(&e->undo[0], &e->undo[1], (ME_UNDO_MAX - 1) * sizeof e->undo[0]);
        memset(&e->undo[ME_UNDO_MAX - 1], 0, sizeof e->undo[0]);
        e->undo_count--;
    }
    MaskStep *s = &e->undo[e->undo_count++];
    memset(s, 0, sizeof *s);
    s->cls = cls;
    e->modified_known = 0, e->version++;
    return s;
}

static int step_add(MaskStep *s, uint32_t idx, uint8_t old)
{
    if (s->count == s->cap) {
        const size_t cap = s->cap ? s->cap * 2 : 64;
        uint32_t *i2 = realloc(s->idx, cap * sizeof *i2);
        if (i2 == NULL) return 0;
        s->idx = i2;
        uint8_t *o2 = realloc(s->old, cap);
        if (o2 == NULL) return 0;
        s->old = o2;
        s->cap = cap;
    }
    s->idx[s->count] = idx;
    s->old[s->count++] = old;
    return 1;
}

/* sets one texel inside the step; 1 when it changed */
static int paint_texel(MaskEdit *e, MaskStep *s, int x, int y, uint8_t cls)
{
    if (x < 0 || y < 0 || x >= e->width || y >= e->height) return 0;
    const size_t i = (size_t)y * e->width + x;
    if (e->now[i] == cls || !step_add(s, (uint32_t)i, e->now[i])) return 0;
    e->now[i] = cls;
    return 1;
}

/* a step that changed nothing is dropped, so that an idle click is no step to undo */
static void step_close(MaskEdit *e)
{
    if (e->undo_count > 0 && e->undo[e->undo_count - 1].count == 0) step_free(&e->undo[--e->undo_count]);
}

void mask_edit_stroke_begin(MaskEdit *e)
{
    e->stroke = 1;   /* 1: no step yet, the first texel makes it; 2: the top step is the stroke's */
}

void mask_edit_stroke_end(MaskEdit *e)
{
    e->stroke = 0;
    step_close(e);
}

static int stroke_open_step(MaskEdit *e, uint8_t cls, int *fresh)
{
    *fresh = 0;
    if (e->stroke == 2 && e->undo_count > 0 && e->undo[e->undo_count - 1].cls == cls) return 1;
    *fresh = 1;
    if (step_new(e, cls) == NULL) return 0;
    if (e->stroke) e->stroke = 2;
    return 1;
}

static int brush(MaskEdit *e, int x, int y, int size, uint8_t cls, MaskStep *s)
{
    if (size < ME_BRUSH_MIN) size = ME_BRUSH_MIN;
    if (size > ME_BRUSH_MAX) size = ME_BRUSH_MAX;
    const int x0 = x - (size - 1) / 2, y0 = y - (size - 1) / 2;
    int n = 0;
    for (int dy = 0; dy < size; dy++)
        for (int dx = 0; dx < size; dx++) n += paint_texel(e, s, x0 + dx, y0 + dy, cls);
    return n;
}

int mask_edit_pencil(MaskEdit *e, int x, int y, int size, uint8_t cls)
{
    cls = canon(cls);
    int fresh;
    if (!stroke_open_step(e, cls, &fresh)) return 0;
    const int n = brush(e, x, y, size, cls, &e->undo[e->undo_count - 1]);
    if (!e->stroke) step_close(e);
    else if (fresh && n == 0) { step_close(e); if (e->stroke) e->stroke = 1; }
    return n;
}

int mask_edit_line(MaskEdit *e, int x0, int y0, int x1, int y1, int size, uint8_t cls)
{
    cls = canon(cls);
    const int own = !e->stroke;
    if (own) mask_edit_stroke_begin(e);
    int n = 0;
    const int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        n += mask_edit_pencil(e, x0, y0, size, cls);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    if (own) mask_edit_stroke_end(e);
    return n;
}

static int same_colour(const MaskEdit *e, size_t a, size_t b)
{
    uint8_t ca[3], cb[3];
    mask_edit_texture_at(e, (int)(a % e->width), (int)(a / e->width), ca);
    mask_edit_texture_at(e, (int)(b % e->width), (int)(b / e->width), cb);
    return memcmp(ca, cb, 3) == 0;
}

int mask_edit_fill(MaskEdit *e, int x, int y, uint8_t cls, MaskFillMode mode)
{
    cls = canon(cls);
    if (x < 0 || y < 0 || x >= e->width || y >= e->height || (mode == ME_FILL_TEXTURE && e->tex == NULL)) return 0;
    const size_t total = (size_t)e->width * e->height, seed = (size_t)y * e->width + x;
    const uint8_t target = e->now[seed];
    if (mode == ME_FILL_CLASS && target == cls) return 0;
    uint8_t *seen = calloc(total, 1);
    uint32_t *stack = malloc(total * sizeof *stack);
    MaskStep *s = (seen != NULL && stack != NULL) ? step_new(e, cls) : NULL;
    int n = 0;
    size_t top = 0;
    if (s != NULL) { stack[top++] = (uint32_t)seed; seen[seed] = 1; }
    while (top > 0) {
        const size_t i = stack[--top];
        const int px = (int)(i % e->width), py = (int)(i / e->width);
        n += paint_texel(e, s, px, py, cls);
        const int nx[4] = { px - 1, px + 1, px, px }, ny[4] = { py, py, py - 1, py + 1 };
        for (int k = 0; k < 4; k++) {
            if (nx[k] < 0 || ny[k] < 0 || nx[k] >= e->width || ny[k] >= e->height) continue;
            const size_t j = (size_t)ny[k] * e->width + nx[k];
            /* a texel not yet visited has not been painted, so the class it has is the one it started with */
            if (seen[j] || (mode == ME_FILL_CLASS ? e->now[j] != target : !same_colour(e, j, seed))) continue;
            seen[j] = 1;
            stack[top++] = (uint32_t)j;
        }
    }
    free(seen);
    free(stack);
    if (s != NULL) step_close(e);
    return n;
}

int mask_edit_undo(MaskEdit *e)
{
    if (e->undo_count == 0) return 0;
    e->stroke = 0;
    MaskStep s = e->undo[--e->undo_count];
    memset(&e->undo[e->undo_count], 0, sizeof e->undo[0]);
    for (size_t i = s.count; i-- > 0;) e->now[s.idx[i]] = s.old[i];
    if (e->redo_count < ME_UNDO_MAX) e->redo[e->redo_count++] = s;
    else step_free(&s);
    e->modified_known = 0, e->version++;
    return 1;
}

int mask_edit_redo(MaskEdit *e)
{
    if (e->redo_count == 0) return 0;
    MaskStep s = e->redo[--e->redo_count];
    memset(&e->redo[e->redo_count], 0, sizeof e->redo[0]);
    for (size_t i = 0; i < s.count; i++) e->now[s.idx[i]] = s.cls;
    e->undo[e->undo_count++] = s;   /* undo_count < ME_UNDO_MAX: a redo step came from an undo */
    e->modified_known = 0, e->version++;
    return 1;
}

int mask_edit_can_undo(const MaskEdit *e) { return e->undo_count > 0; }
int mask_edit_can_redo(const MaskEdit *e) { return e->redo_count > 0; }

int mask_edit_modified(MaskEdit *e)
{
    if (!e->modified_known) {
        e->modified = e->now != NULL && memcmp(e->now, e->saved, (size_t)e->width * e->height) != 0;
        e->modified_known = 1;
    }
    return e->modified;
}

int mask_edit_render(const MaskEdit *e, uint8_t **out, size_t *len)
{
    uint8_t *png = NULL;
    size_t n = 0;
    if (!car_mask_write_grey(e->now, e->width, e->height, &png, &n)) return 0;
    uint8_t *copy = malloc(n);
    if (copy != NULL) memcpy(copy, png, n);
    png_free(png);
    if (copy == NULL) return 0;
    *out = copy;
    *len = n;
    return 1;
}

int mask_edit_save(MaskEdit *e, char *why, size_t why_cap)
{
    if (!mask_edit_modified(e)) return 1;
    uint8_t *png = NULL;
    size_t n = 0;
    char tmp[ME_PATH + 16];
    if (!mask_edit_render(e, &png, &n)) { why_set(why, why_cap, "Cannot encode the mask %s", e->path); return 0; }
    snprintf(tmp, sizeof tmp, "%s.saving", e->path);
    FILE *f = fopen(tmp, "wb");
    const int ok = f != NULL && fwrite(png, 1, n, f) == n;
    if (f != NULL && fclose(f) != 0) { remove(tmp); free(png); why_set(why, why_cap, "Cannot write the mask %s", e->path); return 0; }
    free(png);
    if (!ok || rename(tmp, e->path) != 0) {
        remove(tmp);
        why_set(why, why_cap, "Cannot write the mask %s", e->path);
        return 0;
    }
    memcpy(e->saved, e->now, (size_t)e->width * e->height);
    e->modified_known = 0, e->version++;
    return 1;
}

void mask_edit_compose(const MaskEdit *e, const uint8_t on[ME_CLASSES], int show, float opacity, uint8_t *rgba)
{
    const int a = (int)(opacity * 256.0f + 0.5f);
    for (int y = 0; y < e->height; y++)
        for (int x = 0; x < e->width; x++) {
            uint8_t px[3] = { 40, 40, 40 };
            mask_edit_texture_at(e, x, y, px);
            const int k = mask_edit_class_index(e->now[(size_t)y * e->width + x]);
            uint8_t *o = rgba + ((size_t)y * e->width + x) * 4;
            for (int c = 0; c < 3; c++) o[c] = show && on[k] ? (uint8_t)((px[c] * (256 - a) + CLASSES[k].rgb[c] * a) >> 8) : px[c];
            o[3] = 255;
        }
}

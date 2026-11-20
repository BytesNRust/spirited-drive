#include "mask_tex.h"

#include "formats/car_bank_walk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *buf = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n == cap) {
            cap = cap ? cap * 2 : 1 << 16;
            uint8_t *bigger = realloc(buf, cap);
            if (bigger == NULL) { free(buf); fclose(f); return NULL; }
            buf = bigger;
        }
        const size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    *len = n;
    return buf;
}

typedef struct {
    MaskTexInfo *list;
    int cap, count;
    /* what is wanted (rgb mode) */
    int want_level;
    const char *want_name;
    uint8_t *rgb;
    int rgb_w, rgb_h;
    int widths[2][CAR_BANK_MAX_TEXTURES], heights[2][CAR_BANK_MAX_TEXTURES];
} Walk;

static void name_of(char out[MASK_TEX_NAME], const char in[4])
{
    int n = 0;
    for (int i = 0; i < 4 && in[i] != 0 && in[i] != ' '; i++) out[n++] = in[i];
    out[n] = 0;
}

static void on_texture(void *user, int level, const char name[4], int width, int height, RepaintTexture *tex)
{
    Walk *w = user;
    (void)tex;
    if (w->list != NULL && w->count < w->cap) {
        MaskTexInfo *t = &w->list[w->count];
        t->level = level;
        name_of(t->name, name);
        snprintf(t->file, sizeof t->file, level == 0 ? "%s_mask.png" : "%s_lo_mask.png", t->name);
        t->width = width;
        t->height = height;
        w->count++;
    }
}

static int on_bank(void *user, const RepaintBank *bank, const CarBankMeta *meta)
{
    Walk *w = user;
    if (w->want_name == NULL || meta->level != w->want_level) return 0;
    for (int k = 0; k < bank->texture_count && w->rgb == NULL; k++) {
        char nm[MASK_TEX_NAME];
        name_of(nm, meta->name[k]);
        if (strcmp(nm, w->want_name) != 0) continue;
        const RepaintTexture *t = &bank->textures[k];
        const int width = t->width;
        if (width <= 0 || t->count == 0 || t->count % (size_t)width != 0) continue;
        const int height = (int)(t->count / (size_t)width);
        const uint8_t *pal = bank->palette[0];
        const int six = bank->type == 0x22;
        w->rgb = malloc(t->count * 3);
        if (w->rgb == NULL) continue;
        for (size_t i = 0; i < t->count; i++)
            for (int c = 0; c < 3; c++) {
                const uint8_t v = pal[t->pixels[i] * 3 + c];
                w->rgb[i * 3 + c] = six ? (uint8_t)((v << 2) | (v >> 4)) : v;
            }
        w->rgb_w = width;
        w->rgb_h = height;
    }
    return 0;
}

static int walk(const char *cfm, Walk *w)
{
    size_t len = 0;
    uint8_t *data = read_file(cfm, &len);
    if (data == NULL) return 0;
    const CarBankHooks hooks = { on_texture, on_bank };
    car_bank_walk(data, len, &hooks, w);
    free(data);
    return 1;
}

int mask_tex_list(const char *cfm, MaskTexInfo *out, int cap)
{
    Walk w;
    memset(&w, 0, sizeof w);
    w.list = out;
    w.cap = cap;
    return walk(cfm, &w) ? w.count : 0;
}

int mask_tex_rgb(const char *cfm, int level, const char *name, int *width, int *height, uint8_t **rgb)
{
    Walk w;
    memset(&w, 0, sizeof w);
    w.want_level = level;
    w.want_name = name;
    if (!walk(cfm, &w) || w.rgb == NULL) { free(w.rgb); return 0; }
    *rgb = w.rgb;
    *width = w.rgb_w;
    *height = w.rgb_h;
    return 1;
}

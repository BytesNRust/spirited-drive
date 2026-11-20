#include "car_preview.h"

#include "formats/car_livery_pass.h"
#include "formats/car_mask_codec.h"
#include "formats/car_paint.h"
#include "formats/png.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PATH = 4096, SCREEN_FIRST = 0xfa, TRANSPARENT_INDEX = 0xff, OWN_INDEX = 0xfe };

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

static int write_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

static const char *leaf(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

/* the editor's part of the pass (formats/car_livery_pass.h): a texture's mask is read from the project's folder, the unsaved edit in place of its file, else from the game's own masks
 * folder when there is one (the race's lookup of an original car); scaled to the texture as the game scales a mask */
typedef struct {
    const PreviewIn *in;
} MaskSource;

static int name_ok(const char name[4])
{
    for (int i = 0; i < 4; i++)
        if (!isalnum((unsigned char)name[i])) return 0;
    return 1;
}

static uint8_t *mask_of_png(const uint8_t *png, size_t len, int width, int height)
{
    uint32_t w = 0, h = 0;
    if (png_size(png, len, &w, &h) != PNG_OK || w == 0 || h == 0 || w > 2048 || h > 2048) return NULL;
    uint8_t *mask = malloc((size_t)width * (size_t)height);
    if (mask != NULL && !car_mask_parse(png, len, width, height, mask)) {
        free(mask);
        return NULL;
    }
    return mask;
}

static uint8_t *mask_in_folder(const char *dir, const char *file, int width, int height)
{
    if (dir == NULL) return NULL;
    char path[PATH];
    snprintf(path, sizeof path, "%s/%s", dir, file);
    size_t len = 0;
    uint8_t *png = read_file(path, &len);
    uint8_t *mask = png != NULL ? mask_of_png(png, len, width, height) : NULL;
    free(png);
    return mask;
}

static uint8_t *load_mask(void *user, const char name[4], int level, int width, int height)
{
    const PreviewIn *in = ((const MaskSource *)user)->in;
    if (!name_ok(name) || width <= 0 || height <= 0) return NULL;
    char file[32];
    snprintf(file, sizeof file, "%.4s%s_mask.png", name, level != 0 ? "_lo" : "");
    if (in->edit != NULL && in->edit->now != NULL && strcmp(leaf(in->edit->path), file) == 0) {
        uint8_t *png = NULL;
        size_t len = 0;
        if (mask_edit_render(in->edit, &png, &len)) {
            uint8_t *mask = mask_of_png(png, len, width, height);
            free(png);
            return mask;
        }
    }
    uint8_t *mask = mask_in_folder(in->masks_dir, file, width, height);
    return mask != NULL ? mask : mask_in_folder(in->fallback_dir, file, width, height);
}

typedef struct {
    CarPreview *out;
    int level_count[2];       /* textures seen by the hook in each bank, in the walk's order */
    int widths[2][CAR_BANK_MAX_TEXTURES], heights[2][CAR_BANK_MAX_TEXTURES];
    uint8_t seen[1 << 15];    /* the distinct body colours: a bitmap of 6 bits a channel */
    const MaskSource *source;
    int ok;
} Collect;

static void on_texture(void *user, int level, const char name[4], int width, int height, RepaintTexture *tex)
{
    (void)name, (void)tex;
    Collect *c = user;
    const int k = c->level_count[level]++;
    if (k < CAR_BANK_MAX_TEXTURES) c->widths[level][k] = width, c->heights[level][k] = height;
}

/* a texel the screen palette's own entries stand for (not a colour of the bank): drawn as a pattern, so that nobody takes the bank's colour for it */
static void special_texel(uint8_t index, int x, int y, uint8_t rgba[4])
{
    const int on = ((x >> 1) + (y >> 1)) & 1;
    static const uint8_t POLICE[3] = { 255, 0, 130 };   /* the police lights' entries (0xfa..0xfd): hatched magenta */
    static const uint8_t OWN[3] = { 120, 200, 120 };    /* 0xfe: the car's own slot colour */
    const uint8_t *c = index == OWN_INDEX ? OWN : POLICE;
    if (index == TRANSPARENT_INDEX) {   /* 0xff: transparent in a polygon of mode 1, black in mode 0: a chequer */
        rgba[0] = rgba[1] = rgba[2] = on ? 70 : 30;
    } else {
        for (int k = 0; k < 3; k++) rgba[k] = on ? c[k] : (uint8_t)(c[k] / 3);
    }
    rgba[3] = 255;
}

/* the race's 6-bit to 8-bit palette (game/sys/palette.c palette_6_to_8) */
static void six_to_eight(const uint8_t *six, uint8_t *rgb)
{
    for (int i = 0; i < 256 * 3; i++) rgb[i] = (uint8_t)((six[i] * 0xff + 0x20) / 0x3f);
}

static int on_bank(void *user, const RepaintBank *bank, const CarBankMeta *meta)
{
    Collect *c = user;
    CarPreview *out = c->out;
    uint8_t rgb[256 * 3];
    const uint8_t *pal = bank->palette[0];
    if (bank->type == 0x24) {   /* an 8-bit palette: the race takes it to 6 bits a channel */
        uint8_t six[768];
        for (int i = 0; i < 768; i++) six[i] = (uint8_t)(pal[i] >> 2);
        six_to_eight(six, rgb);
    } else {
        six_to_eight(pal, rgb);
    }
    for (int k = 0; k < bank->texture_count && k < CAR_BANK_MAX_TEXTURES; k++) {
        const RepaintTexture *t = &bank->textures[k];
        const int w = c->widths[meta->level][k], h = c->heights[meta->level][k];
        if (w <= 0 || h <= 0 || t->count < (size_t)w * h || out->count >= MASK_TEX_LIST) continue;
        PreviewTex *pt = &out->tex[out->count];
        memset(pt, 0, sizeof *pt);
        pt->rgba = malloc((size_t)w * h * 4);
        pt->index = malloc((size_t)w * h);
        if (pt->rgba == NULL || pt->index == NULL) { free(pt->rgba); free(pt->index); pt->rgba = pt->index = NULL; c->ok = 0; continue; }
        memcpy(pt->index, t->pixels, (size_t)w * h);
        pt->mask = load_mask((void *)c->source, meta->name[k], meta->level, w, h);   /* as the race reads it, the unsaved edit in place of its file */
        memcpy(out->palette[meta->level], rgb, 768);
        pt->info.level = meta->level;
        int n = 0;
        for (int i = 0; i < 4 && meta->name[k][i] != 0 && meta->name[k][i] != ' '; i++) pt->info.name[n++] = meta->name[k][i];
        pt->info.name[n] = '\0';
        snprintf(pt->info.file, sizeof pt->info.file, meta->level == 0 ? "%s_mask.png" : "%s_lo_mask.png", pt->info.name);
        pt->info.width = w;
        pt->info.height = h;
        pt->body = t->body != REPAINT_NONE;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const uint8_t index = t->pixels[(size_t)y * w + x];
                uint8_t *px = pt->rgba + ((size_t)y * w + x) * 4;
                if (index >= SCREEN_FIRST) {
                    special_texel(index, x, y, px);
                    out->special++;
                    continue;
                }
                memcpy(px, rgb + index * 3, 3);
                px[3] = 255;
                if (pt->body && meta->level == 0) {   /* the body's distinct colours: one bit for each of the 6-bit colours */
                    const int r6 = (pal[index * 3] >> (bank->type == 0x24 ? 2 : 0)) & 63, g6 = (pal[index * 3 + 1] >> (bank->type == 0x24 ? 2 : 0)) & 63,
                              b6 = (pal[index * 3 + 2] >> (bank->type == 0x24 ? 2 : 0)) & 63;
                    const int bit = (r6 << 12) | (g6 << 6) | b6;
                    if (!(c->seen[bit >> 3] & (1 << (bit & 7)))) {
                        c->seen[bit >> 3] |= (uint8_t)(1 << (bit & 7));
                        out->body_colours++;
                    }
                }
            }
        out->count++;
    }
    return 0;
}

static void say_colours(CarPreview *p)
{
    const int own = PREVIEW_OWN_ENTRIES, shared = PREVIEW_SHARED_ENTRIES;
    if (p->body_colours <= own)
        snprintf(p->note, sizeof p->note, "The body shows %d colours; a car has %d entries of its own in the race palette, so they all fit. (The track's light and the screen palette are not applied here.)", p->body_colours, own);
    else
        snprintf(p->note, sizeof p->note, "The body shows %d colours but a car has only %d entries of its own in the race palette (%d shared when several cars race): at least %d of the body's colours will look like their nearest. (The track's light and the screen palette are not applied here.)",
                 p->body_colours, own, shared, p->body_colours - own);
}

/* the painted file, and (`out` not NULL) the preview of it */
static int paint_and_walk(const PreviewIn *in, uint8_t **out_cfm, size_t *out_len, CarPreview *out, int *blacked, int *painted, char *why, size_t why_cap)
{
    size_t size = 0;
    uint8_t *cfm = read_file(in->cfm, &size);
    if (cfm == NULL) {
        snprintf(why, why_cap, "Cannot read the car file %s", in->cfm);
        return 0;
    }
    char upper[32];
    size_t u = 0;
    for (; in->stem[u] != '\0' && u < sizeof upper - 1; u++) upper[u] = (char)toupper((unsigned char)in->stem[u]);
    upper[u] = '\0';
    const MaskSource source = { in };
    const CarLiveryPass pass = { upper, in->added, 0, load_mask, (void *)&source };
    const CarPaint *black = car_paint_black_of(in->stem);
    const uint8_t added_black[3] = CAR_PAINT_ADDED_BLACK;
    int nb = 0, np = 0, walked = 1;
    if (black != NULL || in->added) nb = car_livery_black(cfm, size, &pass, black != NULL ? black->rgb : added_black);   /* as the race does, before any paint */
    if (in->paint != NULL) np = car_livery_paint(cfm, size, &pass, in->paint);
    if (out != NULL) {
        Collect *c = calloc(1, sizeof *c);
        walked = c != NULL;
        if (walked) {
            c->out = out;
            c->ok = 1;
            c->source = &source;
            const CarBankHooks hooks = { on_texture, on_bank };
            car_bank_walk(cfm, size, &hooks, c);
            walked = c->ok;
        }
        free(c);
    }
    if (blacked != NULL) *blacked = nb;
    if (painted != NULL) *painted = np;
    if (!walked) {
        free(cfm);
        snprintf(why, why_cap, "Out of memory making the preview");
        return 0;
    }
    if (out_cfm != NULL) *out_cfm = cfm, *out_len = size;
    else free(cfm);
    return 1;
}

int car_preview_painted_cfm(const PreviewIn *in, uint8_t **out_cfm, size_t *out_len, int *blacked, int *painted, char *why, size_t why_cap)
{
    return paint_and_walk(in, out_cfm, out_len, NULL, blacked, painted, why, why_cap);
}

int car_preview_make(const PreviewIn *in, CarPreview *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (!paint_and_walk(in, NULL, NULL, out, &out->blacked, &out->painted, why, why_cap)) {
        car_preview_free(out);
        return 0;
    }
    if (out->count == 0) {
        car_preview_free(out);
        snprintf(why, why_cap, "The car file %s has no textures the preview can read", leaf(in->cfm));
        return 0;
    }
    say_colours(out);
    return 1;
}

void car_preview_free(CarPreview *p)
{
    for (int i = 0; i < p->count; i++) free(p->tex[i].rgba), free(p->tex[i].index), free(p->tex[i].mask);
    memset(p, 0, sizeof *p);
}

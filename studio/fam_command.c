#include "studio/fam_command.h"

#include "formats/fam_build.h"
#include "formats/fam_image.h"
#include "formats/fam_spec.h"
#include "formats/png.h"
#include "studio/fsio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FAM_PATH = 1024 };

typedef struct {
    const char *spec, *out;
    int force;
} Args;

/* a picture read from a PNG: its RGBA pixels, owned (png_free when from_png, free otherwise) */
typedef struct {
    FamPicture pic;
    uint8_t *buf;
    int from_png;
} Loaded;

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    if (argc < 3 || strcmp(argv[1], "build") != 0) { exchange_fail(err, "usage: fam build SPEC --out FILE.FAM [--force]"); return 0; }
    a->spec = argv[2];
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) a->out = argv[++i];
        else { exchange_fail(err, "unknown argument '%s'", argv[i]); return 0; }
    }
    if (a->out == NULL) exchange_fail(err, "fam build needs --out FILE.FAM");
    return a->out != NULL;
}

/* the spec's text with a NUL after it (malloc'd) */
static char *read_text(const char *path, ExchangeError *err)
{
    size_t size = 0;
    uint8_t *file = fsio_read_file(path, &size, err);
    if (file == NULL) return NULL;
    char *text = realloc(file, size + 1);
    if (text == NULL) { free(file); exchange_fail(err, "out of memory"); return NULL; }
    text[size] = '\0';
    return text;
}

/* `path` under the spec's folder, unless it is absolute */
static void resolve(const char *spec, const char *path, char out[FAM_PATH])
{
    const char *slash = strrchr(spec, '/');
    if (path[0] == '/' || slash == NULL) { snprintf(out, FAM_PATH, "%s", path); return; }
    snprintf(out, FAM_PATH, "%.*s/%s", (int)(slash - spec), spec, path);
}

/* the indexed palette expanded to RGBA (malloc'd, alpha 255) */
static uint8_t *expand_indexed(const PngImage *img)
{
    const size_t count = (size_t)img->width * img->height;
    uint8_t *rgba = malloc(count * 4);
    if (rgba == NULL) return NULL;
    for (size_t i = 0; i < count; i++) {
        const uint8_t *c = img->palette + 3 * img->pixels[i];
        memcpy(rgba + 4 * i, c, 3);
        rgba[4 * i + 3] = 255;
    }
    return rgba;
}

static int load_picture(const char *path, Loaded *out, ExchangeError *err)
{
    size_t size = 0;
    uint8_t *file = fsio_read_file(path, &size, err);
    if (file == NULL) return 0;
    uint32_t w = 0, h = 0;
    PngResult r = png_size(file, size, &w, &h);
    if (r == PNG_OK && (size_t)w * h > PNG_MAX_PIXELS) r = PNG_ERR_SIZE;
    PngImage img;
    memset(&img, 0, sizeof img);
    if (r == PNG_OK) r = png_read(file, size, &img);
    free(file);
    if (r != PNG_OK) { exchange_fail(err, "%s: %s", path, png_result_str(r)); return 0; }
    out->from_png = !img.indexed;
    out->buf = img.indexed ? expand_indexed(&img) : img.pixels;
    if (img.indexed) png_free(img.pixels);
    if (out->buf == NULL) { exchange_fail(err, "out of memory"); return 0; }
    out->pic = (FamPicture){ img.width, img.height, out->buf };
    return 1;
}

static void loaded_free(Loaded *l)
{
    if (l->from_png) png_free(l->buf);
    else free(l->buf);
    l->buf = NULL;
}

/* the pictures of the spec, in its order: each kind's pictures in their own array */
typedef struct {
    Loaded *item;
    FamPicture *terrain, *scenery;
    unsigned *letters;   /* a terrain bank's letters */
    FamPicture horizon;
    size_t terrain_count, scenery_count, loaded;
} Pictures;

static void pictures_free(Pictures *p)
{
    for (size_t i = 0; i < p->loaded; i++) loaded_free(&p->item[i]);
    free(p->item);
    free(p->terrain);
    free(p->letters);
    free(p->scenery);
}

/* the picture just loaded goes to its kind's array */
static void place(Pictures *p, const FamSpecItem *it)
{
    const FamPicture pic = p->item[p->loaded - 1].pic;
    if (it->kind == FAM_SPEC_TERRAIN) p->letters[p->terrain_count] = it->letters, p->terrain[p->terrain_count++] = pic;
    else if (it->kind == FAM_SPEC_SCENERY) p->scenery[p->scenery_count++] = pic;
    else p->horizon = pic;
}

static int load_all(const char *spec, const FamSpec *s, Pictures *p, ExchangeError *err)
{
    p->item = calloc(s->count ? s->count : 1, sizeof *p->item);
    p->terrain = calloc(s->count ? s->count : 1, sizeof *p->terrain);
    p->scenery = calloc(s->count ? s->count : 1, sizeof *p->scenery);
    p->letters = calloc(s->count ? s->count : 1, sizeof *p->letters);
    if (p->item == NULL || p->terrain == NULL || p->scenery == NULL || p->letters == NULL) { exchange_fail(err, "out of memory"); return 0; }
    for (size_t i = 0; i < s->count; i++) {
        char path[FAM_PATH];
        resolve(spec, s->item[i].path, path);
        if (!load_picture(path, &p->item[p->loaded], err)) return 0;
        p->loaded++;
        place(p, &s->item[i]);
    }
    return 1;
}

static int build(const Args *a, ExchangeError *err)
{
    char *text = read_text(a->spec, err);
    if (text == NULL) return 0;
    FamSpec *spec = malloc(sizeof *spec);
    if (spec == NULL) { free(text); exchange_fail(err, "out of memory"); return 0; }
    int ok = fam_spec_parse(text, spec, err);
    free(text);
    Pictures p;
    memset(&p, 0, sizeof p);
    ok = ok && load_all(a->spec, spec, &p, err);
    uint8_t *bytes = NULL;
    size_t len = 0;
    if (ok) {
        const FamSource src = { p.terrain, p.terrain_count, p.scenery, p.scenery_count, &p.horizon, p.letters };
        ok = fam_build(&src, &bytes, &len, err) && fsio_write_file(a->out, bytes, len, a->force, err);
    }
    if (ok) printf("%s: %zu bytes, %zu terrain textures, %zu scenery textures\n", a->out, len, p.terrain_count, p.scenery_count);
    free(bytes);
    pictures_free(&p);
    free(spec);
    return ok;
}

int fam_command(int argc, char **argv, ExchangeError *err)
{
    Args a;
    if (!parse(argc, argv, &a, err)) return 0;
    return build(&a, err);
}

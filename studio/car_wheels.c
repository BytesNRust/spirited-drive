/* tnfs-studio car wheels (studio/car_wheels.h) */
#include "studio/car_wheels.h"

#include "formats/car_wheels_gen.h"
#include "formats/png.h"
#include "studio/fsio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USAGE "usage: tnfs-studio car wheels IN.png OUT_DIR [--centre X,Y] [--radius R] [--tyre F] [--slow-angle D] [--fast-arc D] [--force]"

enum { TRANSPARENT_INDEX = 255 };

static const char *const FRAME_FILES[WHEEL_FRAMES] = { "still.png", "slow-1.png", "slow-2.png", "fast.png" };

typedef struct {
    const char *in, *out;
    int force;
    WheelFrameOptions o;
} Args;

/* a number that is the whole of `text` */
static int number(const char *text, double *v)
{
    char *end;
    *v = strtod(text, &end);
    return end != text && *end == '\0';
}

static int option_value(const char *flag, const char *text, double *v, ExchangeError *err)
{
    if (number(text, v)) return 1;
    exchange_fail(err, "%s '%s' is not a number", flag, text);
    return 0;
}

static int centre(const char *text, WheelFrameOptions *o, ExchangeError *err)
{
    char *end;
    o->centre_x = strtod(text, &end);
    if (end == text || *end != ',' || (o->centre_y = strtod(end + 1, &end), *end != '\0')) {
        exchange_fail(err, "--centre '%s' is not X,Y (pixels, e.g. 24,24)", text);
        return 0;
    }
    o->has_centre = 1;
    return 1;
}

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    wheel_frame_options_default(&a->o);
    int ok = 1;
    for (int i = 2; ok && i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (strcmp(argv[i], "--centre") == 0 && has_value) ok = centre(argv[++i], &a->o, err);
        else if (strcmp(argv[i], "--radius") == 0 && has_value) ok = option_value("--radius", argv[++i], &a->o.radius, err), a->o.has_radius = 1;
        else if (strcmp(argv[i], "--tyre") == 0 && has_value) ok = option_value("--tyre", argv[++i], &a->o.tyre_share, err);
        else if (strcmp(argv[i], "--slow-angle") == 0 && has_value) ok = option_value("--slow-angle", argv[++i], &a->o.slow_angle_deg, err);
        else if (strcmp(argv[i], "--fast-arc") == 0 && has_value) ok = option_value("--fast-arc", argv[++i], &a->o.fast_arc_deg, err);
        else if (argv[i][0] != '-' && a->in == NULL) a->in = argv[i];
        else if (argv[i][0] != '-' && a->out == NULL) a->out = argv[i];
        else ok = 0, exchange_fail(err, "unexpected argument '%s'; %s", argv[i], USAGE);
    }
    if (ok && (a->in == NULL || a->out == NULL)) ok = 0, exchange_fail(err, USAGE);
    return ok;
}

/* the picture at `path` as RGBA (malloc'd), an indexed one through its palette with index 255 clear */
static uint8_t *read_rgba(const char *path, int *w, int *h, ExchangeError *err)
{
    size_t len = 0;
    uint8_t *data = fsio_read_file(path, &len, err);
    if (data == NULL) return NULL;
    PngImage img;
    memset(&img, 0, sizeof img);
    const PngResult r = png_read(data, len, &img);
    free(data);
    if (r != PNG_OK) { exchange_fail(err, "%s is not a PNG this can read (error %d)", path, (int)r); return NULL; }
    if (img.width > WHEEL_FRAME_MAX_SIDE || img.height > WHEEL_FRAME_MAX_SIDE) {
        exchange_fail(err, "%s is %ux%u: a wheel's picture is at most %d a side", path, img.width, img.height, WHEEL_FRAME_MAX_SIDE);
        png_free(img.pixels);
        return NULL;
    }
    *w = (int)img.width, *h = (int)img.height;
    if (!img.indexed) return img.pixels;
    const size_t n = (size_t)img.width * img.height;
    uint8_t *rgba = malloc(n * 4);
    for (size_t i = 0; rgba != NULL && i < n; i++) {
        const uint8_t k = img.pixels[i];
        memcpy(rgba + 4 * i, img.palette + 3 * k, 3);
        rgba[4 * i + 3] = k == TRANSPARENT_INDEX ? 0 : 255;
    }
    png_free(img.pixels);
    if (rgba == NULL) exchange_fail(err, "out of memory for %s", path);
    return rgba;
}

/* the four frames as PNG files in `set` */
static int add_frames(FileSet *set, uint8_t *const frames[WHEEL_FRAMES], int w, int h, ExchangeError *err)
{
    for (int k = 0; k < WHEEL_FRAMES; k++) {
        uint8_t *png = NULL;
        size_t len = 0;
        if (png_write_rgba(frames[k], (uint32_t)w, (uint32_t)h, &png, &len) != PNG_OK) { exchange_fail(err, "could not make %s", FRAME_FILES[k]); return 0; }
        if (!fileset_add_owned(set, FRAME_FILES[k], png, len)) { free(png); exchange_fail(err, "out of memory for %s", FRAME_FILES[k]); return 0; }
    }
    return 1;
}

int car_wheels_command(int argc, char **argv, ExchangeError *err)
{
    Args a;
    if (!parse(argc, argv, &a, err)) return 0;
    int w = 0, h = 0;
    uint8_t *rgba = read_rgba(a.in, &w, &h, err), *frames[WHEEL_FRAMES] = { NULL, NULL, NULL, NULL };
    FileSet set;
    fileset_init(&set);
    int ok = rgba != NULL && wheel_frames_make(rgba, w, h, &a.o, frames, err) && add_frames(&set, frames, w, h, err) && fsio_write_tree(a.out, &set, a.force, err);
    if (ok) printf("car wheels: %s: still.png, slow-1.png, slow-2.png and fast.png from %s (%dx%d)\n", a.out, a.in, w, h);
    fileset_free(&set);
    for (int k = 0; k < WHEEL_FRAMES; k++) free(frames[k]);
    free(rgba);
    return ok;
}

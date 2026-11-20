#include "studio/car_view.h"

#include "formats/png.h"
#include "raster/car_scene.h"
#include "raster/quad.h"
#include "studio/fsio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DEFAULT_W = 640, DEFAULT_H = 400, MAX_W = 2048 };

typedef struct {
    const char *cfm, *shot;
    int have_view, have_orbit;
    OrbitView view;
    double yaw, pitch;
    int32_t dist;
    int w, h;
} Args;

static int parse_view(const char *word, OrbitView *out)
{
    static const char *const NAMES[4] = { "front", "side", "rear", "top" };
    for (int i = 0; i < 4; i++)
        if (strcmp(word, NAMES[i]) == 0) return *out = (OrbitView)i, 1;
    return 0;
}

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    a->w = DEFAULT_W, a->h = DEFAULT_H;
    for (int i = 2; i < argc; i++) {
        const int more = i + 1 < argc;
        if (strcmp(argv[i], "--shot") == 0 && more) a->shot = argv[++i];
        else if (strcmp(argv[i], "--view") == 0 && more) {
            if (!parse_view(argv[++i], &a->view)) { exchange_fail(err, "--view takes front, side, rear or top"); return 0; }
            a->have_view = 1;
        } else if (strcmp(argv[i], "--orbit") == 0 && more) {
            long dist = -1;
            if (sscanf(argv[++i], "%lf,%lf,%ld", &a->yaw, &a->pitch, &dist) < 2) { exchange_fail(err, "--orbit takes YAW,PITCH[,DIST]"); return 0; }
            a->dist = dist > 0 ? (int32_t)dist : 0;
            a->have_orbit = 1;
        } else if (strcmp(argv[i], "--size") == 0 && more) {
            if (sscanf(argv[++i], "%dx%d", &a->w, &a->h) != 2 || a->w < 16 || a->w > MAX_W || a->h < 16 || a->h > QUAD_MAX_ROWS)
                { exchange_fail(err, "--size takes WxH, 16 to 2048 wide and up to 1440 high"); return 0; }
        } else if (argv[i][0] != '-' && a->cfm == NULL) a->cfm = argv[i];
        else { exchange_fail(err, "car view: unknown argument '%s'", argv[i]); return 0; }
    }
    if (a->cfm == NULL || a->shot == NULL) { exchange_fail(err, "usage: tnfs-studio car view CAR.CFM --shot OUT.png [--view front|side|rear|top] [--orbit YAW,PITCH,DIST] [--size WxH]"); return 0; }
    return 1;
}

int car_view_command(int argc, char **argv, ExchangeError *err)
{
    Args a;
    if (!parse(argc, argv, &a, err)) return 0;
    size_t size = 0;
    uint8_t *cfm = fsio_read_file(a.cfm, &size, err);
    if (cfm == NULL) return 0;
    CarScene scene;
    if (!car_scene_open(&scene, cfm, size)) {
        free(cfm);
        exchange_fail(err, "%s is not a car (its high model or its bank would not open)", a.cfm);
        return 0;
    }
    Orbit o;
    orbit_init(&o, scene.radius);
    if (a.have_view) orbit_view(&o, a.view);
    if (a.have_orbit) {
        orbit_set_degrees(&o, a.yaw, a.pitch);
        if (a.dist > 0) orbit_set_dist(&o, a.dist);
    }
    const size_t n = (size_t)a.w * (size_t)a.h;
    uint8_t *pixels = malloc(n), *rgba = malloc(4 * n), *png = NULL;
    size_t png_len = 0;
    int ok = 0;
    if (pixels != NULL && rgba != NULL) {
        memset(pixels, CAR_SCENE_CLEAR, n);
        car_scene_render(&scene, &o, pixels, a.w, a.h);
        car_scene_rgba(&scene, pixels, n, rgba);
        if (png_write_rgba(rgba, (uint32_t)a.w, (uint32_t)a.h, &png, &png_len) != PNG_OK) exchange_fail(err, "could not make the picture");
        else ok = fsio_write_file(a.shot, png, png_len, 1, err);
    } else {
        exchange_fail(err, "out of memory");
    }
    if (ok) printf("car view: %s, yaw %.1f pitch %.1f dist %d (home %d, radius %d) -> %s\n", a.cfm, o.yaw / 16.0, o.pitch / 16.0, o.dist, o.dist_home, scene.radius, a.shot);
    free(png), free(rgba), free(pixels);
    car_scene_close(&scene);
    free(cfm);
    return ok;
}

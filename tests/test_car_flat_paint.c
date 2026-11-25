/* TEMPORARY exception (car_livery_flat_paint): the F512TR's rear strip is flat polygons with no texture, fixed red in the original model. After a catalogue paint, with the exception, those polygons take the
 * paint's colour: rendered with two paints, the pixels the exception changes are red without it and the paint's colour with it, so they follow the chosen paint; a car with no such table entry (the
 * Viper) is left as it is. Needs the CD's CARFAMS ($TNFS_DATA or original/ of the source tree); skipped when it is not there. */
#include "formats/car_livery_pass.h"
#include "raster/car_scene.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 640, H = 360, YAWS = 8 };

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

/* the car painted with `rgb` (and the exception when `flat`), rendered from YAWS sides into `rgba` (YAWS pictures one after the other); returns the references the exception changed */
static int shoot(const uint8_t *orig, size_t size, const char *upper, const uint8_t rgb[3], int flat, uint8_t *rgba)
{
    uint8_t *cfm = malloc(size);
    if (cfm == NULL) return -1;
    memcpy(cfm, orig, size);
    const CarLiveryPass pass = { upper, 0, 0, NULL, NULL };
    car_livery_paint(cfm, size, &pass, rgb);
    const int changed = flat ? car_livery_flat_paint(cfm, size, &pass, rgb) : 0;
    static CarScene s;
    static uint8_t px[W * H];
    if (!car_scene_open(&s, cfm, size)) return free(cfm), -1;
    for (int k = 0; k < YAWS; k++) {
        Orbit o;
        orbit_init(&o, s.radius);
        orbit_set_degrees(&o, 360.0 * k / YAWS, 12);
        memset(px, CAR_SCENE_CLEAR, sizeof px);
        car_scene_render(&s, &o, px, W, H);
        car_scene_rgba(&s, px, (size_t)W * H, rgba + (size_t)k * W * H * 4);
    }
    car_scene_close(&s);
    free(cfm);
    return changed;
}

static int near(const uint8_t *c, const uint8_t rgb[3], int tol)
{
    return abs(c[0] - rgb[0]) <= tol && abs(c[1] - rgb[1]) <= tol && abs(c[2] - rgb[2]) <= tol;
}

int main(void)
{
    const char *root = getenv("TNFS_DATA");
    char path[600], fallback[512];
    snprintf(fallback, sizeof fallback, "%s/original", TNFS_SOURCE_DIR);
    if (root == NULL) root = fallback;
    snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/F512TR.CFM", root);
    size_t size;
    uint8_t *orig = slurp(path, &size);
    if (orig == NULL) {
        fprintf(stderr, "no %s: skipped\n", path);
        return 0;
    }
    static uint8_t plain[YAWS * W * H * 4], fixed[YAWS * W * H * 4];
    static const uint8_t PAINTS[2][3] = { { 244, 244, 240 }, { 30, 70, 170 } };   /* a white and a blue */
    for (int p = 0; p < 2; p++) {
        CHECK(shoot(orig, size, "F512TR", PAINTS[p], 0, plain) == 0);
        const int changed = shoot(orig, size, "F512TR", PAINTS[p], 1, fixed);
        CHECK(changed >= 1);   /* the flat reference (both detail levels' tables that have it) */
        long diff = 0, red_before = 0, paint_after = 0;
        for (size_t i = 0; i < (size_t)YAWS * W * H; i++) {
            const uint8_t *a = plain + i * 4, *b = fixed + i * 4;
            if (memcmp(a, b, 4) == 0) continue;
            diff++;
            red_before += a[0] > 150 && a[1] < 90 && a[2] < 90;   /* the original's red */
            paint_after += near(b, PAINTS[p], 40);                 /* the paint's colour (the nearest the palette has) */
        }
        fprintf(stderr, "paint %d: %ld pixels change, %ld were red, %ld are the paint's\n", p, diff, red_before, paint_after);
        CHECK(diff >= 30);                  /* the strip is seen from some side */
        CHECK(red_before * 10 >= diff * 9); /* all of it was the original's red */
        CHECK(paint_after * 10 >= diff * 9);/* and follows the paint now */
    }
    /* a car with no entry in the table is left as it is */
    char vp[600];
    snprintf(vp, sizeof vp, "%s/SIMDATA/CARFAMS/DVIPER.CFM", root);
    size_t vs;
    uint8_t *viper = slurp(vp, &vs);
    if (viper != NULL) {
        CHECK(shoot(viper, vs, "DVIPER", PAINTS[0], 1, fixed) == 0);
        free(viper);
    }
    free(orig);
    return TEST_REPORT();
}

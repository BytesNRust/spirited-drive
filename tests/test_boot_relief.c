/* The boot relief's blend on made-up sources: a piece brighter than TITLE meets it without a step and keeps its detail, the fill takes its
 * edge's level, the picture comes out in the collage's red, an unknown map value is refused. */
#include "game/frontend/boot_relief.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { W = BOOT_RELIEF_W, H = BOOT_RELIEF_H, N = W * H };

static double detail(int x, int y) { return 25 * sin(x / 5.0) * cos(y / 7.0); }
static int lum(const uint8_t *c) { return (30 * c[0] + 59 * c[1] + 11 * c[2]) / 100; }

static void red(uint8_t *c, double v)
{
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    c[0] = (uint8_t)v, c[1] = (uint8_t)(v / 8), c[2] = (uint8_t)(v / 8);
}

typedef struct {
    uint8_t *title, *screen[BOOT_RELIEF_SCREENS], *photo, *map, *out;
} Scene;

/* TITLE on the left, a piece of screen 0 (40 brighter) on the right from x 320, a fill box, the picture at its place */
static void make(Scene *s)
{
    s->title = malloc(3 * N), s->photo = malloc(3 * BOOT_PHOTO_W * BOOT_PHOTO_H), s->map = malloc(N), s->out = malloc(3 * N);
    for (int k = 0; k < BOOT_RELIEF_SCREENS; k++) s->screen[k] = malloc(3 * N);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            red(s->title + 3 * i, 120 + detail(x, y));
            for (int k = 0; k < BOOT_RELIEF_SCREENS; k++) red(s->screen[k] + 3 * i, 160 + detail(x, y));
            s->map[i] = x >= 320 ? BOOT_MAP_PIECE : BOOT_MAP_TITLE;
            if (x >= 420 && x < 520 && y >= 300 && y < 400) s->map[i] = BOOT_MAP_FILL;
        }
    for (int y = 0; y < BOOT_PHOTO_H; y++)
        for (int x = 0; x < BOOT_PHOTO_W; x++) {
            uint8_t *c = s->photo + 3 * (y * BOOT_PHOTO_W + x);
            c[0] = 40, c[1] = 60, c[2] = (uint8_t)(120 + (x % 20 < 10 ? 60 : 0));   /* a blue striped picture */
            s->map[(BOOT_PHOTO_Y + y) * W + BOOT_PHOTO_X + x] = BOOT_MAP_PHOTO;
        }
}

static void unmake(Scene *s)
{
    free(s->title), free(s->photo), free(s->map), free(s->out);
    for (int k = 0; k < BOOT_RELIEF_SCREENS; k++) free(s->screen[k]);
}

static double row_mean(const uint8_t *rgb, int x, int y0, int y1)
{
    double s = 0;
    for (int y = y0; y < y1; y++) s += lum(rgb + 3 * (y * W + x));
    return s / (y1 - y0);
}

static void the_seam_goes_and_the_detail_stays(void)
{
    Scene s;
    make(&s);
    const BootReliefSources in = { s.title, { s.screen[0], s.screen[1], s.screen[2] }, s.photo, s.map };
    CHECK(boot_relief_compose(&in, s.out));
    /* across the seam at x 320 (rows 150..250, away from the fill and the picture): the step was ~31 in luminance */
    const double step = fabs(row_mean(s.out, 320, 150, 250) - row_mean(s.out, 319, 150, 250));
    CHECK(step < 6);
    /* the detail: neighbouring pixels still differ as the sources did, far from the seam */
    double want = 0, got = 0;
    for (int y = 100; y < 140; y++)
        for (int x = 100; x < 140; x++) {
            want += fabs(detail(x + 1, y) - detail(x, y)) * 0.30 * 1.1;
            got += abs(lum(s.out + 3 * (y * W + x + 1)) - lum(s.out + 3 * (y * W + x)));
        }
    CHECK(got > 0.6 * want);
    /* the fill sits at its edge's level, give or take its grain and the lighter shade under the banner */
    const double inside = row_mean(s.out, 470, 330, 370), edge = row_mean(s.out, 530, 330, 370);
    CHECK(fabs(inside - edge) < 25);
    /* the picture comes out red like the rest, not blue */
    const uint8_t *c = s.out + 3 * ((BOOT_PHOTO_Y + 20) * W + BOOT_PHOTO_X + 100);
    CHECK(c[0] > c[2] + 30);
    unmake(&s);
}

static void an_unknown_region_is_refused(void)
{
    Scene s;
    make(&s);
    s.map[1234] = BOOT_MAP_PIECE + BOOT_MAP_PIECES;
    const BootReliefSources in = { s.title, { s.screen[0], s.screen[1], s.screen[2] }, s.photo, s.map };
    CHECK(!boot_relief_compose(&in, s.out));
    unmake(&s);
}

int main(void)
{
    the_seam_goes_and_the_detail_stays();
    an_unknown_region_is_refused();
    return TEST_REPORT();
}

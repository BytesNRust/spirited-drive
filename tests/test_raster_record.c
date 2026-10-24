/* Render bands P3: the rasterizers called on the pixels an active recorder was begun for record, and a flush replays the
 * commands by bands; the picture must be byte for byte what the same calls paint on a target without one, in whatever
 * order they overlap, on any number of threads. Random mixes of every recorded primitive over a random background (the
 * shade and blend ones read it), twisted and off-box shapes included; plus the primitives that are not recorded
 * (flat_plot, flat_peek) in the middle of a recorded run, and a run past the recorder's capacity. */
#include "game/gfx/flat.h"
#include "game/gfx/flat_internal.h"
#include "game/gfx/raster_record.h"
#include "game/gfx/sprite_scaled.h"
#include "game/sys/bands.h"
#include "platform/platform.h"
#include "viewer/modern_road.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 120, H = 90, TW = 16, TH = 12, TRIALS = 300, KINDS = 14 };
enum { BOX_L = 3, BOX_T = 2, BOX_R = W - 3, BOX_B = H - 2 };

static uint32_t rng_state = 0x13572468;
static uint32_t rnd(void)
{
    rng_state ^= rng_state << 13, rng_state ^= rng_state >> 17, rng_state ^= rng_state << 5;
    return rng_state;
}
static int32_t span(int32_t lo, int32_t hi) { return lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1)); }

static uint8_t background[W * H], direct[W * H], recorded[W * H], texels[TW * TH], shade[256], shape[0x10 + TW * TH];
static uint8_t blend_table[256][256];
static const uint8_t *blend_rows[256];
static struct RasterRecorder recorder;   /* big: not on the stack */
static int only_kind = -1;                /* the tests below run all kinds mixed, or this one */
static int painted;                       /* the trials whose draws changed something */

/* a command keeps the texture by pointer, as the game's (the art's) outlive the frame */
static const FshEntry *bitmap(void)
{
    static FshEntry entry;
    entry = (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, TW, TH, { 0, 4, 0, 0 }, texels, sizeof texels };
    return &entry;
}

static const View *the_view(void)
{
    static View view;
    static int made;
    if (!made) view_init(&view, 0, 0, W, H, 90, 90), made = 1;
    return &view;
}

typedef struct {
    int kind;
    int32_t s[8], uv[8], cam[4][3], x, y, w, h;
    int count, mode;
    uint32_t colour;
    int32_t poly[2 * 16];
} Op;

static void random_op(Op *o, int small)
{
    memset(o, 0, sizeof *o);
    o->kind = only_kind >= 0 ? only_kind : (int)(rnd() % KINDS);
    o->mode = (int)(rnd() % 3);
    o->colour = rnd() & 0xff;
    const int32_t cx = span(-20, W + 20), cy = span(-20, H + 20), r = small ? span(1, 4) : span(2, 60);
    static const int sx[4] = { -1, 1, 1, -1 }, sy[4] = { -1, -1, 1, 1 };
    for (int i = 0; i < 4; i++) {
        o->s[2 * i] = cx + sx[i] * span(1, r), o->s[2 * i + 1] = cy + sy[i] * span(1, r);
        o->uv[2 * i] = (i == 0 || i == 3 ? 0 : TW - 1) << 16, o->uv[2 * i + 1] = (i < 2 ? 0 : TH - 1) << 16;
    }
    if (rnd() % 3 == 0 && !small)   /* twisted, thin, off the box */
        for (int i = 0; i < 8; i += 2) o->s[i] = span(-W, 2 * W), o->s[i + 1] = span(-H, 2 * H);
    o->count = 3 + (int)(rnd() % 3);
    for (int i = 0; i < 2 * 16; i += 2) o->poly[i] = span(-30, W + 30), o->poly[i + 1] = span(-30, H + 30);
    if (rnd() % 8 == 0) o->count = 13 + (int)(rnd() % 4);   /* more points than a command holds */
    o->x = span(-40, W + 10), o->y = span(-40, H + 10), o->w = span(-40, 60), o->h = span(-40, 60);
    const int32_t z = span(30, 500), w = span(5, 150);
    for (int i = 0; i < 4; i++) {
        o->cam[i][0] = sx[i] * w + span(-100, 100), o->cam[i][1] = -sy[i] * w + span(-60, 60), o->cam[i][2] = z + span(-20, 80);
    }
}

static void draw(const QuadTarget *q, const Op *o)
{
    const FshEntry *t = bitmap();
    const int32_t *const cam[4] = { o->cam[0], o->cam[1], o->cam[2], o->cam[3] };
    int32_t uv6[6], s6[6];
    memcpy(uv6, o->uv, sizeof uv6);
    memcpy(s6, o->s, sizeof s6);
    switch (o->kind) {
    case 0: quad_draw_mode(q, t, o->uv, o->s, (MapMode)o->mode, shade); break;
    case 1: tri_draw_mode(q, t, uv6, s6, (MapMode)o->mode, shade); break;
    case 2: quad_draw_full_tri(q, t, o->s, o->mode & 1); break;
    case 3: flat_polygon(q, o->colour, o->count, o->poly); break;
    case 4: flat_polygon_shade(q, shade, o->count, o->poly, o->mode & 1); break;
    case 5: flat_line(q, o->s[0], o->s[1], o->s[2], o->s[3], o->colour); break;
    case 6: flat_fill_rect(q, o->x, o->y, o->w, o->h, o->colour); break;
    case 7: sprite_scaled(q, shape, o->w, o->h, o->x, o->y); break;
    case 8: sprite_scaled_blend(q, blend_rows, shape, o->w, o->h, o->x, o->y); break;
    case 9: modern_road_cell(q, the_view(), t, o->uv, cam, o->mode & 1); break;
    case 10: modern_polygon(q, the_view(), t, o->uv, cam, 3 + (o->mode & 1), (MapMode)o->mode, shade); break;
    case 11: modern_flat(q, the_view(), (uint8_t)o->colour, cam, 3 + (o->mode & 1)); break;
    case 12: sprite_scaled_at(q, shape, o->w * 0x1000, o->x, o->y); break;
    case 13: flat_plot(q, o->x, o->y, o->colour), (void)flat_peek(q, o->x + 1, o->y); break;   /* not recorded: syncs */
    }
}

/* `count` random ops made at once and through a recorder on `threads`; whether the pictures agree */
static int agree(int count, int small, int threads)
{
    static Op ops[RASTER_RECORD_MAX * 2 + 100];
    for (int i = 0; i < count; i++) random_op(&ops[i], small);
    for (int i = 0; i < W * H; i++) background[i] = (uint8_t)rnd();
    memcpy(direct, background, sizeof direct);
    memcpy(recorded, background, sizeof recorded);
    const QuadTarget qd = { direct, W, BOX_L, BOX_T, BOX_R, BOX_B };
    for (int i = 0; i < count; i++) draw(&qd, &ops[i]);
    bands_set_threads(threads);
    raster_record_set_runner(threads != 0 ? bands_rows : NULL);
    raster_record_begin(&recorder, recorded);
    const QuadTarget qr = { recorded, W, BOX_L, BOX_T, BOX_R, BOX_B };
    for (int i = 0; i < count; i++) draw(&qr, &ops[i]);
    raster_record_end(&recorder);
    CHECK(raster_active == NULL);
    CHECK(recorder.count == 0);
    painted += memcmp(direct, background, sizeof direct) != 0;
    return memcmp(direct, recorded, sizeof direct) == 0;
}

int main(void)
{
    for (int i = 0; i < TW * TH; i++) {
        const uint32_t r = rnd() % 10;
        texels[i] = r == 0 ? 0xff : r == 1 ? 0xfe : (uint8_t)rnd();
    }
    for (int i = 0; i < 256; i++) {
        shade[i] = (uint8_t)(i * 5 + 3);
        for (int j = 0; j < 256; j++) blend_table[i][j] = (uint8_t)(i * 7 + j * 13);
        blend_rows[i] = blend_table[i];
    }
    shape[0] = 0x7b, shape[4] = TW, shape[6] = TH;
    for (int i = 0; i < TW * TH; i++) shape[0x10 + i] = (uint8_t)(rnd() % 7 == 0 ? 0xff : rnd());
    for (only_kind = 0; only_kind < KINDS; only_kind++)   /* each primitive alone: a failure names it */
        for (int threads = 0; threads <= 4; threads += 2) {
            int bad = 0;
            for (int n = 0; n < 60; n++) bad += !agree(1 + (int)(rnd() % 30), 0, threads);
            if (bad != 0) fprintf(stderr, "kind %d on %d threads: %d / 60 differ\n", only_kind, threads, bad);
            CHECK(bad == 0);
        }
    only_kind = -1;
    for (int threads = 0; threads <= 5; threads++) {
        for (int n = 0; n < TRIALS; n++) CHECK(agree(1 + (int)(rnd() % 40), 0, threads));
        for (int n = 0; n < TRIALS; n++) CHECK(agree(1 + (int)(rnd() % 6), 0, threads));   /* below a band batch */
    }
    CHECK(agree(RASTER_RECORD_MAX + 700, 1, 3));   /* the recorder fills and is flushed on the way */
    CHECK(agree(2 * RASTER_RECORD_MAX + 5, 1, 4));
    CHECK(painted > 1500);
    bands_set_threads(-1);
    platform_shutdown();
    return TEST_REPORT();
}

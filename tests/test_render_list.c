/* Render bands P2: the road's draws recorded in a RenderList and replayed by bands must paint exactly what the same
 * draws paint made at once, in any order they overlap, whatever the threads. Random mixes of the four road draws (the
 * uv quad, the whole texture as a quad and as two triangles, the modern road's cell) over a random background; a list
 * longer than its capacity (flushed on the way) and one shorter than a band batch are covered too. */
#include "game/sys/bands.h"
#include "platform/platform.h"
#include "game/viewer/render_list.h"
#include "viewer/modern_road.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 120, H = 90, TW = 16, TH = 12, TRIALS = 400 };
enum { BOX_L = 3, BOX_T = 2, BOX_R = W - 3, BOX_B = H - 2 };

static uint32_t rng_state = 0x2468ace;
static uint32_t rnd(void)
{
    rng_state ^= rng_state << 13, rng_state ^= rng_state >> 17, rng_state ^= rng_state << 5;
    return rng_state;
}
static int32_t span(int32_t lo, int32_t hi) { return lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1)); }

static uint8_t background[W * H], direct[W * H], listed[W * H], texels[TW * TH];
static int painted;   /* the trials whose draws changed something */
static RenderList list;   /* big: not on the stack */

static FshEntry bitmap(void)
{
    return (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, TW, TH, { 0, 4, 0, 0 }, texels, sizeof texels };
}

typedef struct {
    RenderKind kind;
    int32_t uv[8], screen[8], cam[4][3];
    int transparent;
} Op;

static void random_op(Op *o, int small)
{
    memset(o, 0, sizeof *o);
    o->kind = (RenderKind)(rnd() % 4);
    o->transparent = (int)(rnd() & 1);
    const int32_t cx = span(-20, W + 20), cy = span(-20, H + 20), r = small ? span(1, 4) : span(2, 60);
    static const int sx[4] = { -1, 1, 1, -1 }, sy[4] = { -1, -1, 1, 1 };
    for (int i = 0; i < 4; i++) {
        o->screen[2 * i] = cx + sx[i] * span(1, r), o->screen[2 * i + 1] = cy + sy[i] * span(1, r);
        o->uv[2 * i] = (i == 0 || i == 3 ? 0 : TW - 1) << 16, o->uv[2 * i + 1] = (i < 2 ? 0 : TH - 1) << 16;
    }
    if (rnd() % 3 == 0 && !small)   /* twisted, thin, off the box: the quads quad.c retraces */
        for (int i = 0; i < 8; i += 2) o->screen[i] = span(-W, 2 * W), o->screen[i + 1] = span(-H, 2 * H);
    if (o->kind == RENDER_ROAD_CELL) {
        const int32_t z = span(30, 500), w = span(5, 150);
        for (int i = 0; i < 4; i++) {
            o->cam[i][0] = sx[i] * w + span(-100, 100), o->cam[i][1] = -sy[i] * w + span(-60, 60);
            o->cam[i][2] = z + span(-20, 80);
        }
    }
}

static const View *the_view(void)
{
    static View view;
    static int made;
    if (!made) view_init(&view, 0, 0, W, H, 90, 90), made = 1;
    return &view;
}

static void draw_now(const QuadTarget *q, const Op *o, const FshEntry *t)
{
    switch (o->kind) {
    case RENDER_QUAD: quad_draw(q, t, o->uv, o->screen, o->transparent); break;
    case RENDER_FULL: quad_draw_full(q, t, o->screen, o->transparent); break;
    case RENDER_FULL_TRI: quad_draw_full_tri(q, t, o->screen, o->transparent); break;
    case RENDER_ROAD_CELL: {
        const int32_t *const cam[4] = { o->cam[0], o->cam[1], o->cam[2], o->cam[3] };
        modern_road_cell(q, the_view(), t, o->uv, cam, o->transparent);
        break;
    }
    }
}

static void record(RenderList *l, const Op *o, const FshEntry *t)
{
    switch (o->kind) {
    case RENDER_QUAD: render_list_add_quad(l, t, o->uv, o->screen, o->transparent); break;
    case RENDER_FULL: render_list_add_full(l, t, o->screen, o->transparent); break;
    case RENDER_FULL_TRI: render_list_add_full_tri(l, t, o->screen, o->transparent); break;
    case RENDER_ROAD_CELL: {
        const int32_t *const cam[4] = { o->cam[0], o->cam[1], o->cam[2], o->cam[3] };
        render_list_add_road_cell(l, the_view(), t, o->uv, cam, o->transparent);
        break;
    }
    }
}

/* `count` random ops drawn at once and through the list on `threads`; returns whether they agree */
static int agree(int count, int small, int threads)
{
    static Op ops[RENDER_LIST_MAX * 2 + 100];
    for (int i = 0; i < count; i++) random_op(&ops[i], small);
    for (int i = 0; i < W * H; i++) background[i] = (uint8_t)rnd();
    memcpy(direct, background, sizeof direct);
    memcpy(listed, background, sizeof listed);
    const FshEntry t = bitmap();
    const QuadTarget qd = { direct, W, BOX_L, BOX_T, BOX_R, BOX_B };
    for (int i = 0; i < count; i++) draw_now(&qd, &ops[i], &t);
    const QuadTarget ql = { listed, W, BOX_L, BOX_T, BOX_R, BOX_B };
    bands_set_threads(threads);
    render_list_begin(&list, &ql);
    for (int i = 0; i < count; i++) record(&list, &ops[i], &t);
    render_list_flush(&list);
    CHECK(list.count == 0);
    painted += memcmp(direct, background, sizeof direct) != 0;
    return memcmp(direct, listed, sizeof direct) == 0;
}

int main(void)
{
    for (int i = 0; i < TW * TH; i++) {
        const uint32_t r = rnd() % 10;
        texels[i] = r == 0 ? 0xff : (uint8_t)rnd();
    }
    for (int threads = 0; threads <= 5; threads++) {
        for (int n = 0; n < TRIALS; n++) CHECK(agree(1 + (int)(rnd() % 40), 0, threads));
        for (int n = 0; n < TRIALS; n++) CHECK(agree(1 + (int)(rnd() % 6), 0, threads));   /* below a band batch */
    }
    CHECK(agree(RENDER_LIST_MAX + 700, 1, 3));   /* the list fills and is flushed on the way */
    CHECK(agree(2 * RENDER_LIST_MAX + 5, 1, 4));
    CHECK(painted > 600);
    bands_set_threads(-1);
    platform_shutdown();
    return TEST_REPORT();
}

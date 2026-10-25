/* The row limits of the rasterizers (quad.h QUAD_MAX_ROWS, flat.h FLAT_MAX_ROWS) were the exe's 768 and 600 rows; they
 * are lifted for the render scale (re/notes/render-scale-design.md). A shape drawn far down a tall surface must be the
 * pixels the same shape makes on a short one: the rows below 768 / 600 are drawn as the rows above, and a target of
 * the new limit is accepted. Needs no exe. */
#include "game/gfx/flat.h"
#include "game/gfx/quad.h"
#include "test.h"

#include <stdlib.h>

enum { W = 160, SHORT_H = 200, SHAPE_ROWS = 160 };

static uint8_t tex[64];
static FshEntry texture(void)
{
    for (int i = 0; i < 64; i++) tex[i] = (uint8_t)(i % 7 == 0 ? 0xFF : i + 1);
    FshEntry e = { .type = FSH_TYPE_BITMAP, .width = 8, .height = 8, .payload = tex, .payload_size = 64 };
    return e;
}

typedef void (*Draw)(const QuadTarget *t, int dy);

static void d_polygon(const QuadTarget *t, int dy)
{
    const int32_t p[10] = { 20, 10 + dy, 90, 20 + dy, 120, 70 + dy, 60, 140 + dy, 15, 80 + dy };
    flat_polygon(t, 0x42, 5, p);
}
static void d_merge(const QuadTarget *t, int dy)
{
    const int32_t p[8] = { 30, 20 + dy, 100, 25 + dy, 90, 120 + dy, 25, 100 + dy };
    flat_polygon_merge(t, 0x43, 4, p);
}
static void d_line(const QuadTarget *t, int dy) { flat_line(t, 5, 3 + dy, 150, 150 + dy, 0x44); }
static void d_rect(const QuadTarget *t, int dy) { flat_fill_rect(t, 10, 10 + dy, 120, 100, 0x45); }
static void d_quad(const QuadTarget *t, int dy)
{
    const FshEntry e = texture();
    const int32_t uv[8] = { 0, 0, (8 << 16) - 1, 0, (8 << 16) - 1, (8 << 16) - 1, 0, (8 << 16) - 1 };
    const int32_t s[8] = { 20, 10 + dy, 120, 20 + dy, 130, 120 + dy, 15, 100 + dy };
    quad_draw(t, &e, uv, s, 1);
}
static void d_full(const QuadTarget *t, int dy)
{
    const FshEntry e = texture();
    const int32_t s[8] = { 10, 10 + dy, 140, 14 + dy, 150, 140 + dy, 12, 150 + dy };
    quad_draw_full(t, &e, s, 0);
}
static void d_full_tri(const QuadTarget *t, int dy)
{
    const FshEntry e = texture();
    const int32_t s[8] = { 10, 10 + dy, 140, 14 + dy, 150, 140 + dy, 12, 150 + dy };
    quad_draw_full_tri(t, &e, s, 1);
}
static void d_tri(const QuadTarget *t, int dy)
{
    const FshEntry e = texture();
    const int32_t uv[6] = { 0, 0, 7, 0, 3, 7 };
    const int32_t s[6] = { 10, 140 + dy, 150, 150 + dy, 70, 5 + dy };
    tri_draw(t, &e, uv, s, 0);
}

static const struct { const char *name; Draw draw; } kinds[] = {
    { "flat_polygon", d_polygon }, { "flat_polygon_merge", d_merge }, { "flat_line", d_line },
    { "flat_fill_rect", d_rect },  { "quad_draw", d_quad },           { "quad_draw_full", d_full },
    { "quad_draw_full_tri", d_full_tri }, { "tri_draw", d_tri },
};

/* The shape on a short surface at dy 0 and on a tall one (the limit's rows) at dy, whole clips. */
static void check_kind(int k, int tall_rows, int dy)
{
    uint8_t *shortb = calloc((size_t)W * SHORT_H, 1), *tall = calloc((size_t)W * (size_t)tall_rows, 1);
    const QuadTarget st = { shortb, W, 0, 0, W, SHORT_H }, tt = { tall, W, 0, 0, W, tall_rows };
    kinds[k].draw(&st, 0);
    kinds[k].draw(&tt, dy);
    long painted = 0, same = 1, above = 1;
    for (int y = 0; y < tall_rows; y++)
        for (int x = 0; x < W; x++) {
            const uint8_t t = tall[(size_t)y * W + x];
            if (y < dy) above &= t == 0;
            else if (y < dy + SHORT_H) {
                same &= t == shortb[(size_t)(y - dy) * W + x];
                painted += t != 0;
            } else above &= t == 0;
        }
    if (!same || !above || painted < 100) fprintf(stderr, "%s at row %d of %d: same %ld above %ld painted %ld\n", kinds[k].name, dy, tall_rows, same, above, painted);
    CHECK(same);
    CHECK(above);
    CHECK(painted >= 100);
    free(shortb);
    free(tall);
}

int main(void)
{
    CHECK(QUAD_MAX_ROWS >= 1440);
    CHECK(FLAT_MAX_ROWS >= 1440);
    for (int k = 0; k < (int)(sizeof kinds / sizeof kinds[0]); k++) {
        check_kind(k, 1440, 1440 - SHORT_H);   /* the last rows of the limit */
        check_kind(k, 1000, 800);              /* past the exe's 768 and 600 */
        check_kind(k, 700, 520);               /* past 600, below 768 */
    }
    /* one row past the limit draws nothing */
    uint8_t *over = calloc((size_t)W * (QUAD_MAX_ROWS + 1), 1);
    const QuadTarget ot = { over, W, 0, 0, W, QUAD_MAX_ROWS + 1 };
    d_polygon(&ot, 0), d_quad(&ot, 0), d_tri(&ot, 0);
    long any = 0;
    for (size_t i = 0; i < (size_t)W * (QUAD_MAX_ROWS + 1); i++) any += over[i];
    CHECK_EQ_INT(any, 0);
    free(over);
    return TEST_REPORT();
}

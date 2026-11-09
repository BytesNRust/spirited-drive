/* The race's 2D layer (640 x 480, LAYER_KEY where nothing was drawn) put over the wide 3D view (layer_compose.h). */
#include "viewer/layer_compose.h"
#include "viewer/screen_layout.h"
#include "test.h"

#include <string.h>

enum { W = 854, H = 480, LW = 640, SCALE_854 = (854 << 16) / 640 };

static uint8_t layer[LW * H], wide[W * H];

static void box(int x0, int y0, int x1, int y1, uint8_t c)
{
    for (int y = y0; y < y1; y++) memset(layer + y * LW + x0, c, (size_t)(x1 - x0));
}

/* port (render bands): layer_compose_rows over unequal bands, in any order, is the whole compose, byte for byte */
static void test_bands_make_the_whole(void)
{
    enum { WIDE_MAX = 1120, PIECES = 3 };
    static uint8_t rnd_layer[LW * H], background[WIDE_MAX * H], whole[WIDE_MAX * H], banded[WIDE_MAX * H];
    uint32_t seed = 12345;
    for (int i = 0; i < LW * H; i++) {
        seed = seed * 1664525u + 1013904223u;
        rnd_layer[i] = (seed >> 24) % 5 == 0 ? LAYER_KEY : (uint8_t)(seed >> 16);
    }
    for (int i = 0; i < WIDE_MAX * H; i++) seed = seed * 1664525u + 1013904223u, background[i] = (uint8_t)(seed >> 8);
    const LayerPiece pieces[PIECES] = { { { 520, 306, 633, 446 }, 700, 330, 0, 0 },
                                        { { 200, 20, 440, 60 }, 150, 10, 1, 0 },
                                        { { 10, 400, 90, 470 }, 30, 390, 0, 90 } };
    static const int widths[3] = { 640, 854, 1120 };
    for (int w = 0; w < 3; w++)
        for (int in_car = 0; in_car < 2; in_car++) {
            const int32_t scale = (int32_t)(((int64_t)widths[w] << 16) / LW);
            const LayerCompose c = { LW, H, widths[w], scale, in_car, pieces, PIECES };
            memcpy(whole, background, sizeof whole);
            layer_compose(&c, rnd_layer, whole);
            memcpy(banded, background, sizeof banded);
            static const int cuts[] = { 0, 7, 8, 150, 151, 333, 480 };
            for (int b = (int)(sizeof cuts / sizeof cuts[0]) - 2; b >= 0; b--)   /* last band first */
                layer_compose_rows(&c, rnd_layer, banded, cuts[b], cuts[b + 1]);
            CHECK_MEM(whole, banded, sizeof whole);
            CHECK(memcmp(whole, background, sizeof whole) != 0);   /* it drew something */
        }
}


/* port (perf): the pieces' mask is set only over the rows a band reads. Pieces reaching past the layer's right and bottom edges (a piece's own draw reads only inside it), bands cut anywhere, a second frame with other pieces after the
 * first on the same thread (a mask row left set would show): every banding is the whole compose, byte for byte */
static void test_mask_rows_follow_the_band(void)
{
    enum { WIDE_MAX = 1120, PIECES = 5 };
    static uint8_t rnd_layer[LW * H], background[WIDE_MAX * H], whole[WIDE_MAX * H], banded[WIDE_MAX * H];
    uint32_t seed = 777;
    for (int i = 0; i < LW * H; i++) {
        seed = seed * 1664525u + 1013904223u;
        rnd_layer[i] = (seed >> 24) % 7 == 0 ? LAYER_KEY : (uint8_t)(seed >> 16);
    }
    for (int i = 0; i < WIDE_MAX * H; i++) seed = seed * 1664525u + 1013904223u, background[i] = (uint8_t)(seed >> 8);
    const LayerPiece sets[2][PIECES] = {
        { { { 0, 0, 30, 10 }, 0, 0, 0, 0 }, { { 600, 460, 700, 500 }, 500, 440, 0, 0 }, { { 100, 100, 100, 200 }, 0, 0, 0, 0 }, { { 0, 0, 640, 12 }, 0, 0, 0, 0 }, { { 300, 470, 340, 480 }, 300, 470, 0, 0 } },
        { { { 20, 150, 220, 151 }, 20, 150, 0, 0 }, { { 400, 0, 410, 480 }, 400, 0, 0, 0 }, { { 7, 7, 8, 8 }, 7, 7, 0, 0 }, { { 630, 300, 700, 400 }, 630, 300, 0, 0 }, { { 0, 479, 640, 700 }, 0, 479, 0, 0 } } };
    static const int widths[3] = { 640, 854, 1120 };
    for (int frame = 0; frame < 2; frame++)
        for (int w = 0; w < 3; w++)
            for (int in_car = 0; in_car < 2; in_car++) {
                const int32_t scale = (int32_t)(((int64_t)widths[w] << 16) / LW);
                const LayerCompose c = { LW, H, widths[w], scale, in_car, sets[frame], PIECES };
                memcpy(whole, background, sizeof whole);
                layer_compose(&c, rnd_layer, whole);
                for (int run = 0; run < 6; run++) {
                    memcpy(banded, background, sizeof banded);
                    int cut = 0;
                    while (cut < H) {   /* bands of 1 .. 120 rows, any size */
                        seed = seed * 1664525u + 1013904223u;
                        const int next = cut + 1 + (int)((seed >> 16) % 120);
                        layer_compose_rows(&c, rnd_layer, banded, cut, next < H ? next : H);
                        cut = next;
                    }
                    CHECK_MEM(whole, banded, sizeof whole);
                }
            }
}

/* port (render scale): the upscale of the layer is every drawn pixel as a scale x scale block, LAYER_KEY leaving the 3D
 * as it was; against the per-pixel definition, for layers of runs of both, odd widths, every scale */
static uint32_t rnd_state = 12345;
static uint32_t rnd(void)
{
    rnd_state = rnd_state * 1664525u + 1013904223u;
    return rnd_state >> 8;
}

static void test_upscale(void)
{
    enum { UW = 203, UH = 60 };
    static uint8_t src[UW * UH], got[UW * UH * 16], want[UW * UH * 16];
    for (int s = 1; s <= 4; s++) {
        for (int y = 0; y < UH; y++) {
            const int mode = (int)(rnd() % 5);   /* all key, all drawn, runs of both, scattered */
            for (int x = 0; x < UW;) {
                const int run = mode == 4 ? 1 : 1 + (int)(rnd() % 40);
                const int key = mode == 0 || (mode != 1 && (rnd() & 1));
                for (int i = 0; i < run && x < UW; i++, x++) src[y * UW + x] = key ? LAYER_KEY : (uint8_t)(rnd() % 255);
            }
        }
        const size_t n = (size_t)UW * UH * s * s;
        for (size_t i = 0; i < n; i++) got[i] = want[i] = (uint8_t)rnd();
        for (int y = 0; y < UH; y++)
            for (int x = 0; x < UW; x++)
                if (src[y * UW + x] != LAYER_KEY)
                    for (int j = 0; j < s; j++)
                        memset(want + ((size_t)(y * s + j) * UW + x) * s, src[y * UW + x], (size_t)s);
        layer_upscale_rows(src, UW, s, got, 0, 25);   /* in two bands */
        layer_upscale_rows(src, UW, s, got, 25, UH);
        CHECK(memcmp(got, want, n) == 0);
    }
}


/* The compose before the speed-ups (all-key rows skipped, the pieces' source columns made once, the frame's columns clipped once), kept here as the reference: the same loops, pixel by pixel. */
static void ref_put(const LayerCompose *c, uint8_t *wide, int x, int y, uint8_t p)
{
    if (p != LAYER_KEY && x >= 0 && x < c->width && y >= 0 && y < c->layer_h) wide[(int64_t)y * c->width + x] = p;
}

/* the pieces' pixels of the layer, marked for the rest's loop (the pieces' rects set, used, then cleared again) */
enum { REF_MASK_W = 640, REF_MASK_H = 480 };
static uint8_t ref_mask[REF_MASK_W * REF_MASK_H];   /* port (render bands): per thread */

/* port (perf): only the layer rows [lo, hi) the band reads are marked (every band marked every piece's whole rect before), the thread's mask once in a local (a
 * thread local read per pixel is a call on macOS) and a row of it set at a time. The mask outside [lo, hi) stays 0 (every mark is cleared with the same rows). */
static void ref_mark_pieces(const LayerCompose *c, uint8_t on, int lo, int hi)
{
    uint8_t *const mask = ref_mask;
    for (int i = 0; i < c->piece_count; i++) {
        const LayerRect *r = &c->pieces[i].from;
        const int left = r->left < 0 ? 0 : r->left, right = r->right < c->layer_w ? r->right : c->layer_w;
        const int top = r->top < lo ? lo : r->top, bottom = r->bottom < hi ? r->bottom : hi;
        if (left >= right) continue;
        for (int y = top; y < bottom; y++) memset(mask + y * c->layer_w + left, on, (size_t)(right - left));
    }
}

/* the rest of the layer: inside the car the dash (each wide pixel from the layer pixel screen_dash_x / _y give),
 * outside it centred */
static void ref_rest(const LayerCompose *c, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    if (c->layer_w > REF_MASK_W || c->layer_h > REF_MASK_H) return;
    uint8_t *const mask = ref_mask;
    if (!c->in_car) {
        const int lo = top < 0 ? 0 : top, hi = bottom < c->layer_h ? bottom : c->layer_h;   /* the layer rows this band reads: its own */
        ref_mark_pieces(c, 1, lo, hi);
        const int dx = (c->width - c->layer_w) / 2;
        for (int y = top; y < bottom; y++)
            for (int x = 0; x < c->layer_w; x++)
                if (!mask[y * c->layer_w + x]) ref_put(c, wide, x + dx, y, layer[y * c->layer_w + x]);
        ref_mark_pieces(c, 0, lo, hi);
        return;
    }
    /* the dash: the band's rows are the layer rows screen_dash_y gives (it never goes back as y goes down, so its ends are the range's) when the frame is wider than the layer */
    int first = top, last = bottom - 1;
    if (c->width > c->layer_w && bottom > top) first = screen_dash_y(top), last = screen_dash_y(bottom - 1);
    const int lo = first < 0 ? 0 : first, hi = last + 1 < c->layer_h ? last + 1 : c->layer_h;
    ref_mark_pieces(c, 1, lo, hi);
    const int width = c->width < SCREEN_DASH_MOST_WIDTH ? c->width : SCREEN_DASH_MOST_WIDTH;
    const int *column = screen_dash_columns(c->width);
    for (int y = top; y < bottom; y++) {
        const int sy = c->width > c->layer_w ? screen_dash_y(y) : y;
        if (sy < 0 || sy >= c->layer_h) continue;
        const uint8_t *row = layer + sy * c->layer_w, *masked = mask + sy * c->layer_w;
        uint8_t *out = wide + (int64_t)y * c->width;
        for (int x = 0; x < width; x++) {
            const int sx = column[x];
            if (sx < c->layer_w && !masked[sx] && row[sx] != LAYER_KEY) out[x] = row[sx];
        }
    }
    ref_mark_pieces(c, 0, lo, hi);
}

static void ref_piece(const LayerCompose *c, const LayerPiece *p, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    const int w = p->from.right - p->from.left, h = p->from.bottom - p->from.top;
    const int32_t scale = p->scaled ? c->scale : 1 << 16;
    const int out_w = p->out_w > 0 ? p->out_w : (int)((int64_t)w * scale >> 16), out_h = (int)((int64_t)h * scale >> 16);
    for (int y = 0; y < out_h; y++) {
        if (p->y + y < top || p->y + y >= bottom) continue;   /* port (render bands): the rows asked for */
        const int sy = p->from.top + (int)(((int64_t)y << 16) / scale);
        for (int x = 0; x < out_w; x++) {
            int sx = p->from.left + (int)(((int64_t)x << 16) / scale);
            if (sx >= p->from.right) sx = p->from.right - 1;   /* a set out_w a column past the scaled one */
            if (sx < c->layer_w && sy < c->layer_h) ref_put(c, wide, p->x + x, p->y + y, layer[sy * c->layer_w + sx]);
        }
    }
}

static void ref_layer_compose_rows(const LayerCompose *c, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    if (top < 0) top = 0;
    if (bottom > c->layer_h) bottom = c->layer_h;
    if (bottom <= top) return;
    ref_rest(c, layer, wide, top, bottom);
    for (int i = 0; i < c->piece_count; i++) ref_piece(c, &c->pieces[i], layer, wide, top, bottom);
}


/* the new compose against the reference: random layers (rows of nothing but the key, runs of it, scattered), random widths and pieces (partly or wholly off the frame, scaled or not, a
 * set width), both places of the layer, and random bands, over the same background: every byte of the frame the same */
static void test_matches_the_reference(void)
{
    enum { WIDE_MAX = 1500 };
    static uint8_t lay[LW * H], bg[WIDE_MAX * H], got[WIDE_MAX * H], want[WIDE_MAX * H];
    uint32_t seed = 2024;
    #define RND(n) (seed = seed * 1664525u + 1013904223u, (int)((seed >> 8) % (uint32_t)(n)))
    long checked = 0;
    for (int round = 0; round < 120; round++) {
        const int density = RND(4);   /* 0: all key, 1: a few pixels, 2: half, 3: almost full */
        for (int y = 0; y < H; y++) {
            const int kind = RND(5);   /* 0, 1: a row of nothing but the key; 2: one pixel that is not (any column, the ends too); else by density */
            const int lone = RND(4) == 0 ? LW - 1 : RND(4) == 0 ? 0 : RND(LW);
            for (int x = 0; x < LW; x++) {
                const int key = density == 0 || kind < 2 || (kind == 2 ? x != lone : density == 1 ? RND(40) != 0 : density == 2 ? RND(2) == 0 : RND(10) == 0);
                lay[y * LW + x] = key ? LAYER_KEY : (uint8_t)RND(250);
            }
        }
        for (int i = 0; i < WIDE_MAX * H; i++) bg[i] = (uint8_t)RND(250);
        const int width = 640 + RND(861);
        LayerPiece pieces[6];
        const int n = RND(7);
        for (int i = 0; i < n; i++) {
            const int l = RND(600), t = RND(440), pw = 1 + RND(120), ph = 1 + RND(60);
            const int px = RND(width + 200) - 100, py = RND(500) - 20, scaled = RND(2), set_w = RND(3) == 0 ? RND(300) : 0;
            pieces[i] = (LayerPiece){ { l, t, l + pw, t + ph }, px, py, scaled, set_w };
        }
        const LayerCompose c = { LW, H, width, (int32_t)(((int64_t)width << 16) / LW), RND(2), pieces, n };
        memcpy(got, bg, sizeof got), memcpy(want, bg, sizeof want);
        int cut = 0;
        while (cut < H) {   /* the same bands for both, in order */
            const int next = cut + 1 + RND(120);
            layer_compose_rows(&c, lay, got, cut, next > H ? H : next);
            ref_layer_compose_rows(&c, lay, want, cut, next > H ? H : next);
            cut = next;
        }
        CHECK(memcmp(got, want, sizeof got) == 0);
        checked++;
    }
    #undef RND
    CHECK_EQ_INT(checked, 120);
    /* a piece wider than the columns table holds (4096) takes the per-pixel division: the same bytes */
    static uint8_t big_got[4600 * H], big_want[4600 * H];
    memset(big_got, 7, sizeof big_got), memset(big_want, 7, sizeof big_want);
    for (int i = 0; i < LW * H; i++) lay[i] = i % 7 == 0 ? LAYER_KEY : (uint8_t)(i * 31);
    const LayerPiece wide_piece = { { 10, 20, 300, 90 }, -50, 15, 1, 4200 };
    const LayerCompose big = { LW, H, 4600, (int32_t)(((int64_t)4600 << 16) / LW), 0, &wide_piece, 1 };
    layer_compose_rows(&big, lay, big_got, 0, H);
    ref_layer_compose_rows(&big, lay, big_want, 0, H);
    CHECK(memcmp(big_got, big_want, sizeof big_got) == 0);
}

int main(void)
{
    test_upscale();
    /* a dash pixel, a piece, and nothing drawn elsewhere */
    memset(layer, LAYER_KEY, sizeof layer);
    box(300, 400, 340, 440, 7);         /* the dash */
    box(520, 306, 633, 446, 9);         /* a piece (the map) */
    memset(wide, 3, sizeof wide);       /* the 3D view */
    const LayerPiece pieces[1] = { { { 520, 306, 633, 446 }, 854 - 113 - 6, 480 - 140 - 6, 0, 0 } };
    box(100, HUD_BAR_HEIGHT, 101, HUD_BAR_HEIGHT + 1, 4);   /* the roof's first line, under the scoreboard */
    const LayerCompose c = { LW, H, W, SCALE_854, 1, pieces, 1 };
    layer_compose(&c, layer, wide);
    /* the 3D stays where the layer drew nothing */
    CHECK_EQ_INT(wide[100 * W + 10], 3);
    /* the dash at its own size in the middle (screen_dash_x / _y): layer (320, 410) -> wide (427, 410 + DASH_DROP) */
    CHECK_EQ_INT(wide[(410 + DASH_DROP) * W + 427], 7);
    CHECK_EQ_INT(wide[(400 + DASH_DROP - 1) * W + 427], 3);
    /* the roof's first line over the lines above the drop, where its column lands stretched */
    int roof_x = 0;
    while (screen_dash_x(W, roof_x) < 100) roof_x++;
    CHECK_EQ_INT(wide[0 * W + roof_x], 4);
    CHECK_EQ_INT(wide[(HUD_BAR_HEIGHT + DASH_DROP) * W + roof_x], 4);
    CHECK_EQ_INT(wide[(HUD_BAR_HEIGHT + DASH_DROP + 1) * W + roof_x], 3);
    /* the piece unscaled at its anchor, its old place in the enlarged dash not drawn */
    CHECK_EQ_INT(wide[(480 - 140 - 6) * W + (854 - 113 - 6)], 9);
    CHECK_EQ_INT(wide[(480 - 6 - 1) * W + (854 - 6 - 1)], 9);
    CHECK_EQ_INT(wide[(480 - 140 - 6 - 1) * W + (854 - 113 - 6 - 1)], 3);

    /* outside the car: no scaling, what is not a piece lands centred */
    memset(wide, 3, sizeof wide);
    const LayerCompose out = { LW, H, W, 1 << 16, 0, pieces, 1 };
    layer_compose(&out, layer, wide);
    CHECK_EQ_INT(wide[420 * W + 320 + (W - LW) / 2], 7);
    CHECK_EQ_INT(wide[420 * W + 320], 3);
    CHECK_EQ_INT(wide[(480 - 140 - 6) * W + (854 - 113 - 6)], 9);

    /* a scaled piece (the mirror): its pixels enlarged at the anchor */
    memset(layer, LAYER_KEY, sizeof layer);
    box(543, 53, 633, 104, 5);
    memset(wide, 3, sizeof wide);
    const LayerPiece mirror[1] = { { { 543, 53, 633, 104 }, 700, 38, 1, 0 } };
    const LayerCompose m = { LW, H, W, SCALE_854, 1, mirror, 1 };
    layer_compose(&m, layer, wide);
    CHECK_EQ_INT(wide[38 * W + 700], 5);
    const int mw = (633 - 543) * 854 / 640;
    CHECK_EQ_INT(wide[38 * W + 700 + mw - 1], 5);
    CHECK_EQ_INT(wide[38 * W + 700 + mw + 1], 3);

    /* a scaled piece told its width (the mirror's right half, to the mirror's end): one column past its own scaled
     * width, that column its last pixel's, not the view behind */
    memset(wide, 3, sizeof wide);
    const LayerPiece to_end[1] = { { { 543, 53, 633, 104 }, 700, 38, 1, mw + 1 } };
    const LayerCompose e = { LW, H, W, SCALE_854, 1, to_end, 1 };
    layer_compose(&e, layer, wide);
    CHECK_EQ_INT(wide[38 * W + 700 + mw], 5);
    CHECK_EQ_INT(wide[38 * W + 700 + mw + 1], 3);

    /* at 640 with scale 1 and the pieces where they are, the layer over the view as it is */
    static uint8_t same[LW * H];
    memset(same, 3, sizeof same);
    memset(layer, LAYER_KEY, sizeof layer);
    box(300, 400, 340, 440, 7);
    const LayerPiece here[1] = { { { 520, 306, 633, 446 }, 520, 306, 0, 0 } };
    const LayerCompose id = { LW, H, LW, 1 << 16, 1, here, 1 };
    layer_compose(&id, layer, same);
    CHECK_EQ_INT(same[420 * LW + 320], 7);
    CHECK_EQ_INT(same[10 * LW + 10], 3);

    /* the map's see-through back grabbed at its wide anchor: what the in-car frame shows there, the windshield
     * (LAYER_KEY) left as it was */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < LW; x++) layer[y * LW + x] = (uint8_t)(10 + (x * 7 + y * 13) % 120);
    box(560, 300, 600, 330, LAYER_KEY);
    memset(wide, 3, sizeof wide);
    const LayerCompose dash = { LW, H, W, SCALE_854, 1, NULL, 0 };
    layer_compose(&dash, layer, wide);
    enum { BW = 113, BH = 140, BX = 854 - 113 - 6 + 2 - 40, BY = 480 - 140 - 6 + 2 - 60 };
    static uint8_t back[BW * BH];
    memset(back, 200, sizeof back);
    layer_dash_grab(layer, W, BX, BY, back, BW, BH);
    int same_as_frame = 1, holes = 0;
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            const uint8_t shown = wide[(BY + y) * W + BX + x], got = back[y * BW + x];
            if (shown == 3) holes += got == 200;
            else same_as_frame &= got == shown;
        }
    CHECK(same_as_frame);
    CHECK(holes > 0);
    test_bands_make_the_whole();
    test_mask_rows_follow_the_band();
    test_matches_the_reference();
    return TEST_REPORT();
}

/* The C PSD reader and the mask rules (formats/psd_read.h, psd_mask.h): small PSDs written here, raw and RLE, with a group, a layer of its own
 * bounds, a unicode name, and wrong files the reader must refuse in words, never crash on (every cut of a good file too). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/car_mask_codec.h"
#include "formats/psd_mask.h"
#include "formats/psd_read.h"
#include "../game/race/dash_mask.h"
#include "psd_fixture.h"

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; fprintf(stderr, "FAIL line %d: ", __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

enum { W = 10, H = 3 };

/* the colours of the car rules, one texel each (alpha, then the class they read as) */
static const struct { uint8_t rgba[4]; int cls; } CAR_TEXELS[W] = {
    { { 255, 255, 255, 0 },   CAR_MASK_PAINT },      /* transparent */
    { { 10, 10, 10, 255 },    CAR_MASK_PAINT },      /* dark */
    { { 20, 40, 220, 255 },   CAR_MASK_SHINE },      /* blue */
    { { 20, 220, 40, 255 },   CAR_MASK_BODY },       /* green */
    { { 255, 0, 255, 255 },   CAR_MASK_BLACK },      /* magenta */
    { { 255, 0, 0, 255 },     CAR_MASK_BRAKE },
    { { 255, 128, 0, 255 },   CAR_MASK_POSITION },
    { { 255, 255, 0, 255 },   CAR_MASK_HEADLIGHT },
    { { 0, 255, 255, 255 },   CAR_MASK_REVERSE },
    { { 150, 150, 150, 255 }, CAR_MASK_MATTE },                 /* anything else */
};

static uint8_t *car_pixels(void)
{
    uint8_t *px = malloc(W * H * 4);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) memcpy(px + (y * W + x) * 4, CAR_TEXELS[x].rgba, 4);
    return px;
}

static void expect_car(const FxBuf *psd, const char *what)
{
    uint8_t *grey = NULL;
    uint32_t w = 0, h = 0;
    char why[128] = "";
    CHECK(psd_car_mask_bytes(psd->p, psd->n, &grey, &w, &h, why, sizeof why), "%s: %s", what, why);
    if (grey == NULL) return;
    CHECK(w == W && h == H, "%s: size %ux%u", what, w, h);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) CHECK(grey[y * W + x] == CAR_TEXELS[x].cls, "%s: texel %d,%d is %d, not %d", what, x, y, grey[y * W + x], CAR_TEXELS[x].cls);
    free(grey);
}

static void expect_refused(const FxBuf *psd, const char *words, const char *what)
{
    uint8_t *grey = NULL;
    uint32_t w, h;
    char why[128] = "";
    CHECK(!psd_car_mask_bytes(psd->p, psd->n, &grey, &w, &h, why, sizeof why) && grey == NULL, "%s: not refused", what);
    CHECK(strstr(why, words) != NULL, "%s: the words were \"%s\", not \"%s\"", what, why, words);
}

static void test_values(void)
{
    CHECK(CAR_MASK_PAINT == 0 && CAR_MASK_SHINE == 128 && CAR_MASK_BODY == 192 && CAR_MASK_BLACK == 224 && CAR_MASK_BRAKE == 240 && CAR_MASK_POSITION == 244, "car classes");
    CHECK(DASH_MASK_AUTO == 0 && DASH_MASK_DASH == 64 && DASH_MASK_DIAL == 128 && DASH_MASK_NOSE == 160 && DASH_MASK_BLADE == 192 && DASH_MASK_LIT == 255, "dash classes");
}

static void test_car_layouts(void)
{
    uint8_t *px = car_pixels();
    uint8_t blue[W * H * 4];
    for (int i = 0; i < W * H; ++i) blue[i * 4] = 0, blue[i * 4 + 1] = 0, blue[i * 4 + 2] = 255, blue[i * 4 + 3] = 255;
    for (int rle = 0; rle < 2; ++rle) {
        FxLayer l[2] = { { "texture", NULL, 0, 0, 0, W, H, blue, rle, 1, 0 }, { "mask", NULL, 0, 0, 0, W, H, px, rle, 1, 0 } };
        FxBuf f = fx_psd(W, H, l, 2, 8, 3, 1);
        expect_car(&f, rle ? "RLE" : "raw");
        free(f.p);
    }
    FxLayer opaque[1] = { { " Mask ", NULL, 0, 0, 0, W, H, px, 0, 0, 0 } };   /* no alpha channel: opaque; the name is trimmed, any case */
    FxBuf f = fx_psd(W, H, opaque, 1, 8, 3, 1);
    uint8_t *grey = NULL; uint32_t w, h; char why[128];
    CHECK(psd_car_mask_bytes(f.p, f.n, &grey, &w, &h, why, sizeof why) && grey != NULL, "no alpha channel: %s", why);
    CHECK(grey != NULL && grey[0] == CAR_MASK_MATTE, "a transparent-looking texel with no alpha channel is opaque white, matte: %d", grey != NULL ? grey[0] : -1);
    free(grey); free(f.p);
    FxLayer uni[1] = { { "layer 1", "mask", 0, 0, 0, W, H, px, 0, 1, 0 } };   /* the unicode name wins */
    f = fx_psd(W, H, uni, 1, 8, 3, 1);
    expect_car(&f, "luni name");
    free(f.p);
    FxLayer grp[4] = { { "</Layer group>", NULL, PSD_LAYER_DIVIDER, 0, 0, 0, 0, NULL, 0, 0, 0 }, { "mask", NULL, 0, 0, 0, W, H, px, 1, 1, 0 },
                       { "Group 1", NULL, PSD_LAYER_OPEN_FOLDER, 0, 0, 0, 0, NULL, 0, 0, 0 }, { "texture", NULL, 0, 0, 0, W, H, blue, 0, 1, 0 } };
    f = fx_psd(W, H, grp, 4, 8, 3, 1);
    expect_car(&f, "mask inside a group");
    free(f.p);
    free(px);
}

static void test_bounds(void)
{
    /* a 4 x 2 layer at (-2, 1) over the 10 x 3 canvas: only its columns 2..3 and row 0..1 land on it; a texel outside is not there */
    uint8_t px[4 * 2 * 4];
    for (int i = 0; i < 8; ++i) memcpy(px + i * 4, (i % 4) < 2 ? (const uint8_t[]){ 255, 0, 0, 255 } : (const uint8_t[]){ 0, 0, 255, 255 }, 4);
    FxLayer l[1] = { { "mask", NULL, 0, -2, 1, 2, 3, px, 1, 1, 0 } };
    FxBuf f = fx_psd(W, H, l, 1, 8, 3, 1);
    uint8_t *grey; uint32_t w, h; char why[128];
    CHECK(psd_car_mask_bytes(f.p, f.n, &grey, &w, &h, why, sizeof why), "bounds: %s", why);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int inside = x < 2 && y >= 1;
            const int want = !inside ? 0 : (x + 2) % 4 < 2 ? CAR_MASK_BRAKE : CAR_MASK_SHINE;
            CHECK(grey[y * W + x] == want, "bounds: texel %d,%d is %d, not %d", x, y, grey[y * W + x], want);
        }
    free(grey); free(f.p);
    FxLayer none[1] = { { "mask", NULL, 0, 0, 0, 0, 0, NULL, 0, 1, 0 } };   /* nothing painted: all paint */
    f = fx_psd(W, H, none, 1, 8, 3, 1);
    CHECK(psd_car_mask_bytes(f.p, f.n, &grey, &w, &h, why, sizeof why) && grey[5] == 0 && w == W, "an empty mask layer: %s", why);
    free(grey); free(f.p);
}

static void test_refusals(void)
{
    uint8_t px[W * H * 4];
    memset(px, 255, sizeof px);
    FxLayer one[1] = { { "mask", NULL, 0, 0, 0, W, H, px, 0, 1, 0 } };
    FxBuf f = fx_psd(W, H, one, 1, 16, 3, 1); expect_refused(&f, "16-bit", "16-bit"); free(f.p);
    f = fx_psd(W, H, one, 1, 8, 4, 1); expect_refused(&f, "not RGB", "CMYK"); free(f.p);
    f = fx_psd(W, H, one, 1, 8, 3, 2); expect_refused(&f, "PSB", "PSB"); free(f.p);
    FxLayer zip[1] = { { "mask", NULL, 0, 0, 0, W, H, px, 0, 1, 2 } };
    f = fx_psd(W, H, zip, 1, 8, 3, 1); expect_refused(&f, "ZIP", "ZIP"); free(f.p);
    FxLayer other[1] = { { "texture", NULL, 0, 0, 0, W, H, px, 0, 1, 0 } };
    f = fx_psd(W, H, other, 1, 8, 3, 1); expect_refused(&f, "no layer named \"mask\"", "no mask layer"); free(f.p);
    FxBuf junk = { 0 }; fx_put(&junk, "GIF89a not a psd", 16); expect_refused(&junk, "not a PSD", "junk"); free(junk.p);
    f = fx_psd(W, H, one, 1, 8, 3, 1);   /* every cut of a good file is refused, and never read past its end (ASan) */
    for (size_t cut = 0; cut < f.n; cut += 7) {
        FxBuf part = { f.p, cut, cut };
        uint8_t *grey = NULL; uint32_t w, h; char why[128] = "";
        const int ok = psd_car_mask_bytes(part.p, part.n, &grey, &w, &h, why, sizeof why);
        CHECK(!ok && why[0] != '\0', "a file cut at %zu was read", cut);
        free(grey);
    }
    free(f.p);
}

static void test_dash(void)
{
    /* mask layer: a row of dash colours; limpia layer: a red mark over the first texel; colour layer: one texel */
    static const uint8_t M[7][4] = { { 0, 0, 255, 255 }, { 0, 255, 0, 255 }, { 255, 255, 0, 255 }, { 0, 255, 255, 255 }, { 255, 0, 0, 255 }, { 0, 0, 0, 255 }, { 200, 100, 50, 255 } };
    uint8_t mask[7 * 4], limpia[7 * 4] = { 0 }, colour[7 * 4] = { 0 };
    memcpy(mask, M, sizeof mask);
    limpia[0] = 255, limpia[3] = 255;      /* the blade over the dash texel */
    limpia[5 * 4] = 255, limpia[5 * 4 + 3] = 255;   /* over a black texel: a blade too */
    colour[2 * 4] = 10, colour[2 * 4 + 1] = 200, colour[2 * 4 + 2] = 30, colour[2 * 4 + 3] = 255;
    const int want[7] = { DASH_MASK_BLADE, DASH_MASK_DIAL, DASH_MASK_LIT, DASH_MASK_NOSE, DASH_MASK_BLADE, DASH_MASK_BLADE, DASH_MASK_AUTO };
    FxLayer l[3] = { { "mask", NULL, 0, 0, 0, 7, 1, mask, 0, 1, 0 }, { "wipers", NULL, 0, 0, 0, 7, 1, limpia, 1, 1, 0 }, { "Color", NULL, 0, 0, 0, 7, 1, colour, 0, 1, 0 } };
    FxBuf f = fx_psd(7, 1, l, 3, 8, 3, 1);
    uint8_t *cls = NULL, *rgba = NULL; uint32_t w, h; char why[128] = "";
    CHECK(psd_dash_mask_bytes(f.p, f.n, &cls, &rgba, &w, &h, why, sizeof why) && cls != NULL, "dash: %s", why);
    for (int x = 0; cls != NULL && x < 7; ++x) CHECK(cls[x] == want[x], "dash texel %d is %d, not %d", x, cls[x], want[x]);
    CHECK(rgba != NULL && rgba[2 * 4] == 10 && rgba[2 * 4 + 1] == 200 && rgba[2 * 4 + 3] == 255 && rgba[3] == 0, "the colour layer");
    free(cls); free(rgba);
    cls = rgba = NULL;
    CHECK(psd_dash_mask_bytes(f.p, f.n, &cls, NULL, &w, &h, why, sizeof why) && cls != NULL, "dash without asking for colour");
    free(cls); free(f.p);
    FxLayer plain[1] = { { "mask", NULL, 0, 0, 0, 7, 1, mask, 0, 1, 0 } };
    f = fx_psd(7, 1, plain, 1, 8, 3, 1);
    cls = NULL; rgba = (uint8_t *)1;
    CHECK(psd_dash_mask_bytes(f.p, f.n, &cls, &rgba, &w, &h, why, sizeof why) && rgba == NULL, "a dash with no colour layer gives NULL");
    free(cls); free(f.p);
}

int main(void)
{
    test_values();
    test_car_layouts();
    test_bounds();
    test_refusals();
    test_dash();
    if (g_fail != 0) return fprintf(stderr, "%d failed\n", g_fail), 1;
    puts("psd_mask: ok");
    return 0;
}

/* The PC's alternative sky as the PS1 sky's draw takes it (psx_sky_draw.h): a made-up 4 x 2 'horz' picture, its own palette and a race palette. */
#include "viewer/psx_sky_draw.h"
#include "test.h"

#include <string.h>

enum { W = 4, H = 2 };

static void test_indexed_and_in_palette(void)
{
    uint8_t texels[W * H] = { 0, 1, 1, 0, 1, 1, 0, 0 };
    uint8_t decode[768] = { 0 };
    decode[3 * 1] = 200, decode[3 * 1 + 1] = 40, decode[3 * 1 + 2] = 40;   /* texel colour 1: red */
    FshEntry horizon = { { 0 }, FSH_TYPE_BITMAP, 0, W, H, { 0, 1, 0, 0 }, texels, sizeof texels };
    PsxSkyDraw d;
    memset(&d, 0, sizeof d);
    CHECK(psx_sky_draw_make_indexed(&d, &horizon, decode));
    CHECK(d.ready && d.horz.width == W && d.horz.height == H && d.horz.misc[1] == 1);   /* the horizon row is the picture's own */
    CHECK(d.horz.payload != texels && memcmp(d.horz.payload, texels, sizeof texels) == 0);   /* a copy */
    CHECK(psx_sky_draw_in_palette(&d) == NULL);   /* no target yet */
    /* the race's palette as the world is shown: entry 7 a red, entry 9 a blue; red's nearest is 7 */
    uint8_t target[768] = { 0 };
    target[3 * 7] = 190, target[3 * 7 + 1] = 50, target[3 * 7 + 2] = 50;
    target[3 * 9] = 30, target[3 * 9 + 1] = 30, target[3 * 9 + 2] = 200;
    psx_sky_draw_target(&d, target);
    const FshEntry *shown = psx_sky_draw_in_palette(&d);
    CHECK(shown != NULL && shown->payload != NULL && shown->width == W);
    CHECK(shown->payload[1] == 7 && shown->payload[0] == horizon_smooth_nearest(&d.smooth, 0, 0, 0));
    /* the target changes: the picture follows it */
    target[3 * 7] = 20, target[3 * 7 + 1] = 20, target[3 * 7 + 2] = 220;
    target[3 * 9] = 200, target[3 * 9 + 1] = 45, target[3 * 9 + 2] = 45;
    psx_sky_draw_target(&d, target);
    shown = psx_sky_draw_in_palette(&d);
    CHECK(shown->payload[1] == 9);
    psx_sky_draw_free(&d);
    CHECK(!d.ready && d.mapped == NULL);
}

static void test_rejected(void)
{
    PsxSkyDraw d;
    memset(&d, 0, sizeof d);
    uint8_t texels[3] = { 0 };
    uint8_t decode[768] = { 0 };
    const FshEntry short_payload = { { 0 }, FSH_TYPE_BITMAP, 0, W, H, { 0 }, texels, sizeof texels };
    CHECK(!psx_sky_draw_make_indexed(&d, &short_payload, decode));
    CHECK(!psx_sky_draw_make_indexed(&d, NULL, decode));
    CHECK(!d.ready);
}

int main(void)
{
    test_indexed_and_in_palette();
    test_rejected();
    return TEST_REPORT();
}

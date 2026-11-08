/* The wiper blades' drawing (wiper_draw.h): a blade darkens the pixels along it at its angle for the tick, leaves the rest of the box alone and stays inside it. */
#include "game/viewer/wiper_draw.h"
#include "tests/test.h"

#include <string.h>

enum { W = 200, H = 120 };

static void test_the_blade_darkens_along_its_angle(void)
{
    static uint8_t px[W * H];
    uint8_t dark[256];
    for (int i = 0; i < 256; i++) dark[i] = (uint8_t)(i == 0xFF ? 0xFF : 7);
    memset(px, 200, sizeof px);
    const int32_t box[4] = { 0, 0, W, H };
    WiperPass w = { { { 100.0f, 100.0f, 80.0f, 3.14159265f, 1.5707963f, 0, 0, 0, 0, 0 } }, 1, wiper_timing(WIPER_RAIN) };
    wiper_draw(px, W, box, &w, 0, dark);   /* parked: pointing left from the pivot */
    CHECK_EQ_INT(px[100 * W + 60], 7);     /* on the blade, 40 px to the left */
    CHECK_EQ_INT(px[100 * W + 100 - 80 + 3], 7);   /* near its tip */
    CHECK_EQ_INT(px[30 * W + 100], 200);   /* straight up: not there at the park */
    CHECK_EQ_INT(px[10 * W + 10], 200);    /* the far corner untouched */
    memset(px, 200, sizeof px);
    wiper_draw(px, W, box, &w, (uint32_t)w.timing.sweep / 2, dark);   /* at the top: straight up */
    CHECK_EQ_INT(px[60 * W + 100], 7);
    CHECK_EQ_INT(px[100 * W + 60], 200);   /* and no longer to the left */
}

static void test_it_stays_inside_the_box_and_in_the_pixels_it_is_given(void)
{
    static uint8_t px[W * H];
    uint8_t dark[256];
    for (int i = 0; i < 256; i++) dark[i] = 7;
    memset(px, 200, sizeof px);
    const int32_t box[4] = { 20, 10, 120, 90 };   /* a box inside the buffer, its own origin */
    WiperPass w = { { { 10.0f, 70.0f, 300.0f, 3.14159265f * 0.9f, 1.0f, 0, 0, 0, 0, 0 } }, 1, wiper_timing(WIPER_STORM) };   /* reaches far outside it */
    wiper_draw(px, W, box, &w, 0, dark);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (x < box[0] || x >= box[2] || y < box[1] || y >= box[3]) CHECK_EQ_INT(px[y * W + x], 200);
    int inside = 0;
    for (int y = box[1]; y < box[3]; y++)
        for (int x = box[0]; x < box[2]; x++) inside += px[y * W + x] == 7;
    CHECK(inside > 0);
    wiper_draw(NULL, W, box, &w, 0, dark);        /* no pixels, no blades: nothing to do */
    w.count = 0;
    wiper_draw(px, W, box, &w, 0, dark);
}

int main(void)
{
    test_the_blade_darkens_along_its_angle();
    test_it_stays_inside_the_box_and_in_the_pixels_it_is_given();
    return TEST_REPORT();
}

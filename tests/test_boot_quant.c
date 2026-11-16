/* The boot screen's quantiser: few colours come out as they are, a ramp keeps its order, every index is inside the colours made. */
#include "game/frontend/boot_quant.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static void few_colours_stay_exact(void)
{
    uint8_t rgb[3 * 6] = { 200, 10, 10, 200, 10, 10, 40, 0, 0, 40, 0, 0, 255, 255, 255, 40, 0, 0 };
    uint8_t pal[768], idx[6];
    const int made = boot_quant(rgb, 6, 8, pal, idx);
    CHECK(made >= 3 && made <= 8);
    for (int i = 0; i < 6; i++) CHECK(memcmp(&pal[3 * idx[i]], &rgb[3 * i], 3) == 0);
}

static void a_ramp_keeps_its_order_and_fits(void)
{
    enum { N = 256 * 64 };
    uint8_t *rgb = malloc(3 * N), *idx = malloc(N), pal[768];
    for (int i = 0; i < N; i++) rgb[3 * i] = (uint8_t)(i / 64), rgb[3 * i + 1] = (uint8_t)(i / 512), rgb[3 * i + 2] = 0;
    const int made = boot_quant(rgb, N, 238, pal, idx);
    CHECK(made > 1 && made <= 238);
    int worst = 0, ordered = 1;
    for (int i = 0; i < N; i++) {
        CHECK(idx[i] < made);
        const int d = abs(pal[3 * idx[i]] - rgb[3 * i]);
        worst = d > worst ? d : worst;
        if (i >= 64 && pal[3 * idx[i]] < pal[3 * idx[i - 64]]) ordered = 0;
    }
    CHECK(worst <= 8);
    CHECK(ordered);
    free(rgb), free(idx);
}

/* a box whose last layer along its widest channel holds more than half its pixels is still split (that layer the new box): a photo's
 * dominant colour at one end (a bright sky, a black car) never leaves the rest in one box, a small red (tail lights) its mean of a brown */
static void a_box_heavy_at_its_end_is_split(void)
{
    enum { N = 100 };
    uint8_t rgb[3 * N], idx[N], pal[768];
    for (int i = 0; i < N; i++) rgb[3 * i] = 250, rgb[3 * i + 1] = 250, rgb[3 * i + 2] = 250;   /* 88 near white */
    for (int i = 0; i < 12; i++) rgb[3 * i] = (uint8_t)(16 * i), rgb[3 * i + 1] = 20, rgb[3 * i + 2] = 20;   /* 12 reds, dark to bright */
    const int made = boot_quant(rgb, N, 16, pal, idx);
    CHECK_EQ_INT(made, 13);
    int worst = 0;
    for (int i = 0; i < N; i++)
        for (int c = 0; c < 3; c++) {
            const int d = abs(pal[3 * idx[i] + c] - rgb[3 * i + c]);
            worst = d > worst ? d : worst;
        }
    CHECK_EQ_INT(worst, 0);   /* 13 colours, 13 boxes: each its own */
}

int main(void)
{
    few_colours_stay_exact();
    a_ramp_keeps_its_order_and_fits();
    a_box_heavy_at_its_end_is_split();
    return TEST_REPORT();
}

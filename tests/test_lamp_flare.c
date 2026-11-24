/* The tail light halo (lamp_flare.h): the blob scan, the radius, the dithered level, the draw  */
#include "tests/test.h"

#include "game/race/lamp_flare.h"

#include <stdlib.h>
#include <string.h>

enum { W = 40, H = 24, LAMP = 77 };

static void disc(uint8_t *px, int cx, int cy, int r, uint8_t v)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) px[y * W + x] = v;
}

static void test_blobs(void)
{
    uint8_t member[256] = { 0 };
    member[LAMP] = 1;
    uint8_t px[W * H];
    memset(px, 3, sizeof px);
    disc(px, 8, 12, 3, LAMP);    /* 29 pixels */
    disc(px, 30, 12, 2, LAMP);   /* 13 pixels */
    disc(px, 20, 4, 3, 99);      /* another index, not a lamp */
    LampBlob b[LAMP_FLARE_BLOBS];
    const int n = lamp_blobs(px, W, 0, 0, W - 1, H - 1, member, b, LAMP_FLARE_BLOBS);
    CHECK_EQ_INT(n, 2);
    CHECK(b[0].area > b[1].area);      /* the largest first */
    CHECK_EQ_INT(b[0].cx, 8);
    CHECK_EQ_INT(b[0].cy, 12);
    CHECK_EQ_INT(b[1].cx, 30);
    CHECK_EQ_INT(b[1].cy, 12);
    /* a rectangle that holds only the right blob finds only it; one holding none finds none */
    CHECK_EQ_INT(lamp_blobs(px, W, 20, 0, W - 1, H - 1, member, b, LAMP_FLARE_BLOBS), 1);
    CHECK_EQ_INT(lamp_blobs(px, W, 14, 0, 24, H - 1, member, b, LAMP_FLARE_BLOBS), 0);
    CHECK_EQ_INT(lamp_blobs(px, W, 0, 0, W - 1, H - 1, member, b, 1), 1);   /* the cap */
    /* two touching discs are one blob */
    memset(px, 3, sizeof px);
    disc(px, 10, 12, 3, LAMP);
    disc(px, 15, 12, 3, LAMP);
    CHECK_EQ_INT(lamp_blobs(px, W, 0, 0, W - 1, H - 1, member, b, LAMP_FLARE_BLOBS), 1);
    CHECK_EQ_INT(b[0].value, LAMP);
}

/* the whole frame's blobs (the street lamps': many, small, far): a frame bigger than the car rectangle's cap, the nearest (largest) kept when there are more than asked for */
static void test_frame_blobs(void)
{
    enum { FW = 1600, FH = 900, MAXB = 32 };   /* 1.44 million pixels: past lamp_blobs' cap */
    uint8_t *px = malloc((size_t)FW * FH);
    CHECK(px != NULL);
    if (px == NULL) return;
    memset(px, 3, (size_t)FW * FH);
    uint8_t member[256] = { 0 };
    member[LAMP] = 1, member[LAMP + 1] = 1;
    for (int i = 0; i < 40; i++)   /* 40 blobs of growing size, in a row */
        for (int y = 100; y < 100 + 1 + i / 4; y++)
            for (int x = 20 + i * 30; x < 20 + i * 30 + 1 + i / 4; x++) px[(size_t)y * FW + x] = i % 2 ? LAMP : LAMP + 1;   /* a lamp's two entries: one blob each */
    static LampBlob b[MAXB];
    const int n = lamp_blobs_frame(px, FW, 0, 0, FW - 1, FH - 1, member, b, MAXB);
    CHECK_EQ_INT(n, MAXB);
    for (int i = 1; i < n; i++) CHECK(b[i - 1].area >= b[i].area);   /* the largest first */
    CHECK_EQ_INT(b[0].area, 10 * 10);                                /* the biggest of the 40 is there... */
    CHECK(b[n - 1].area >= 4);                                       /* ...and the 8 smallest (1 and 4 pixels) were the ones dropped */
    CHECK_EQ_INT(lamp_blobs_frame(px, FW, 0, 0, FW - 1, FH - 1, member, b, 0), 0);
    CHECK_EQ_INT(lamp_blobs_frame(px, FW, 800, 500, FW - 1, FH - 1, member, b, MAXB), 0);   /* a rectangle with none */
    free(px);
}

/* a blob counts as a lamp only where a lamp's own sprite was drawn: the traffic signs the reflectors light are the same entries, but their rectangles are none of these */
static void test_blob_in_rects(void)
{
    const LampBlob b = { .cx = 100, .cy = 50, .area = 9, .value = LAMP };
    const int rects[2][4] = { { 10, 10, 40, 40 }, { 90, 30, 120, 80 } };
    CHECK_EQ_INT(lamp_blob_in_rects(&b, rects, 2, 0), 1);          /* inside the second */
    CHECK_EQ_INT(lamp_blob_in_rects(&b, rects, 1, 0), 0);          /* the first alone: outside */
    CHECK_EQ_INT(lamp_blob_in_rects(&b, rects, 0, 0), 0);          /* no sprite drawn: no lamp */
    CHECK_EQ_INT(lamp_blob_in_rects(&b, NULL, 2, 0), 0);
    const int near[1][4] = { { 102, 52, 120, 80 } };
    CHECK_EQ_INT(lamp_blob_in_rects(&b, near, 1, 0), 0);           /* just outside */
    CHECK_EQ_INT(lamp_blob_in_rects(&b, near, 1, 2), 1);           /* a margin of two pixels takes it in */
    const int flipped[1][4] = { { 120, 80, 90, 30 } };
    CHECK_EQ_INT(lamp_blob_in_rects(&b, flipped, 1, 0), 1);        /* corners in any order */
}

static void test_rects_overflow(void)
{
    static LampRects list;
    lamp_rects_reset(&list);
    for (int i = 0; i < LAMP_RECTS_MAX; i++) CHECK_EQ_INT(lamp_rects_push(&list, i, 0, i + 1, 1), 1);
    CHECK_EQ_INT(list.count, LAMP_RECTS_MAX);
    CHECK_EQ_INT(lamp_rects_push(&list, 500, 500, 600, 600), 0);   /* full: not kept */
    CHECK_EQ_INT(lamp_rects_push(&list, 500, 500, 600, 600), 0);
    CHECK_EQ_INT(list.count, LAMP_RECTS_MAX);
    CHECK_EQ_INT(list.dropped, 2);
    const LampBlob b = { .cx = 550, .cy = 550, .area = 9, .value = LAMP };
    CHECK_EQ_INT(lamp_blob_in_rects(&b, (const int (*)[4])list.rects, list.count, 3), 0);   /* in a dropped rect: no halo */
    lamp_rects_reset(&list);
    CHECK_EQ_INT(list.count, 0);
    CHECK_EQ_INT(list.dropped, 0);
    CHECK_EQ_INT(lamp_rects_push(NULL, 0, 0, 1, 1), 0);
}

static void test_radius(void)
{
    CHECK_EQ_INT(lamp_flare_radius(0), LAMP_FLARE_MIN_RADIUS);
    CHECK_EQ_INT(lamp_flare_radius(12), 2 * LAMP_FLARE_REACH / 10);   /* the blob's r = 2, times the reach */
    CHECK(lamp_flare_radius(100) >= 3 * 5);                          /* a lamp of a hundred pixels glows a few times its own size */
    CHECK(lamp_flare_radius(100) > lamp_flare_radius(12));
    CHECK_EQ_INT(lamp_flare_radius(1000000), LAMP_FLARE_MAX_RADIUS);
    CHECK_EQ_INT(lamp_flare_radius_scaled(100, 100), lamp_flare_radius(100));            /* 100 %: the same */
    CHECK_EQ_INT(lamp_flare_radius_scaled(100, 50), lamp_flare_radius(100));             /* never smaller */
    CHECK_EQ_INT(lamp_flare_radius_scaled(100, 200), lamp_flare_radius(100) * 2);        /* a powerful light's halo is bigger */
    CHECK_EQ_INT(lamp_flare_radius_scaled(1000000, 300), LAMP_FLARE_BIG_RADIUS);         /* and capped */
}

static void test_level(void)
{
    const int r = 12;
    int sum_in = 0, sum_out = 0;
    for (int dy = -3; dy <= 3; dy++)
        for (int dx = -3; dx <= 3; dx++) sum_in += lamp_halo_level(dx, dy, r);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = 8; dx <= r; dx++) sum_out += lamp_halo_level(dx, dy, r);
    CHECK(sum_in > sum_out);                         /* the centre is brighter than the rim */
    CHECK_EQ_INT(lamp_halo_level(r, 0, r), 0);       /* nothing at and past the radius */
    CHECK_EQ_INT(lamp_halo_level(r + 5, r + 5, r), 0);
    CHECK_EQ_INT(lamp_halo_level(0, 0, 0), 0);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            const int l = lamp_halo_level(dx, dy, r);
            CHECK(l >= 0 && l <= LAMP_FLARE_LEVELS);
        }
    /* the dither: near the centre both levels appear over a block, so it is a pattern and not a flat disc */
    int seen[LAMP_FLARE_LEVELS + 1] = { 0 };
    for (int dy = -r / 2; dy <= r / 2; dy++)
        for (int dx = -r / 2; dx <= r / 2; dx++) seen[lamp_halo_level(dx, dy, r)]++;
    CHECK(seen[1] > 0);
    CHECK(seen[2] > 0);
    CHECK(seen[3] > 0);
}

static uint8_t map_one[256], map_two[256], map_three[256];

static void test_draw(void)
{
    for (int i = 0; i < 256; i++) map_one[i] = i == 77 ? 77 : 11, map_two[i] = i == 77 ? 77 : 22, map_three[i] = i == 77 ? 77 : 33;
    const uint8_t *const map[LAMP_FLARE_LEVELS] = { map_one, map_two, map_three };
    uint8_t px[W * H];
    memset(px, 3, sizeof px);
    lamp_halo_draw(px, W, 0, 0, W - 1, H - 1, 20, 12, 8, map);
    int ones = 0, twos = 0, threes = 0, kept = 0, outside = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const uint8_t v = px[y * W + x];
            ones += v == 11, twos += v == 22, threes += v == 33, kept += v == 3;
            if (v != 3 && (x - 20) * (x - 20) + (y - 12) * (y - 12) >= 64) outside++;
        }
    CHECK(ones > 0 && twos > 0 && threes > 0 && kept > 0);
    CHECK_EQ_INT(outside, 0);
    CHECK(px[12 * W + 20] == 33 || px[12 * W + 20] == 22);   /* the centre is lit */
    /* a lamp pixel under the halo keeps its colour (the map's own entry for it) */
    memset(px, 3, sizeof px);
    px[12 * W + 20] = 77;
    lamp_halo_draw(px, W, 0, 0, W - 1, H - 1, 20, 12, 8, map);
    CHECK_EQ_INT(px[12 * W + 20], 77);
    /* the clip: a halo half off the surface and one off it entirely draw only inside it */
    memset(px, 3, sizeof px);
    lamp_halo_draw(px, W, 0, 0, W - 1, H - 1, 0, 0, 8, map);
    lamp_halo_draw(px, W, 0, 0, W - 1, H - 1, -50, -50, 8, map);
    CHECK(px[0] != 3);
    /* a clip rectangle smaller than the surface keeps the pixels outside it */
    memset(px, 3, sizeof px);
    lamp_halo_draw(px, W, 18, 10, 22, 14, 20, 12, 8, map);
    CHECK_EQ_INT(px[0], 3);
    CHECK_EQ_INT(px[12 * W + 10], 3);
}

static void test_far_fade(void)
{
    CHECK_EQ_INT(lamp_far_fade(0, 6400), 100);        /* the near model's range: all of it */
    CHECK_EQ_INT(lamp_far_fade(6400, 6400), 100);     /* at the change of model: still all of it (no step there) */
    CHECK_EQ_INT(lamp_far_fade(9600, 6400), 50);      /* half way out */
    CHECK_EQ_INT(lamp_far_fade(12800, 6400), 0);      /* twice: gone */
    CHECK_EQ_INT(lamp_far_fade(40000, 6400), 0);
    CHECK_EQ_INT(lamp_far_fade(7000, 6400) > lamp_far_fade(8000, 6400), 1);   /* falls steadily with the distance */
    CHECK_EQ_INT(lamp_far_fade(9999, 0), 100);        /* no far distance known: all of it */
}

/* port (traffic_halo): the traffic's halo follows the lit lamps' low light, as the racers' does (dawn, dusk, night), not only the night: setting on and lamps low */
static void test_traffic_halo_in_low_light(void)
{
    CHECK_EQ_INT(lamp_flare_traffic_halo(1, 1), 1);   /* dawn or dusk: the lamps are lit */
    CHECK_EQ_INT(lamp_flare_traffic_halo(1, 0), 0);   /* noon: lit by nothing */
    CHECK_EQ_INT(lamp_flare_traffic_halo(0, 1), 0);   /* the setting off */
    CHECK_EQ_INT(lamp_flare_traffic_halo(0, 0), 0);
}

/* port (lamp_flares): a tunnel lamp's halo fades in and out with how much of the lamp's own light shows, instead of switching on and off: the small far lamps (4..20 px) show a bright
 * texel in one frame and none in the next, and the halo blinked with it */
static void test_lamp_halo_fades(void)
{
    CHECK_EQ_INT(lamp_fade_target(0, 0), 0);
    CHECK_EQ_INT(lamp_fade_target(46, 1), 0);   /* the road's asphalt and paint (46 % of the glow) never keep a halo */
    CHECK_EQ_INT(lamp_fade_target(50, 0), 0);   /* a halo starts at 55 % ... */
    CHECK_EQ_INT(lamp_fade_target(55, 0), 100);
    CHECK_EQ_INT(lamp_fade_target(50, 1), 100);   /* ... and is kept down to 47 % */
    CHECK_EQ_INT(lamp_fade_target(137, 0), 100);
    int level = 0, miss = 0;   /* a lamp comes in: a ramp, not a flash */
    for (int f = 1; f <= 3; f++) {
        const int next = lamp_fade_step(level, 100, &miss);
        CHECK(next > level && next - level <= LAMP_FADE_IN);
        level = next;
    }
    CHECK_EQ_INT(level, 100);
    for (int f = 1; f <= LAMP_FADE_HOLD; f++) CHECK_EQ_INT(lamp_fade_step(level, 0, &miss), 100);   /* not found: it stays LAMP_FADE_HOLD frames ... */
    for (int f = 1; f <= 10; f++) {   /* ... then goes in steps */
        const int next = lamp_fade_step(level, 0, &miss);
        CHECK_EQ_INT(next, level - LAMP_FADE_OUT);
        level = next;
    }
    CHECK_EQ_INT(level, 0);
    /* a small lamp whose bright texel shows one frame in two (70 %, then 45 %): its halo never goes out and never moves */
    level = 0, miss = 0;
    int lowest = 100, step_max = 0;
    for (int f = 0; f < 40; f++) {
        const int next = lamp_fade_step(level, lamp_fade_target(f % 2 ? 45 : 70, level > 0), &miss);
        if (f >= 6 && next < lowest) lowest = next;
        if (abs(next - level) > step_max) step_max = abs(next - level);
        level = next;
    }
    CHECK(lowest == 100 && step_max <= LAMP_FADE_IN);
    CHECK_EQ_INT(lamp_fade_match_px(3), LAMP_FADE_MATCH_MIN_PX);   /* a far lamp: the least */
    CHECK_EQ_INT(lamp_fade_match_px(23), 23);                      /* a near lamp moves more pixels a frame: as many as it is wide */
}

int main(void)
{
    test_lamp_halo_fades();
    test_traffic_halo_in_low_light();
    test_far_fade();
    test_blobs();
    test_frame_blobs();
    test_blob_in_rects();
    test_rects_overflow();
    test_radius();
    test_level();
    test_draw();
    return TEST_REPORT();
}

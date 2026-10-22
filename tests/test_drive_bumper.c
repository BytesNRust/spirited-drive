/* port: the bumper view's extras (drive_bumper.h): EXTRAS' Bumper Mirror and Bumper Radar as a race's start, F1 / F2
 * turning them over on a press, the mirror's picture under the scoreboard's right end with a frame. */
#include "viewer/drive_bumper.h"
#include "test.h"

#include <string.h>

static void test_keys_turn_over_on_a_press(void)
{
    DriveBumper b;
    drive_bumper_start(&b, 1, 0);
    CHECK_EQ_INT(b.mirror, 1);
    CHECK_EQ_INT(b.radar, 0);
    drive_bumper_keys(&b, 1, 0);   /* F1 down: the mirror off */
    CHECK_EQ_INT(b.mirror, 0);
    drive_bumper_keys(&b, 1, 0);   /* held: nothing more */
    CHECK_EQ_INT(b.mirror, 0);
    drive_bumper_keys(&b, 0, 1);   /* F1 up, F2 down: the radar on */
    CHECK_EQ_INT(b.mirror, 0);
    CHECK_EQ_INT(b.radar, 1);
    drive_bumper_keys(&b, 1, 1);   /* F1 again (F2 held) */
    CHECK_EQ_INT(b.mirror, 1);
    CHECK_EQ_INT(b.radar, 1);
    drive_bumper_start(&b, 0, 0);  /* the next race starts from EXTRAS again */
    CHECK_EQ_INT(b.mirror + b.radar, 0);
}

static void test_only_the_bumper_view(void)
{
    CHECK(drive_bumper_view(DRIVE_BUMPER_KIND));
    CHECK(!drive_bumper_view(0));
    CHECK(!drive_bumper_view(5));
}

/* the mirror against the right edge under the scoreboard, the frame's margin inside the screen */
static void test_mirror_place(void)
{
    const ScreenPoint p640 = drive_bumper_mirror_at(640, 96);
    CHECK_EQ_INT(p640.x, 640 - 96 - DRIVE_BUMPER_MARGIN);
    CHECK_EQ_INT(p640.y, HUD_BAR_HEIGHT + DRIVE_BUMPER_MARGIN);
    const ScreenPoint p1120 = drive_bumper_mirror_at(1120, 144);
    CHECK_EQ_INT(p1120.x, 1120 - 144 - DRIVE_BUMPER_MARGIN);
    CHECK_EQ_INT(p1120.y, HUD_BAR_HEIGHT + DRIVE_BUMPER_MARGIN);
}

/* the picture copied at its place, a frame of DRIVE_BUMPER_FRAME colour round it, the rest untouched */
static void test_blit(void)
{
    enum { W = 40, H = 30, PW = 6, PH = 4 };
    static uint8_t screen[W * H];
    uint8_t picture[PW * PH];
    for (int i = 0; i < PW * PH; i++) picture[i] = (uint8_t)(10 + i);
    memset(screen, 7, sizeof screen);
    drive_bumper_blit(screen, W, H, picture, PW, PH, (ScreenPoint){ 20, 10 });
    CHECK_EQ_INT(screen[10 * W + 20], 10);
    CHECK_EQ_INT(screen[13 * W + 25], 10 + PW * PH - 1);
    CHECK_EQ_INT(screen[9 * W + 20], DRIVE_BUMPER_FRAME);    /* above */
    CHECK_EQ_INT(screen[14 * W + 26], DRIVE_BUMPER_FRAME);   /* below right corner */
    CHECK_EQ_INT(screen[12 * W + 19], DRIVE_BUMPER_FRAME);   /* left */
    CHECK_EQ_INT(screen[8 * W + 20], 7);                     /* beyond the frame */
    int changed = 0;
    for (int i = 0; i < W * H; i++) changed += screen[i] != 7;
    CHECK_EQ_INT(changed, (PW + 2) * (PH + 2));
    /* off the screen's edge: cut, nothing written outside */
    memset(screen, 7, sizeof screen);
    drive_bumper_blit(screen, W, H, picture, PW, PH, (ScreenPoint){ W - 3, H - 2 });
    CHECK_EQ_INT(screen[(H - 2) * W + W - 3], 10);
}

/* an 8-bit FSH shape (type 0x7b) w x h of one colour */
static void make_shape(uint8_t *shape, int w, int h, uint8_t colour)
{
    memset(shape, 0, 0x10);
    shape[0] = 0x7b, shape[4] = (uint8_t)w, shape[6] = (uint8_t)h;
    memset(shape + 0x10, colour, (size_t)w * h);
}

enum { BIG_LIT = 21, BIG_DARK = 22, SMALL_LIT = 23, SMALL_DARK = 24, BIG_W = 8, BIG_H = 6, SMALL_W = 4, SMALL_H = 4,
       SW = 80, SH = 40, BACK = 9, LIGHT = 31, FACE = 32, DARK = 33 };

/* the colour of the big light and of each small one as the detector at `at` shows them */
static void read_radar(const uint8_t *screen, ScreenPoint at, int *big, int small[DRIVE_BUMPER_RADAR_SMALL])
{
    const int row = at.y + DRIVE_BUMPER_RADAR_PAD + 2;   /* a line every shape covers */
    int x = at.x + DRIVE_BUMPER_RADAR_PAD;
    *big = screen[row * SW + x];
    x += BIG_W;
    for (int i = 0; i < DRIVE_BUMPER_RADAR_SMALL; i++) {
        x += DRIVE_BUMPER_RADAR_GAP;
        small[i] = screen[row * SW + x];
        x += SMALL_W;
    }
}

static void test_radar(void)
{
    uint8_t big_lit[0x10 + BIG_W * BIG_H], big_dark[0x10 + BIG_W * BIG_H];
    uint8_t small_lit[0x10 + SMALL_W * SMALL_H], small_dark[0x10 + SMALL_W * SMALL_H];
    make_shape(big_lit, BIG_W, BIG_H, BIG_LIT), make_shape(big_dark, BIG_W, BIG_H, BIG_DARK);
    make_shape(small_lit, SMALL_W, SMALL_H, SMALL_LIT), make_shape(small_dark, SMALL_W, SMALL_H, SMALL_DARK);
    const DriveBumperRadarArt art = { big_lit, big_dark, small_lit, small_dark, LIGHT, FACE, DARK };
    static uint8_t screen[SW * SH];
    const ScreenPoint at = { 6, 5 };
    int big, small[DRIVE_BUMPER_RADAR_SMALL];

    const ScreenPoint size = drive_bumper_radar_size(&art);
    CHECK_EQ_INT(size.x, BIG_W + 5 * (DRIVE_BUMPER_RADAR_GAP + SMALL_W) + 2 * DRIVE_BUMPER_RADAR_PAD);
    CHECK_EQ_INT(size.y, BIG_H + 2 * DRIVE_BUMPER_RADAR_PAD);
    CHECK_EQ_INT(DRIVE_BUMPER_RADAR_PAD, 7);
    const DriveBumperRadarArt none = { 0 };
    CHECK_EQ_INT(drive_bumper_radar_size(&none).x, 0);
    CHECK_EQ_INT(drive_bumper_radar_at(1120).x, DRIVE_BUMPER_MARGIN);   /* the left end, under the scoreboard */
    CHECK_EQ_INT(drive_bumper_radar_at(640).y, HUD_BAR_HEIGHT + DRIVE_BUMPER_MARGIN);

    /* nothing near: every light dark, the big one too whatever the police lights */
    memset(screen, BACK, sizeof screen);
    drive_bumper_radar(screen, SW, SH, &art, 0, 1, at);
    read_radar(screen, at, &big, small);
    CHECK_EQ_INT(big, BIG_DARK);
    for (int i = 0; i < 5; i++) CHECK_EQ_INT(small[i], SMALL_DARK);
    /* the case: a light edge on its top and left, a dark one on its bottom and right, the face between */
    CHECK_EQ_INT(screen[at.y * SW + at.x + 10], LIGHT);
    CHECK_EQ_INT(screen[(at.y + 10) * SW + at.x], LIGHT);
    CHECK_EQ_INT(screen[(at.y + size.y - 1) * SW + at.x + 10], DARK);
    CHECK_EQ_INT(screen[(at.y + 10) * SW + at.x + size.x - 1], DARK);
    CHECK_EQ_INT(screen[(at.y + 1) * SW + at.x + 10], FACE);
    CHECK_EQ_INT(screen[(at.y + 10) * SW + at.x + 1], FACE);
    CHECK_EQ_INT(screen[(at.y + size.y - 2) * SW + at.x + 10], FACE);
    /* the sunken well inside it, the other way round, black round the lights */
    const int w = DRIVE_BUMPER_RADAR_BEVEL + DRIVE_BUMPER_RADAR_FACE;
    CHECK_EQ_INT(screen[(at.y + w) * SW + at.x + 10], DARK);
    CHECK_EQ_INT(screen[(at.y + 10) * SW + at.x + w], DARK);
    CHECK_EQ_INT(screen[(at.y + size.y - 1 - w) * SW + at.x + 10], LIGHT);
    CHECK_EQ_INT(screen[(at.y + 10) * SW + at.x + size.x - 1 - w], LIGHT);
    CHECK_EQ_INT(screen[(at.y + w + 1) * SW + at.x + 10], DRIVE_BUMPER_FRAME);
    CHECK_EQ_INT(screen[(at.y + size.y) * SW + at.x], BACK);                     /* and nothing past it */
    CHECK_EQ_INT(screen[at.y * SW + at.x + size.x], BACK);

    /* three lit from the first, the big light on with the toggle, off without */
    drive_bumper_radar(screen, SW, SH, &art, 3, 1, at);
    read_radar(screen, at, &big, small);
    CHECK_EQ_INT(big, BIG_LIT);
    for (int i = 0; i < 5; i++) CHECK_EQ_INT(small[i], i < 3 ? SMALL_LIT : SMALL_DARK);
    drive_bumper_radar(screen, SW, SH, &art, 3, 0, at);
    read_radar(screen, at, &big, small);
    CHECK_EQ_INT(big, BIG_DARK);

    /* more than five: all five; a count below 0 lights none (cockpit_lights' loops) */
    drive_bumper_radar(screen, SW, SH, &art, 9, 1, at);
    read_radar(screen, at, &big, small);
    for (int i = 0; i < 5; i++) CHECK_EQ_INT(small[i], SMALL_LIT);
    drive_bumper_radar(screen, SW, SH, &art, -2, 1, at);
    read_radar(screen, at, &big, small);
    for (int i = 0; i < 5; i++) CHECK_EQ_INT(small[i], SMALL_DARK);

    /* without its art nothing is drawn; off the screen's edge it is cut */
    memset(screen, BACK, sizeof screen);
    drive_bumper_radar(screen, SW, SH, &none, 3, 1, at);
    for (int i = 0; i < SW * SH; i++) CHECK_EQ_INT(screen[i], BACK);
    drive_bumper_radar(screen, SW, SH, &art, 3, 1, (ScreenPoint){ SW - 5, SH - 3 });
    CHECK_EQ_INT(screen[(SH - 3) * SW + SW - 5], LIGHT);   /* its corner */
    CHECK_EQ_INT(screen[(SH - 1) * SW + SW - 1], FACE);    /* its face as far as the screen goes */
}

int main(void)
{
    test_radar();
    test_keys_turn_over_on_a_press();
    test_only_the_bumper_view();
    test_mirror_place();
    test_blit();
    return TEST_REPORT();
}

/* port: the bumper view's extras (drive_bumper.h): EXTRAS' Bumper Mirror and Bumper Radar as a race's start, F7 / F8
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
    drive_bumper_keys(&b, 1, 0);   /* F7 down: the mirror off */
    CHECK_EQ_INT(b.mirror, 0);
    drive_bumper_keys(&b, 1, 0);   /* held: nothing more */
    CHECK_EQ_INT(b.mirror, 0);
    drive_bumper_keys(&b, 0, 1);   /* F7 up, F8 down: the radar on */
    CHECK_EQ_INT(b.mirror, 0);
    CHECK_EQ_INT(b.radar, 1);
    drive_bumper_keys(&b, 1, 1);   /* F7 again (F8 held) */
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

int main(void)
{
    test_keys_turn_over_on_a_press();
    test_only_the_bumper_view();
    test_mirror_place();
    test_blit();
    return TEST_REPORT();
}

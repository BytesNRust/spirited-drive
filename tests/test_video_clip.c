/* 0047383c's box on the blits (video_set_clip), opened again by 00473680 (video_select_target). */
#include "game/platform/platform.h"
#include "game/sys/video.h"
#include "test.h"

void platform_present(const uint8_t *pixels, const PlatformColor palette[256]) { (void)pixels, (void)palette; }
void platform_wait_vblank(void) {}
void platform_framebuffer_size(int *width, int *height) { *width = 640, *height = 480; }

enum { W = 16, H = 8 };

static void blit_square(uint8_t colour, int x, int y)
{
    uint8_t px[4 * 4];
    memset(px, colour, sizeof px);
    const FshEntry e = { .width = 4, .height = 4, .payload = px, .payload_size = sizeof px };
    video_blit(&e, x, y);
}

static void test_clip_keeps_blits_inside(void)
{
    uint8_t pic[W * H];
    memset(pic, 0, sizeof pic);
    video_select_target(pic, W, H);
    video_set_clip(6, 8, 0, 3);
    blit_square(9, 5, 1);  /* covers x 5..8, y 1..4: only x 6..7, y 1..2 kept */
    int drawn = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (pic[y * W + x]) {
                drawn++;
                CHECK(x >= 6 && x < 8 && y >= 1 && y < 3);
            }
    CHECK_EQ_INT(drawn, 4);
}

static void test_clip_cut_to_target(void)
{
    uint8_t pic[W * H];
    memset(pic, 0, sizeof pic);
    video_select_target(pic, W, H);
    video_set_clip(-5, 100, -5, 100);  /* 0047383c: below 0 to 0, past the target to its size */
    blit_square(3, W - 2, H - 2);
    CHECK_EQ_INT(pic[(H - 1) * W + W - 1], 3);
    CHECK_EQ_INT(pic[(H - 3) * W + W - 1], 0);
    uint8_t guarded[W * H + 2 * W];  /* the picture with a row on each side: nothing written outside it */
    memset(guarded, 0, sizeof guarded);
    video_select_target(guarded + W, W, H);
    video_set_clip(-5, 100, -5, 100);
    blit_square(4, -2, -2);
    CHECK_EQ_INT(guarded[W], 4);
    CHECK_EQ_INT(guarded[W + W + 1], 4);
    for (int i = 0; i < W; i++) CHECK_EQ_INT(guarded[i], 0);
    CHECK_EQ_INT(guarded[W + W - 1], 0);
    CHECK_EQ_INT(guarded[W + W - 2], 0);
}

static void test_select_opens_the_box(void)
{
    uint8_t pic[W * H];
    memset(pic, 0, sizeof pic);
    video_select_target(pic, W, H);
    video_set_clip(0, 1, 0, 1);
    video_select_target(pic, W, H);
    blit_square(7, 10, 3);
    CHECK_EQ_INT(pic[3 * W + 10], 7);
    CHECK_EQ_INT(pic[6 * W + 13], 7);
}

int main(void)
{
    if (!video_init()) return 1;
    test_clip_keeps_blits_inside();
    test_clip_cut_to_target();
    test_select_opens_the_box();
    video_select_target(NULL, 0, 0);
    video_shutdown();
    return TEST_REPORT();
}

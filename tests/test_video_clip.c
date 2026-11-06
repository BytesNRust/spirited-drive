/* 0047383c's box on the blits (video_set_clip), opened again by 00473680 (video_select_target). */
#include "game/platform/platform.h"
#include "game/sys/palette.h"
#include "game/sys/video.h"
#include "test.h"

void platform_present(const uint8_t *pixels, const PlatformColor palette[256]) { (void)pixels, (void)palette; }
static int vblanks;   /* the frontend's frames waited */
void platform_wait_vblank(void) {}
void platform_wait_frontend_frame(void) { vblanks++; }
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

/* port: whether the display is already dark, so a screen that has to fade its caller's picture out before putting
 * its own page up can skip the fade when the display is already black. A fade scales the palette, and
 * palette_fade_out_step(PALETTE_FADE_STEPS - 1) is 0, so a fade_out over the whole palette leaves it entirely zero
 * and the seventeen vblanks after it change nothing. Fading again is dead time the player sits through.
 *
 * The answer is the video layer's own, not a caller's flag: anything that puts light on the display clears it. */
static void lit_palette(void)
{
    uint8_t rgb[PALETTE_BYTES];
    for (int i = 0; i < PALETTE_COLORS; i++)
        rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = (uint8_t)(40 + i);
    video_set_palette(0, PALETTE_COLORS, rgb);
}

static void test_a_full_fade_out_leaves_the_display_dark(void)
{
    lit_palette();
    CHECK_EQ_INT(video_display_is_dark(), 0);   /* something is showing */

    video_fade_out(0, PALETTE_COLORS);
    CHECK_EQ_INT(video_display_is_dark(), 1);

    /* and it stays dark across frames that only present what is already there */
    video_flip();
    CHECK_EQ_INT(video_display_is_dark(), 1);
}

/* anything that puts light on the display makes it lit again, or the next screen would skip a fade it needs */
static void test_light_on_the_display_clears_it(void)
{
    video_fade_out(0, PALETTE_COLORS);
    CHECK_EQ_INT(video_display_is_dark(), 1);

    lit_palette();                               /* what palette_set does */
    CHECK_EQ_INT(video_display_is_dark(), 0);

    video_fade_out(0, PALETTE_COLORS);
    uint8_t target[PALETTE_BYTES];
    memset(target, 0xff, sizeof target);
    video_fade_in(0, PALETTE_COLORS, target);    /* what a screen's own fade_in does */
    CHECK_EQ_INT(video_display_is_dark(), 0);
}

/* a fade that leaves part of the palette lit is not a dark display: only the whole range counts */
static void test_a_partial_fade_out_is_not_dark(void)
{
    lit_palette();
    video_fade_out(0, PALETTE_COLORS / 2);       /* half the entries only */
    CHECK_EQ_INT(video_display_is_dark(), 0);
    video_fade_out(0, PALETTE_COLORS);            /* and the other half after */
    CHECK_EQ_INT(video_display_is_dark(), 1);
}

/* the short fades take PALETTE_FADE_QUICK_STEPS frames, to black and back, and leave the display dark/lit as the long ones do */
static void test_the_quick_fades(void)
{
    lit_palette();
    vblanks = 0;
    video_fade_out_quick(0, PALETTE_COLORS);
    CHECK_EQ_INT(vblanks, PALETTE_FADE_QUICK_STEPS);
    CHECK_EQ_INT(video_display_is_dark(), 1);
    uint8_t target[PALETTE_BYTES];
    memset(target, 63, sizeof target);
    vblanks = 0;
    video_fade_in_quick(0, PALETTE_COLORS, target);
    CHECK_EQ_INT(vblanks, PALETTE_FADE_QUICK_STEPS);
    CHECK_EQ_INT(video_display_is_dark(), 0);
}

int main(void)
{
    if (!video_init()) return 1;
    test_clip_keeps_blits_inside();
    test_clip_cut_to_target();
    test_select_opens_the_box();
    test_a_full_fade_out_leaves_the_display_dark();
    test_light_on_the_display_clears_it();
    test_a_partial_fade_out_is_not_dark();
    test_the_quick_fades();
    video_select_target(NULL, 0, 0);
    video_shutdown();
    return TEST_REPORT();
}

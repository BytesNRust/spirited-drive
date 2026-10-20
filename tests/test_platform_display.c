/* The platform's framebuffer width, refresh period and display options from its config. SDL's dummy video driver:
 * no window is shown. */
#include "platform/platform.h"
#include "test.h"

#include <stdlib.h>

static bool present_blank(int width, int height)
{
    uint8_t *pixels = calloc((size_t)width * (size_t)height, 1);
    PlatformColor palette[256] = { { 0 } };
    if (pixels == NULL) return false;
    platform_present(pixels, palette);
    free(pixels);
    return true;
}

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    int w = 0, h = 0;

    PlatformConfig wide = { 854, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&wide));
    platform_framebuffer_size(&w, &h);
    CHECK_EQ_INT(w, 854);
    CHECK_EQ_INT(h, 480);
    CHECK_EQ_INT(platform_refresh_period_ns(), 16666666);
    CHECK(present_blank(854, 480));
    platform_shutdown();

    PlatformConfig fast = { 1120, 480, "test", 1, 0, 144 };
    CHECK(platform_init(&fast));
    platform_framebuffer_size(&w, &h);
    CHECK_EQ_INT(w, 1120);
    CHECK_EQ_INT(h, 480);
    CHECK_EQ_INT(platform_refresh_period_ns(), 6944444);
    CHECK(present_blank(1120, 480));
    platform_shutdown();

    PlatformConfig original = { 640, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&original));
    platform_framebuffer_size(&w, &h);
    CHECK_EQ_INT(w, 640);
    CHECK_EQ_INT(platform_refresh_period_ns(), 16666666);
    platform_shutdown();
    return TEST_REPORT();
}

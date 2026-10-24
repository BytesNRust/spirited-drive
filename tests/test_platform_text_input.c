/* The window's text input is on for the menus' name boxes and off while a race is driven: with it on, macOS routes
 * every letter key through its input client, and a letter held down (the horn is C) opens the accent chooser, which
 * swallows the key events of everything held with it -- late and lost key presses, a steering key that is not let go.
 * SDL's dummy video driver: no window is shown. */
#include "platform/platform.h"
#include "test.h"

#include <stdlib.h>

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    PlatformConfig config = { 640, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    CHECK(platform_text_input_active());   /* the menus' state from the start */
    platform_text_input(false);
    CHECK(!platform_text_input_active());
    platform_text_input(false);            /* twice is the same */
    CHECK(!platform_text_input_active());
    platform_text_input(true);
    CHECK(platform_text_input_active());
    platform_shutdown();
    CHECK(!platform_text_input_active());   /* no window: none */
    return TEST_REPORT();
}

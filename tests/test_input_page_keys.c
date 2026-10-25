/* The page keys on the keyboard path: a scripted PGDN is the PageDown (0x5100) input_get_key reports, and it
 * is reported only while the page keys are on (input_page_keys). The script is the port's own, so the name has
 * to be one the key table knows; the screen's own side of it -- that 0x5100 turns the page -- is
 * test_extras_menu's. */
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/video.h"
#include "platform/platform.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* input.c's pointer and video layers: the test drives the keyboard, not the pointer, and never closes the
 * window (video_shutdown is only reached when it does) */
void pointer_sync_from_os(void) {}
void video_shutdown(void) {}

/* The script holds each key for 100 ms (SCRIPT_HOLD_MS) and lets it go after that, so the entries are 100 ms
 * apart: a read keeps an entry alive for its window and the next one takes over, and the key is down for the
 * whole of the test. 64 entries is the script's own maximum. */
#define SCRIPT_ENTRIES 64
#define SCRIPT_HOLD_MS 100

int main(void)
{
    char script[1024] = "";
    for (int i = 0; i < SCRIPT_ENTRIES; i++) {
        const int n = snprintf(script + strlen(script), sizeof script - strlen(script), "%s%d:PGDN", i ? "," : "",
                               i * SCRIPT_HOLD_MS);
        if (n < 0 || (size_t)n >= sizeof script - strlen(script)) return 1;
    }

    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("TNFS_INPUT_SCRIPT", script, 1);

    const PlatformConfig config = { 640, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&config));

    /* the name the script used is one the key table knows, and its code is the one page_key reads */
    const int pgdn = platform_script_key("PGDN");
    CHECK_EQ_INT(pgdn, 0x51);

    /* wait for the script's key to go down: it is held from its first entry on */
    int held = 0;
    for (int i = 0; i < 3000 && !held; i++) {
        held = platform_key_down(pgdn);
        if (!held) platform_sleep_ms(1);
    }
    CHECK(held);

    /* the page keys on: the held PageDown is what the game is told */
    input_page_keys(1);
    CHECK_EQ_INT(input_get_key(), 0x5100);

    /* the page keys off: the same held key is nothing, as in the exe */
    input_page_keys(0);
    CHECK_EQ_INT(input_get_key(), 0);

    /* and the key was still held when the answer above was 0: that answer was the gate and not a key that had
     * let go. Without this the check above would also pass on a key the script had dropped. */
    CHECK(platform_key_down(pgdn));

    platform_shutdown();
    return TEST_REPORT();
}

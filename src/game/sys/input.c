#include "game/sys/input.h"

#include "game/sys/video.h"
#include "platform/platform.h"

#include <stdlib.h>

static int translate(PlatformKey key)
{
    switch (key) {
    case PLATFORM_KEY_ENTER: return KEY_ENTER;
    case PLATFORM_KEY_ESCAPE: return KEY_ESCAPE;
    case PLATFORM_KEY_SPACE: return KEY_SPACE;
    case PLATFORM_KEY_UP: return KEY_UP;
    case PLATFORM_KEY_DOWN: return KEY_DOWN;
    case PLATFORM_KEY_LEFT: return KEY_LEFT;
    case PLATFORM_KEY_RIGHT: return KEY_RIGHT;
    default: return KEY_NONE;
    }
}

int input_get_key(void)
{
    if (!platform_poll_events()) {
        /* The original only quits through its menus; closing the window here
         * ends the process the way a fatal error (00477a40) would. */
        video_shutdown();
        platform_shutdown();
        exit(EXIT_SUCCESS);
    }
    /* The game's wait loops spin on this call; yield so the OS (and the
     * browser, under ASYNCIFY) keeps running. */
    platform_sleep_ms(1);
    return translate(platform_next_key());
}

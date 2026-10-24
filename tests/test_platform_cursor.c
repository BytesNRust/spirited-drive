/* The system's mouse cursor is hidden over the game's window (the menus draw their own pointer, a race has none), in
 * windowed and fullscreen mode, and is back when the window loses the focus and when the game ends. SDL's dummy video
 * driver: no window is shown; the focus events are pushed as the window system would send them. */
#include "platform/platform.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

static void focus(SDL_EventType type)
{
    SDL_Event event = { 0 };
    event.type = type;
    CHECK(SDL_PushEvent(&event));
    CHECK(platform_poll_events());
}

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    CHECK(!platform_cursor_hidden());                /* no window: nothing hidden */
    PlatformConfig config = { 640, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    CHECK(platform_cursor_hidden());                 /* windowed */
    focus(SDL_EVENT_WINDOW_FOCUS_LOST);
    CHECK(!platform_cursor_hidden());                /* the window lost the focus: the system's is back */
    focus(SDL_EVENT_WINDOW_FOCUS_GAINED);
    CHECK(platform_cursor_hidden());
    config.fullscreen = 1;
    CHECK(platform_reconfigure(&config));
    CHECK(platform_cursor_hidden());                 /* fullscreen */
    config.fullscreen = 0;
    CHECK(platform_reconfigure(&config));
    CHECK(platform_cursor_hidden());                 /* and windowed again */
    platform_shutdown();
    CHECK(!platform_cursor_hidden());
    CHECK(SDL_Init(SDL_INIT_VIDEO));                 /* the game's end left the cursor shown for whoever runs on */
    CHECK(SDL_CursorVisible());
    SDL_Quit();
    return TEST_REPORT();
}

/* A touchpad tap (Windows tap-to-click) whose button down and up come in one poll of the events is still a click: the press is latched until a read has seen it, as the keys' latch does.
 * SDL's dummy video driver: the events are pushed as the window system would send them. */
#include "platform/platform.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

static void button(bool down, Uint8 which)
{
    SDL_Event event = { 0 };
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = which;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event));
}

static unsigned read_buttons(void)
{
    int x, y;
    unsigned b = 0;
    (void)platform_mouse(&x, &y, &b);
    return b;
}

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    PlatformConfig config = { 640, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)read_buttons(), 0);
    /* down and up in one poll: the state at the end says up, the click is not lost */
    button(true, SDL_BUTTON_LEFT);
    button(false, SDL_BUTTON_LEFT);
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)(read_buttons() & PLATFORM_MOUSE_LEFT), PLATFORM_MOUSE_LEFT);   /* seen as a press */
    CHECK_EQ_INT((int)(read_buttons() & PLATFORM_MOUSE_LEFT), PLATFORM_MOUSE_LEFT);   /* by every reader before the next poll */
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)read_buttons(), 0);                                              /* and one click only: gone at the next poll */
    /* a tap nobody read before the next poll stays until it is read */
    button(true, SDL_BUTTON_RIGHT);
    button(false, SDL_BUTTON_RIGHT);
    CHECK(platform_poll_events());
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)(read_buttons() & PLATFORM_MOUSE_RIGHT), PLATFORM_MOUSE_RIGHT);
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)read_buttons(), 0);
    /* a button held across polls reads down while held and up once let go */
    button(true, SDL_BUTTON_LEFT);
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)(read_buttons() & PLATFORM_MOUSE_LEFT), PLATFORM_MOUSE_LEFT);
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)(read_buttons() & PLATFORM_MOUSE_LEFT), PLATFORM_MOUSE_LEFT);
    button(false, SDL_BUTTON_LEFT);
    CHECK(platform_poll_events());
    CHECK_EQ_INT((int)read_buttons(), 0);
    platform_shutdown();
    return TEST_REPORT();
}

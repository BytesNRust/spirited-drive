/* A quick tap (button down and up within one poll of the events: a touchpad's tap-to-click, or a real click during a slow frame) reaches the menus' reads at the menus' own rate.
 * A menu's loop pumps the input twice a turn (input_get_key, before and after the pointer is read) and reads the buttons once: a tap that came in at the second pump
 * must still be there at the next turn's read. SDL's dummy video driver: the events are pushed as the window system would send them. */
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "platform/platform.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

/* what input.c and pointer.c call and this test does not need: the timers, the video, the pads */
#include "game/sys/timer.h"
#include "game/sys/video.h"
void timer_set_deadline(int ticks) { (void)ticks; }
int timer_deadline_passed(void) { return 1; }
void video_shutdown(void) {}
void video_set_overlay(const FshEntry *shape, int x, int y) { (void)shape, (void)x, (void)y; }
unsigned platform_joy_shoulders(int device) { return (void)device, 0; }

static void button(bool down, Uint8 which)
{
    SDL_Event event = { 0 };
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = which;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event));
}

static void tap(Uint8 which)
{
    button(true, which);
    button(false, which);
}

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    PlatformConfig config = { 640, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    pointer_start();
    pointer_mark();
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    /* the tap comes in at the turn's first pump, the read follows at once: the case the first latch served */
    tap(SDL_BUTTON_LEFT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);   /* one click only */
    /* the tap comes in at the turn's second pump (after the read), and the turn's slow work follows: the next turn's read still finds it */
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    tap(SDL_BUTTON_LEFT);
    input_pump();                              /* the second pump of the turn */
    SDL_Delay(900);                            /* the draw of a very slow frame: no pump in it, so nothing wears the tap, whatever the clock does */
    input_pump();                              /* the next turn's first pump */
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    /* the right button too */
    tap(SDL_BUTTON_RIGHT);
    input_pump();
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_BACK);
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_BACK);   /* every reader of the turn sees it */
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);                      /* and it is gone at the next pump */
    /* a button held over turns reads down on every read, then up once let go: no second click out of one press */
    button(true, SDL_BUTTON_LEFT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    button(false, SDL_BUTTON_LEFT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
        /* two taps in two pumps in a row (each down and up inside its poll): the state reads LEFT both times, and the second is a press too */
    tap(SDL_BUTTON_LEFT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    tap(SDL_BUTTON_LEFT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    tap(SDL_BUTTON_LEFT);
    input_pump();
    tap(SDL_BUTTON_LEFT);
    input_pump();   /* both before a read: one turn, one click */
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    /* the skip screens read with pointer_buttons_unseen after a sync, no pointer_read_buttons: a click that skips one does not skip the next */
    for (int screen = 0; screen < 3; screen++) {
        for (int turn = 0; turn < 3; turn++) {
            input_pump();
            pointer_sync_from_os();
            CHECK_EQ_INT(pointer_buttons_unseen(), 0);   /* nothing is held over from the click before */
        }
        tap(SDL_BUTTON_LEFT);
        input_pump();
        pointer_sync_from_os();
        CHECK_EQ_INT(pointer_buttons_unseen(), POINTER_BUTTON_SELECT);   /* the click skips this one */
    }
    /* a tap that many pumps went by without a read having it (a fade, a movie) is not a click on a later screen: counted in pumps, so a loaded machine cannot change the answer */
    tap(SDL_BUTTON_LEFT);
    input_pump();
    for (int pumps = 0; pumps < 100; pumps++) input_pump();      /* a click's wait pumps about this many: still there */
    CHECK_EQ_INT(pointer_read_buttons(), POINTER_BUTTON_SELECT);
    input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    tap(SDL_BUTTON_LEFT);
    input_pump();
    for (int pumps = 0; pumps < 450; pumps++) input_pump();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    /* the next screen starts clean: a tap before pointer_start is gone */
    tap(SDL_BUTTON_LEFT);
    input_pump();
    pointer_start();
    CHECK_EQ_INT(pointer_read_buttons(), 0);
    platform_shutdown();
    return TEST_REPORT();
}

/* Our own screens' hold: they stay their time on the timer, a press moves them on only when it began on the screen (a key or a button held from the
 * screen before, or a click read twice, counts once), and what a press does is its source's. */
#include "game/frontend/screen_hold.h"
#include "test.h"

static const HoldSource UP[HOLD_SOURCES] = { { 0, HOLD_STAY }, { 0, HOLD_STAY }, { 0, HOLD_STAY } };

static void it_stays_its_time_then_goes_on(void)
{
    ScreenHold h = screen_hold_start(1000, 500);
    CHECK_EQ_INT(screen_hold_step(&h, 1000, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 1499, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 1500, UP), HOLD_NEXT);
}

static void the_time_wraps_with_the_timer(void)
{
    ScreenHold h = screen_hold_start(0xFFFFFF00u, 0x200);
    CHECK_EQ_INT(screen_hold_step(&h, 0xFFFFFFFFu, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 0x80, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 0x100, UP), HOLD_NEXT);
}

/* the owner's bug: a click went two pages, the button seen down again on the next page before its release was pumped */
static void a_button_held_from_before_counts_only_after_it_is_let_go(void)
{
    ScreenHold h = screen_hold_start(0, 2000);
    const HoldSource held[HOLD_SOURCES] = { { 0, HOLD_STAY }, { 1, HOLD_NEXT }, { 0, HOLD_STAY } };
    CHECK_EQ_INT(screen_hold_step(&h, 1, held), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 2, held), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 3, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 4, held), HOLD_NEXT);
}

static void a_key_held_from_before_counts_only_after_it_is_let_go(void)
{
    ScreenHold h = screen_hold_start(0, 2000);
    const HoldSource space[HOLD_SOURCES] = { { 1, HOLD_NEXT }, { 0, HOLD_STAY }, { 0, HOLD_STAY } };
    CHECK_EQ_INT(screen_hold_step(&h, 1, space), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 2, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 3, space), HOLD_NEXT);
}

static void each_source_is_armed_on_its_own(void)
{
    ScreenHold h = screen_hold_start(0, 2000);
    const HoldSource key_held[HOLD_SOURCES] = { { 1, HOLD_NEXT }, { 0, HOLD_STAY }, { 0, HOLD_STAY } };
    const HoldSource key_and_pad[HOLD_SOURCES] = { { 1, HOLD_NEXT }, { 0, HOLD_STAY }, { 1, HOLD_NEXT } };
    CHECK_EQ_INT(screen_hold_step(&h, 1, key_held), HOLD_STAY);       /* the pad is seen up here, the key is not */
    CHECK_EQ_INT(screen_hold_step(&h, 2, key_and_pad), HOLD_NEXT);    /* the pad, pressed on this screen */
}

static void a_press_does_what_its_source_says(void)
{
    ScreenHold h = screen_hold_start(0, 2000);
    const HoldSource escape[HOLD_SOURCES] = { { 1, HOLD_LEAVE }, { 0, HOLD_STAY }, { 0, HOLD_STAY } };
    const HoldSource arrow[HOLD_SOURCES] = { { 1, HOLD_STAY }, { 0, HOLD_STAY }, { 0, HOLD_STAY } };
    CHECK_EQ_INT(screen_hold_step(&h, 1, arrow), HOLD_STAY);   /* a key the screen does not take, held from before: it does nothing */
    CHECK_EQ_INT(screen_hold_step(&h, 2, escape), HOLD_LEAVE); /* so the key that follows it is a press of this screen's */
    h = screen_hold_start(0, 2000);
    const HoldSource right_button[HOLD_SOURCES] = { { 0, HOLD_STAY }, { 1, HOLD_LEAVE }, { 0, HOLD_STAY } };
    CHECK_EQ_INT(screen_hold_step(&h, 1, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&h, 2, right_button), HOLD_LEAVE);
}

static void a_new_screen_forgets_the_last_ones_presses(void)
{
    const HoldSource held[HOLD_SOURCES] = { { 0, HOLD_STAY }, { 1, HOLD_NEXT }, { 0, HOLD_STAY } };
    ScreenHold a = screen_hold_start(0, 2000);
    CHECK_EQ_INT(screen_hold_step(&a, 1, UP), HOLD_STAY);
    CHECK_EQ_INT(screen_hold_step(&a, 2, held), HOLD_NEXT);
    ScreenHold b = screen_hold_start(3, 2000);
    CHECK_EQ_INT(screen_hold_step(&b, 4, held), HOLD_STAY);   /* the same click, not let go yet */
}

int main(void)
{
    it_stays_its_time_then_goes_on();
    the_time_wraps_with_the_timer();
    a_button_held_from_before_counts_only_after_it_is_let_go();
    a_key_held_from_before_counts_only_after_it_is_let_go();
    each_source_is_armed_on_its_own();
    a_press_does_what_its_source_says();
    a_new_screen_forgets_the_last_ones_presses();
    return TEST_REPORT();
}

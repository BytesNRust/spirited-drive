/* The replay's controls hidden and shown by a double click on the picture (game/race/replay_bare.h). */
#include "game/race/replay_bare.h"
#include "test.h"

enum { PICTURE = 0, CONTROLS = 1 };

/* a click: the button down for one frame at `ms`, up the next */
static int32_t click(ReplayBare *b, int32_t where, uint32_t ms)
{
    const int32_t changed = replay_bare_frame(b, 1, where, ms);
    return changed | replay_bare_frame(b, 0, where, ms + 16);
}

static void double_click_hides_and_shows(void)
{
    ReplayBare b = { 0 };
    CHECK_EQ_INT(click(&b, PICTURE, 1000), 0);
    CHECK_EQ_INT(b.bare, 0);
    CHECK_EQ_INT(click(&b, PICTURE, 1300), 1);
    CHECK_EQ_INT(b.bare, 1);
    CHECK_EQ_INT(click(&b, PICTURE, 5000), 0);
    CHECK_EQ_INT(click(&b, PICTURE, 5000 + REPLAY_BARE_DOUBLE_MS), 1);
    CHECK_EQ_INT(b.bare, 0);
}

/* two clicks too far apart are two single clicks; the second can still start a double click */
static void slow_clicks_do_nothing(void)
{
    ReplayBare b = { 0 };
    click(&b, PICTURE, 1000);
    CHECK_EQ_INT(click(&b, PICTURE, 1001 + REPLAY_BARE_DOUBLE_MS), 0);
    CHECK_EQ_INT(b.bare, 0);
    CHECK_EQ_INT(click(&b, PICTURE, 1200 + REPLAY_BARE_DOUBLE_MS), 1);
    CHECK_EQ_INT(b.bare, 1);
}

/* a third click right after a double click is a new first click, not another toggle */
static void triple_click_toggles_once(void)
{
    ReplayBare b = { 0 };
    click(&b, PICTURE, 1000);
    click(&b, PICTURE, 1100);
    CHECK_EQ_INT(click(&b, PICTURE, 1200), 0);
    CHECK_EQ_INT(b.bare, 1);
}

/* clicks on the controls are the buttons': two fast ones (the scrub buttons are pressed like that) hide nothing,
 * and one of them does not pair with a click on the picture */
static void clicks_on_the_controls_do_not_count(void)
{
    ReplayBare b = { 0 };
    click(&b, CONTROLS, 1000);
    CHECK_EQ_INT(click(&b, CONTROLS, 1100), 0);
    CHECK_EQ_INT(click(&b, PICTURE, 1200), 0);
    CHECK_EQ_INT(b.bare, 0);
    click(&b, PICTURE, 2000);
    click(&b, CONTROLS, 2100);
    CHECK_EQ_INT(click(&b, PICTURE, 2200), 0);
    CHECK_EQ_INT(b.bare, 0);
}

/* bare, there is no strip: a double click where it was shows the controls again */
static void bare_every_click_counts(void)
{
    ReplayBare b = { .bare = 1 };
    click(&b, CONTROLS, 1000);
    CHECK_EQ_INT(click(&b, CONTROLS, 1200), 1);
    CHECK_EQ_INT(b.bare, 0);
}

/* a button held over many frames is one click */
static void held_button_is_one_click(void)
{
    ReplayBare b = { 0 };
    int32_t changed = 0;
    for (uint32_t ms = 1000; ms < 1300; ms += 16) changed |= replay_bare_frame(&b, 1, PICTURE, ms);
    CHECK_EQ_INT(changed, 0);
    CHECK_EQ_INT(b.bare, 0);
}

/* the clock's wrap between the two clicks */
static void clock_wraps(void)
{
    ReplayBare b = { 0 };
    click(&b, PICTURE, 0xffffff00u);
    CHECK_EQ_INT(click(&b, PICTURE, 0x00000040u), 1);
}

int main(void)
{
    double_click_hides_and_shows();
    slow_clicks_do_nothing();
    triple_click_toggles_once();
    clicks_on_the_controls_do_not_count();
    bare_every_click_counts();
    held_button_is_one_click();
    clock_wraps();
    return TEST_REPORT();
}

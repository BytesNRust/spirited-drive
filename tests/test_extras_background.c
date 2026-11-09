/* The EXTRAS screen (extras_menu.c), split from test_extras_menu.c: the clean background under it, the panel without one, the palette and
 * the styles on the way in, the fades. Same rig (extras_rig.h). */
#include "extras_rig.h"

/* port: the screen opens over the configuration screens' clean background, not over the caller's picture: asked
 * for once, put on before the dark panel shades over it. */
static void test_clean_background_is_under_the_panel(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.bgnd_asks, 1);
    CHECK_EQ_INT(r.fake.panel_after_bgnd, 1);
    CHECK_EQ_INT(r.fake.panel_before_bgnd, 0);
}

/* with no background to be had (config_bgnd_load failed) the screen still runs, over the caller's picture */
static void test_no_background_still_runs(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    r.fake.bgnd_ok = 0;
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.panel_after_bgnd, 0);
    CHECK_EQ_INT(r.fake.panel_before_bgnd, 0);
}

/* port: no flash on the way in. fade_in ramps whatever the display shows, so the opener's picture goes out first in its
 * own palette, the page is presented on the black display, and only then is this screen's palette faded in: the run
 * opens O F I. The page is whole before the flip. (Fading in before the flip ramped Game Mode's art up again first,
 * then cut to this page -- the capture showed it.) */
static void test_no_flash_on_the_way_in(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.pixels_drawn_before_fade_in, 1);
    CHECK(r.fake.event_count >= 3);
    CHECK_EQ_INT(r.fake.events[0], 'O');
    CHECK_EQ_INT(r.fake.events[1], 'F');
    CHECK_EQ_INT(r.fake.events[2], 'I');
}

/* port: and none on the way out either. Every screen that opens one of these fades the whole display out as it leaves,
 * the way 00423d28's close_screen does, so the menu the screen was opened from comes back up in its own palette
 * instead of reading this screen's. */
static void test_the_display_is_faded_out_on_the_way_out(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.fade_outs, 2);   /* the opener's picture going out, then this screen's */
    /* and after the last thing drawn, not before it: a fade_out earlier would blank the screen while it was up */
    CHECK(r.fake.event_count >= 2);
    CHECK_EQ_INT(r.fake.events[r.fake.event_count - 1], 'O');
}

/* port: the letters are drawn in a ramp matched to the palette on the display, so the styles can only be rebuilt once
 * this screen's own palette has gone on. The old order asked for them as the screen opened -- before the fade -- which
 * built them from the caller's palette: Options' rows came out in the exe's colours over the clean background, and on
 * the way out the rebuild took that same palette with it. */
static void test_the_styles_are_rebuilt_after_the_palette_is_on(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.style_rebuilds, 2);      /* the screen's palette going up, and the caller's coming back */
    CHECK_EQ_INT(r.fake.styles_faded, 2);        /* both of them after the screen's palette was on the display */
}

/* port: no clean background to put a palette on (config_bgnd_load fails), so the screen stays over the caller's live
 * picture -- and then it must not fade the display out at all, or the caller comes back to a black screen. The
 * Graphics screen had exactly this defect: it had no palette of its own to fall back to and faded anyway. */
static void test_no_background_leaves_the_display_alone(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    r.fake.bgnd_ok = 0;
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.fade_ins, 0);
    CHECK_EQ_INT(r.fake.fade_outs, 0);
}

/* port: the words go straight onto the clean background, no dark panel; only over the caller's picture (no
 * background to compose) does the panel keep them readable */
static void test_a_panel_only_without_the_clean_background(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.panels, 0);
    Rig bare;
    setup(&bare);
    bare.fake.bgnd_ok = 0;
    run(&bare, keys, 2);
    CHECK(bare.fake.panels > 0);
}

/* port: the screen's own palette reaches the display. Options fades out before opening it and only fade_in puts one
 * back, so without it the whole page draws on black. Once, not per redraw: it is the same palette every time. */
static void test_the_palette_reaches_the_display(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.fade_ins, 1);
}

/* port: the styles are rebuilt for the screen's own palette as it opens, so the letters are drawn with a ramp matched
 * to the background that is showing, and the caller's are rebuilt on the way out. */
static void test_the_styles_are_built_for_the_screens_palette(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.style_rebuilds, 2);   /* the screen's palette going up, the caller's coming back */
}

int main(void)
{
    test_a_panel_only_without_the_clean_background();
    test_the_palette_reaches_the_display();
    test_no_flash_on_the_way_in();
    test_the_display_is_faded_out_on_the_way_out();
    test_no_background_leaves_the_display_alone();
    test_the_styles_are_rebuilt_after_the_palette_is_on();
    test_the_styles_are_built_for_the_screens_palette();
    test_clean_background_is_under_the_panel();
    test_no_background_still_runs();
    return TEST_REPORT();
}

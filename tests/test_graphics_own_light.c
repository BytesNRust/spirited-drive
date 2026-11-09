/* The port's own Graphics screen (graphics_own.c), split from test_graphics_own.c: the clean background and the fades in and out
 * (no flash on the way in, a dark display not faded again) and the light-effect rows. Same rig (graphics_own_rig.h). */
#include "graphics_own_rig.h"

/* 13: port: the screen must never be a black rectangle with letters on it. Options fades the display out before it
 * opens this screen, and only a palette of its own puts anything back on it -- so when the clean background cannot be
 * composed (config_bgnd_load fails, so the glue has no shape and this screen never fades a palette in), the words
 * would be drawn over a black screen. It then runs over the caller's own burnt-in Graphics screen, the picture 00423d28
 * put up, with that screen's palette -- so there is something to read. */
static void test_no_clean_background_runs_over_the_originals(void)
{
    Rig r;
    rig_init(&r);
    r.f.bgnd_ok = 0;   /* the CD screens could not be composed */
    script[0] = ESCAPE;
    run_script(&r, 1);

    CHECK_EQ_INT(r.f.bgnd_calls, 1);        /* it did ask, once, before drawing anything */
    CHECK_EQ_INT(r.f.burnt_in_reads, 1);    /* and it asked for the original's screen, once */
    CHECK_EQ_INT(r.f.fade_ins, 1);          /* so the original's own palette went on the display */
    CHECK_EQ_INT(r.f.pixels_drawn_before_fade_in, 1);   /* and only after the screen was drawn */
    CHECK(r.f.palette_sets > 0);           /* the palette came off the original's own bank */
    CHECK_EQ_INT(r.f.non_raw_modes, 0);     /* and with the mode that bank's header says, as the exe reads it */
}

/* 14: port: no flash on the way in. fade_in ramps whatever is ON the display, not the backbuffer: video_set_palette
 * presents on every step. So the order has to be the caller's picture out in its own palette, the page put up on the
 * black display, and only then this screen's palette faded in -- the run opens O F I, the order extras_menu.c's page
 * takes (fc15abb). Fading in before the flip ramped Options' own art up out of black in this screen's palette for all
 * seventeen steps, and the capture showed it: frames 402-417 climbing while Options' TRANSMISSION/ABS rows were the
 * picture, then 418 a hard cut to this page.
 *
 * This is a display with a menu on it -- nothing has faded it out -- so the fade is the one that has to happen. */
static void test_no_flash_on_the_way_in(void)
{
    Rig r;
    rig_init(&r);
    r.f.dark = 0;   /* a menu is showing: there is a picture to fade out before the page goes up */
    script[0] = ESCAPE;
    run_script(&r, 1);

    /* the page is drawn into the backbuffer before the display is touched, so the flip has something to put up */
    CHECK_EQ_INT(r.f.pixels_drawn_before_fade_in, 1);
    CHECK(r.f.event_count >= 3);
    CHECK_EQ_INT(r.f.events[0], 'O');   /* the menu's picture going out, in its own palette */
    CHECK_EQ_INT(r.f.events[1], 'F');   /* the page onto the black display */
    CHECK_EQ_INT(r.f.events[2], 'I');   /* and the page's own palette up over it */
}

/* port: the walk from Options, where Options has already faded the display out (check_options.c's choose) before it
 * opened this screen. A fade scales the palette and the last step of a fade out is 0, so that display is entirely
 * black -- fading it out again scales zeros for seventeen vblanks, and the player waits through them. One fade on
 * the way out, and the page still goes up on a black display. */
static void test_an_already_black_display_is_not_faded_again(void)
{
    Rig r;
    rig_init(&r);
    r.f.dark = 1;   /* Options faded the display out before opening this screen */
    script[0] = ESCAPE;
    run_script(&r, 1);

    CHECK_EQ_INT(r.f.fade_outs, 1);          /* only the screen's own, as it closes */
    CHECK_EQ_INT(r.f.fade_ins, 1);           /* the page's palette still goes up */
    /* the page is still put up on a black display before the palette ramps over it, or it would flash */
    CHECK(r.f.event_count >= 2);
    CHECK_EQ_INT(r.f.events[0], 'F');
    CHECK_EQ_INT(r.f.events[1], 'I');
}

/* 15: port: and none on the way out either. 00423d28's close_screen fades the whole display out, and the screen
 * leaves with nothing on it; this one left the previous screen showing in this screen's palette. Two fades leave on
 * a lit display: the opener's picture going out as the screen opens, and this screen's as it closes. */
static void test_the_display_is_faded_out_on_the_way_out(void)
{
    Rig r;
    rig_init(&r);
    r.f.dark = 0;
    script[0] = ESCAPE;
    run_script(&r, 1);

    CHECK_EQ_INT(r.f.fade_outs, 2);
    /* and the last of them after the last thing drawn, not before it: a fade_out earlier would blank the screen while
     * it was up */
    CHECK(r.f.event_count >= 2);
    CHECK_EQ_INT(r.f.events[r.f.event_count - 1], 'O');
}

/* ---- Light effects, and what is no longer on the screen ---- */

/* the row is one switch over both keys; a port.cfg that has them mixed shows Off, and a step turns both on */
static void test_light_effects_value_and_step(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    s.psx_light = 0, s.psx_shadow = 0;
    graphics_own_value(&s, GRAPHICS_OWN_LIGHT, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_step(&s, GRAPHICS_OWN_LIGHT, 1, SCREEN_21_9);
    CHECK_EQ_INT(s.psx_light, 1);
    CHECK_EQ_INT(s.psx_shadow, 1);
    graphics_own_value(&s, GRAPHICS_OWN_LIGHT, out);
    CHECK(strcmp(out, "On") == 0);
    graphics_own_step(&s, GRAPHICS_OWN_LIGHT, -1, SCREEN_21_9);
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.psx_shadow, 0);

    s.psx_light = 1, s.psx_shadow = 0;
    graphics_own_value(&s, GRAPHICS_OWN_LIGHT, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_step(&s, GRAPHICS_OWN_LIGHT, 1, SCREEN_21_9);
    CHECK_EQ_INT(s.psx_light, 1);
    CHECK_EQ_INT(s.psx_shadow, 1);
    s.psx_light = 0, s.psx_shadow = 1;
    graphics_own_value(&s, GRAPHICS_OWN_LIGHT, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_step(&s, GRAPHICS_OWN_LIGHT, 1, SCREEN_21_9);
    CHECK_EQ_INT(s.psx_light, 1);
    CHECK_EQ_INT(s.psx_shadow, 1);
}

/* the PS1 look is read when the race starts, so the pause menu dims the row */
static void test_light_effects_live_in_race(void)
{
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_LIGHT, 0, 0), 0);
}

/* a step on the row through the screen writes both keys and nothing else, and Done saves it */
static void test_light_effects_write_only_the_light_keys(void)
{
    Rig r;
    rig_init(&r);
    r.port.psx_light = 0, r.port.psx_shadow = 0;
    r.port.psx_time = PSX_TIME_DUSK;
    r.port.weather = WEATHER_RAIN;
    run_script(&r, script_walk(GRAPHICS_OWN_LIGHT, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(r.port.psx_light, 1);
    CHECK_EQ_INT(r.port.psx_shadow, 1);
    CHECK_EQ_INT(r.port.psx_time, PSX_TIME_DUSK);   /* the track screen's, kept in port.cfg */
    CHECK_EQ_INT(r.port.weather, WEATHER_RAIN);
    CHECK_EQ_INT(r.f.saves, 1);
}

int main(void)
{
    test_no_clean_background_runs_over_the_originals();
    test_no_flash_on_the_way_in();
    test_an_already_black_display_is_not_faded_again();
    test_the_display_is_faded_out_on_the_way_out();
    test_light_effects_value_and_step();
    test_light_effects_live_in_race();
    test_light_effects_write_only_the_light_keys();
    return TEST_REPORT();
}

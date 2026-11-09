/* The port's own Graphics screen (graphics_own.c): the eleven rows in the owner's order over the clean configuration
 * background, its own title, and Done/Cancel as Options has them. Driven through the fake ops with a key script, the
 * same rig test_graphics_plus.c uses.
 *
 * What is pinned here:
 *   - the row order and the exact labels, "Perspective Correction" not "Modern 3D";
 *   - every row's value text, including the view distance's fourth value "Ultra";
 *   - that each row writes into the settings the owner named (scale_integer, view_ultra, fov_adjust, format,
 *     refresh_hz, fullscreen, and the three that moved in from EXTRAS);
 *   - Done saving, Cancel putting back, Escape putting back;
 *   - the clean background being asked for exactly once and then cached, and the title drawn at the top left. */
#include "graphics_own_rig.h"

/* 1: the ten labels, in the owner's order, and nothing called "Modern 3D" */
static void test_rows_in_order(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);

    static const char *const want[] = { "Perspective Correction", "Texture Filter", "View Distance", "Distance Fog",
                                        "Light Effects", "Field Of View", "Screen Format", "Refresh Rate",
                                        "Full Screen", "Scaling", "Render Threads", "Render Scale" };
    CHECK_EQ_INT(GRAPHICS_OWN_SETTINGS, 12);
    for (int row = 0; row < GRAPHICS_OWN_SETTINGS; row++) {
        CHECK(drew(&r.f, want[row]));
        /* each label drawn at the same left edge: the labels are a column */
        for (int i = 0; i < r.f.text_count && i < TEXTS; i++)
            if (strcmp(r.f.texts[i], want[row]) == 0) CHECK_EQ_INT(r.f.text_x[i], GRAPHICS_OWN_LABEL_X);
    }
    CHECK(!drew(&r.f, "Modern 3D"));
}

/* 2: the title at the top left, drawn once per draw */
static void test_title_top_left(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.title_draws > 0);
    CHECK_EQ_INT(r.f.bgnd_calls, 1);   /* composed once and cached, not per redraw */
}

/* 3: the screen's own palette reaches the display. Options fades the screen out before it opens this one, and only
 * fade_in puts a palette on the display, so without it the whole page is drawn on black. */
static void test_palette_reaches_the_display(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.fade_ins > 0);
}

/* 4: palette_set is never handed its own destination as the source. palette_put reads src and writes the same
 * buffer, so a call that aliases shifts every colour byte left by two ONCE PER REDRAW; after a few redraws the
 * bytes have overflowed and the page is neon. This is what the capture showed. */
static void test_palette_is_not_rewritten_in_place(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.aliased_palettes == 0);
}

/* 5: the palette goes on raw. The clean background's palette is already display-ready -- config_bgnd.c reads it
 * through fsh_palette_rgb, which shifts the CD's 6-bit VGA values left by one once. Passing any other mode makes
 * palette_put shift them a second time, which is the other half of the neon. */
static void test_palette_is_put_raw(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.palette_mode_count > 0);
    CHECK(r.f.non_raw_modes == 0);
}

/* 6: DONE and CANCEL are not drawn over the background's own. The clean configuration background carries those two
 * words in its corners, so the screen drawing its own lands a second, slightly offset copy beside the first -- the
 * headless capture showed "CANCEL - CANCEL". The rows stay selectable; only the letters are the background's. */
static void test_done_and_cancel_are_not_drawn_twice(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK_EQ_INT(r.f.done_draws, 0);
    CHECK_EQ_INT(r.f.cancel_draws, 0);
}

/* port: DONE and CANCEL light up when chosen, as Options' own art (Donl / Canl) over the background's words, and
 * stay dark otherwise. Both banks are freed as the screen closes: Options' own, for that art, and the original's
 * burnt-in Graphics screen. */
static void test_done_and_cancel_light_up(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK_EQ_INT(r.f.lit_done, 0);
    CHECK_EQ_INT(r.f.lit_cancel, 0);
    CHECK_EQ_INT(r.f.frees, 2);
    Rig done;
    rig_init(&done);
    run_script(&done, script_walk(GRAPHICS_OWN_VIEW, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK(done.f.lit_done > 0);
    CHECK_EQ_INT(done.f.lit_cancel, 0);
    Rig cancel;
    rig_init(&cancel);
    int n = 0;
    for (int i = 0; i < GRAPHICS_OWN_CANCEL; i++) script[n++] = DOWN;
    script[n++] = ESCAPE;
    run_script(&cancel, n);
    CHECK(cancel.f.lit_cancel > 0);
}

/* 7: the text goes through the background-restoring op. Every redraw repaints the whole page -- the screen blits
 * the clean background whole and then writes the letters over it -- so the letters have to land through the draw
 * that puts the background back underneath first, the way Options' own rows do. Without it the letters are written
 * straight onto whatever was on the picture and a move leaves the old row's edges behind. */
static void test_text_is_drawn_over_the_background(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    /* every word on the page, both times over: the screen must not fall back to the plain draw for any of them */
    CHECK(r.f.smooth_draws > 0);
    CHECK_EQ_INT(r.f.smooth_draws, r.f.text_count);
}

/* 8: the smooth draw is used only where there is a background to restore. The original screen (the race's pause
 * menu) has the burnt-in picture rather than the clean one, so the glue leaves this slot empty and the rows must
 * still be drawn. */
static void test_rows_are_drawn_without_a_smooth_op(void)
{
    Rig r;
    rig_init(&r);
    OPS.plus_text_smooth = NULL;
    script[0] = ESCAPE;
    run_script(&r, 1);
    OPS.plus_text_smooth = f_text_smooth;
    CHECK_EQ_INT(r.f.smooth_draws, 0);
    CHECK(drew(&r.f, "Perspective Correction"));
    CHECK(drew(&r.f, "Full Screen"));
    CHECK(r.f.title_draws > 0);
}

/* 4: View Distance offers four values, "Ultra" among them: the fourth is the port's, and it turns view_ultra on */
static void test_view_distance_has_ultra(void)
{
    PortSettings s = port_settings_defaults();
    int16_t view = 0;
    char out[16];
    graphics_own_view_value(view, s.view_ultra, out);
    CHECK(strcmp(out, "Far") == 0);
    /* three steps from far: medium, close, ultra */
    for (int i = 0; i < 3; i++) graphics_own_view_step(&view, &s.view_ultra);
    CHECK_EQ_INT(s.view_ultra, 1);
    graphics_own_view_value(view, s.view_ultra, out);
    CHECK(strcmp(out, "Ultra") == 0);
    /* and on to far again, which turns it off */
    graphics_own_view_step(&view, &s.view_ultra);
    CHECK_EQ_INT(s.view_ultra, 0);
    graphics_own_view_value(view, s.view_ultra, out);
    CHECK(strcmp(out, "Far") == 0);
}

/* 4: the View Distance row is the fourth, and stepping it on the screen walks view_ultra */
static void test_view_distance_cycles(void)
{
    Rig r;
    rig_init(&r);
    /* down to row 3 and three steps to the right */
    run_script(&r, script_walk(GRAPHICS_OWN_VIEW, RIGHT, 3, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(r.port.view_ultra, 1);
    CHECK_EQ_INT(r.f.saves, 1);   /* Done kept it, so the four steps are what set it */
}

/* 5: the rows write the settings the owner named, each one from its own row */
static void test_values_write_settings(void)
{
    /* Scaling (row 2) toggles scale_integer */
    Rig scaling;
    rig_init(&scaling);
    run_script(&scaling, script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(scaling.port.scale_integer, 1);

    /* Full screen (row 8) takes the desktop's format */
    Rig full;
    rig_init(&full);
    run_script(&full, script_walk(GRAPHICS_OWN_FULLSCREEN, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(full.port.fullscreen, 1);
    CHECK_EQ_INT(full.port.format, SCREEN_21_9);

    /* Refresh rate (row 7) steps 60 -> 120 */
    Rig refresh;
    rig_init(&refresh);
    run_script(&refresh, script_walk(GRAPHICS_OWN_REFRESH, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(refresh.port.refresh_hz, 120);

    /* Field of view (row 5) steps from the default 0 up to +5 */
    Rig fov;
    rig_init(&fov);
    run_script(&fov, script_walk(GRAPHICS_OWN_FOV, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(fov.port.fov_adjust, 5);

    /* Perspective correction (row 0), texture filter (row 1) and fog (row 4) turn on */
    Rig perspective;
    rig_init(&perspective);
    run_script(&perspective, script_walk(GRAPHICS_OWN_PERSPECTIVE, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(perspective.port.perspective, 1);

    Rig filter;
    rig_init(&filter);
    run_script(&filter, script_walk(GRAPHICS_OWN_FILTER, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(filter.port.texture_filter, 1);

    Rig fog;
    rig_init(&fog);
    run_script(&fog, script_walk(GRAPHICS_OWN_FOG, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(fog.port.fog, 1);

    /* Screen format (row 6) steps through its three */
    Rig format;
    rig_init(&format);
    run_script(&format, script_walk(GRAPHICS_OWN_FORMAT, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(format.port.format, SCREEN_16_9);
}

/* 6: Done saves, Cancel puts back, Escape puts back */
static void test_done_saves_cancel_restores(void)
{
    /* Scaling, changed, then Done: to row 2, right, down to Done, choose */
    Rig d;
    rig_init(&d);
    int n = script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE);
    CHECK_EQ_INT(run_script(&d, n), 1);   /* Done */
    CHECK_EQ_INT(d.f.saves, 1);
    CHECK_EQ_INT(d.f.put_backs, 0);       /* Done keeps the settings, it does not put them back */
    CHECK_EQ_INT(d.port.scale_integer, 1);

    /* the same change, then Cancel: nothing saved, and the settings as they were on entry */
    Rig c;
    rig_init(&c);
    n = script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, ESCAPE);
    n--;
    for (int i = 0; i < GRAPHICS_OWN_CANCEL - GRAPHICS_OWN_SCALING; i++) script[n++] = DOWN;
    script[n++] = 0;
    script[n++] = SPACE;
    script[n++] = 0;
    script[n++] = ESCAPE;
    CHECK_EQ_INT(run_script(&c, n), 0);   /* Cancel */
    CHECK_EQ_INT(c.f.saves, 0);
    CHECK_EQ_INT(c.f.put_backs, 1);
    CHECK_EQ_INT(c.port.scale_integer, 0);   /* and the settings are as they were on entry */

    /* and Escape, straight away, without touching anything */
    Rig e;
    rig_init(&e);
    script[0] = ESCAPE;
    CHECK_EQ_INT(run_script(&e, 1), 0);
    CHECK_EQ_INT(e.f.saves, 0);
    CHECK_EQ_INT(e.f.put_backs, 1);
}

/* 7: every row the owner named is reachable, and the walk wraps at both ends */
static void test_down_walks_every_row(void)
{
    /* down from the first reaches each row in turn */
    for (int row = 1; row < GRAPHICS_OWN_ROWS; row++) {
        Rig r;
        rig_init(&r);
        const int count = script_walk(row, 0, 0, ESCAPE);   /* no key on the row, only the Escape that ends */
        script[count - 1] = ESCAPE;
        run_script(&r, count - 1);                 /* the walk, without the key that would end it */
        CHECK_EQ_INT(r.f.pointer_row, row);
    }
    /* down from the last wraps to the first */
    Rig wrap;
    rig_init(&wrap);
    int n = 0;
    for (int i = 0; i < GRAPHICS_OWN_ROWS; i++) script[n++] = DOWN;
    script[n++] = ESCAPE;
    run_script(&wrap, n);
    CHECK_EQ_INT(wrap.f.pointer_row, 0);

    /* and up from the first wraps to the last */
    Rig up;
    rig_init(&up);
    script[0] = UP;
    script[1] = ESCAPE;
    run_script(&up, 2);
    CHECK_EQ_INT(up.f.pointer_row, GRAPHICS_OWN_CANCEL);
}

/* 8: FOV's value text */
static void test_fov_value(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    CHECK(strcmp(out, "0") == 0);
    s.fov_adjust = 20;
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    CHECK(strcmp(out, "+20") == 0);
    s.fov_adjust = -10;
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    CHECK(strcmp(out, "-10") == 0);
}

/* 9: every row's value text, pinned one by one */
static void test_value_texts(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_PERSPECTIVE, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FILTER, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_SCALING, out);
    CHECK(strcmp(out, "Fill") == 0);
    graphics_own_view_value(0, s.view_ultra, out);
    CHECK(strcmp(out, "Far") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FOG, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FORMAT, out);
    CHECK(strcmp(out, "4:3") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_REFRESH, out);
    CHECK(strcmp(out, "60 Hz") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FULLSCREEN, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_BANDS, out);
    CHECK(strcmp(out, "Auto") == 0);   /* Auto is the default: as many threads as the logical cores */
    graphics_own_value(&s, GRAPHICS_OWN_SCALE, out);
    CHECK(strcmp(out, "1x") == 0);   /* 1x is the default: the world drawn at the frame's size */

    s.view_ultra = 1;
    graphics_own_view_value(0, s.view_ultra, out);
    CHECK(strcmp(out, "Ultra") == 0);
    graphics_own_view_value(1, 0, out);
    CHECK(strcmp(out, "Medium") == 0);
    graphics_own_view_value(2, 0, out);
    CHECK(strcmp(out, "Close") == 0);
    s.scale_integer = 1;
    graphics_own_value(&s, GRAPHICS_OWN_SCALING, out);
    CHECK(strcmp(out, "Whole") == 0);
    s.format = SCREEN_21_9;
    graphics_own_value(&s, GRAPHICS_OWN_FORMAT, out);
    CHECK(strcmp(out, "21:9") == 0);
}

/* the render threads' four values: Off, Auto, 2 and 4 */
static void test_render_threads_values(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_BANDS, out);
    CHECK(strcmp(out, "Auto") == 0);   /* Auto is the default */
    s.render_bands = 0;
    graphics_own_value(&s, GRAPHICS_OWN_BANDS, out);
    CHECK(strcmp(out, "Off") == 0);
    s.render_bands = -1;
    graphics_own_value(&s, GRAPHICS_OWN_BANDS, out);
    CHECK(strcmp(out, "Auto") == 0);
    s.render_bands = 2;
    graphics_own_value(&s, GRAPHICS_OWN_BANDS, out);
    CHECK(strcmp(out, "2") == 0);
    s.render_bands = 4;
    graphics_own_value(&s, GRAPHICS_OWN_BANDS, out);
    CHECK(strcmp(out, "4") == 0);
}

/* the render scale's three values: 1x, 2x and 3x */
static void test_render_scale_values(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_SCALE, out);
    CHECK(strcmp(out, "1x") == 0);   /* 1x is the default */
    s.render_scale = 2;
    graphics_own_value(&s, GRAPHICS_OWN_SCALE, out);
    CHECK(strcmp(out, "2x") == 0);
    s.render_scale = 3;
    graphics_own_value(&s, GRAPHICS_OWN_SCALE, out);
    CHECK(strcmp(out, "3x") == 0);
}

/* the row steps through its three values, wrapping at both ends, and writes only its own setting */
static void test_render_scale_step(void)
{
    PortSettings s = port_settings_defaults();
    CHECK_EQ_INT(s.render_scale, 1);   /* 1x is where the row starts */
    graphics_own_step(&s, GRAPHICS_OWN_SCALE, 1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_scale, 2);   /* 1x -> 2x */
    graphics_own_step(&s, GRAPHICS_OWN_SCALE, 1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_scale, 3);
    graphics_own_step(&s, GRAPHICS_OWN_SCALE, 1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_scale, 1);   /* 3x -> 1x */
    graphics_own_step(&s, GRAPHICS_OWN_SCALE, -1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_scale, 3);   /* and the other way round */
    /* the rest of the settings are not touched */
    CHECK_EQ_INT(s.format, SCREEN_4_3);
    CHECK_EQ_INT(s.refresh_hz, 60);
    CHECK_EQ_INT(s.fullscreen, 0);
    CHECK_EQ_INT(s.render_bands, -1);
}

/* the row steps through its four values, wrapping at both ends, and writes only its own setting */
static void test_render_threads_step(void)
{
    PortSettings s = port_settings_defaults();
    CHECK_EQ_INT(s.render_bands, -1);   /* Auto is where the row starts */
    graphics_own_step(&s, GRAPHICS_OWN_BANDS, 1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_bands, 2);   /* Auto -> 2 */
    graphics_own_step(&s, GRAPHICS_OWN_BANDS, 1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_bands, 4);
    graphics_own_step(&s, GRAPHICS_OWN_BANDS, 1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_bands, 0);   /* 4 -> Off */
    graphics_own_step(&s, GRAPHICS_OWN_BANDS, 1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_bands, -1);  /* and back to Auto */
    graphics_own_step(&s, GRAPHICS_OWN_BANDS, -1, SCREEN_4_3);
    CHECK_EQ_INT(s.render_bands, 0);   /* and the other way round */
    /* the rest of the settings are not touched */
    CHECK_EQ_INT(s.format, SCREEN_4_3);
    CHECK_EQ_INT(s.refresh_hz, 60);
    CHECK_EQ_INT(s.fullscreen, 0);
}

/* 10: a row steps its own setting only */
static void test_step_writes_only_its_row(void)
{
    PortSettings s = port_settings_defaults();
    const PortSettings before = s;
    graphics_own_step(&s, GRAPHICS_OWN_SCALING, 1, SCREEN_16_9);
    CHECK_EQ_INT(s.scale_integer, before.scale_integer ^ 1);
    CHECK_EQ_INT(s.format, before.format);
    CHECK_EQ_INT(s.refresh_hz, before.refresh_hz);
    CHECK_EQ_INT(s.fullscreen, before.fullscreen);
    CHECK_EQ_INT(s.fov_adjust, before.fov_adjust);
    CHECK_EQ_INT(s.view_ultra, before.view_ultra);
}

/* 11: turning full screen on takes the desktop's format */
static void test_fullscreen_takes_desktop_format(void)
{
    PortSettings s = port_settings_defaults();
    s.format = SCREEN_4_3, s.profile = PROFILE_RESTOMOD;
    graphics_own_step(&s, GRAPHICS_OWN_FULLSCREEN, 1, SCREEN_21_9);
    CHECK_EQ_INT(s.fullscreen, 1);
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    /* not where the mode holds the format */
    s = port_settings_defaults();
    s.format = SCREEN_4_3, s.profile = PROFILE_STOCK;
    graphics_own_step(&s, GRAPHICS_OWN_FULLSCREEN, 1, SCREEN_21_9);
    CHECK_EQ_INT(s.fullscreen, 1);
    CHECK_EQ_INT(s.format, SCREEN_4_3);
}

/* 12: port: 00423d28 leaves through one door, and only Cancel says no. Its loop ends on Done, on Cancel, and on the
 * idle deadline, the right button or the quit flag -- and everything but Cancel falls through to the same plus_save
 * (graphics_menu.c: put_back is the only thing that sets cancelled). So the three exits that are not the player's
 * decision still save what they changed on the way past, and the screen reports that it left. Each walk changes a
 * setting and then settles, with no Done, no Cancel and no Escape: the trigger, not a key, is what ends it. */

/* one trigger's run: change Scaling, then let `f` end the screen the way that trigger does */
static void test_the_three_exits_that_are_not_a_decision_save(void)
{
    enum { SCALING = GRAPHICS_OWN_SCALING };
    int n;

    /* the idle deadline passing */
    Rig idle;
    rig_init(&idle);
    change_a_setting_then_settle(SCALING, RIGHT, &n);
    idle.f.deadline_passed = 1;
    CHECK_EQ_INT(run_script_idling(&idle, n), 1);   /* it left, and left as Done leaves */
    CHECK_EQ_INT(idle.f.saves, 1);
    CHECK_EQ_INT(idle.f.put_backs, 0);
    CHECK_EQ_INT(idle.port.scale_integer, 1);   /* the change is still in the settings that were saved */

    /* the right button: a mouse button only reaches the screen while the pointer is active, so the rig turns it on */
    Rig right;
    rig_init(&right);
    change_a_setting_then_settle(SCALING, RIGHT, &n);
    right.f.right_button = 1;
    right.pointer_active = 1;
    CHECK_EQ_INT(run_script_idling(&right, n), 1);
    CHECK_EQ_INT(right.f.saves, 1);
    CHECK_EQ_INT(right.f.put_backs, 0);
    CHECK_EQ_INT(right.port.scale_integer, 1);

    /* the quit flag */
    Rig quit;
    rig_init(&quit);
    change_a_setting_then_settle(SCALING, RIGHT, &n);
    CHECK_EQ_INT(run_script_quitting(&quit, n), 1);
    CHECK_EQ_INT(quit.f.saves, 1);
    CHECK_EQ_INT(quit.f.put_backs, 0);
    CHECK_EQ_INT(quit.port.scale_integer, 1);
}

/* the render threads on the page: the tenth row, its label, its value, and the band it sits in */
static void test_render_threads_row_on_the_page(void)
{
    Rig r;
    rig_init(&r);
    /* the pointer down to the last row and on to Done: the screen asks about the bands from the first row up to
     * the one the pointer is in, so only a walk that reaches the last row asks about every row's band. */
    int n = 0;
    for (int i = 0; i < GRAPHICS_OWN_SETTINGS; i++) script[n++] = DOWN;
    script[n++] = 0;
    script[n++] = SPACE;
    script[n++] = 0;
    run_script(&r, n);

    /* the label is drawn, in the label column, with the other ten */
    CHECK(drew(&r.f, "Render Threads"));
    for (int i = 0; i < r.f.text_count && i < TEXTS; i++)
        if (strcmp(r.f.texts[i], "Render Threads") == 0) CHECK_EQ_INT(r.f.text_x[i], GRAPHICS_OWN_LABEL_X);
    /* and the value, on the right: Auto, the default */
    CHECK(drew(&r.f, "Auto"));

    /* the tenth row's band, and every row's: the bands the screen asked about, in order. Its band ends above the
     * Done/Cancel band, which starts at 0x1a4 (420) -- a row's band must not run into it, or that row and the
     * buttons would share their rows. */
    CHECK(r.f.band_best >= GRAPHICS_OWN_SETTINGS);
    CHECK_EQ_INT(r.f.band_best_tops[GRAPHICS_OWN_BANDS], 365);
    CHECK(r.f.band_best_bottoms[GRAPHICS_OWN_BANDS] <= 0x1a4);
}

/* the render scale on the page: the eleventh row, its label, its value, and the band it sits in */
static void test_render_scale_row_on_the_page(void)
{
    Rig r;
    rig_init(&r);
    /* the pointer down to the eleventh row and on to Done: the screen asks about the bands from the first row up to
     * the one the pointer is in, so only a walk that reaches the last row asks about every row's band. */
    int n = 0;
    for (int i = 0; i < GRAPHICS_OWN_SCALE + 1; i++) script[n++] = DOWN;
    script[n++] = 0;
    script[n++] = SPACE;
    script[n++] = 0;
    run_script(&r, n);

    /* the label is drawn, in the label column, with the other ten */
    CHECK(drew(&r.f, "Render Scale"));
    for (int i = 0; i < r.f.text_count && i < TEXTS; i++)
        if (strcmp(r.f.texts[i], "Render Scale") == 0) CHECK_EQ_INT(r.f.text_x[i], GRAPHICS_OWN_LABEL_X);
    /* and the value, on the right: 1x, the default */
    CHECK(drew(&r.f, "1x"));

    /* the eleventh row's band, and every row's: the bands the screen asked about, in order. The last one is the
     * eleventh row's, and it ends above the Done/Cancel band, which starts at 0x1a4 (420) -- the rows' band must not
     * run into it, or the last row and the buttons would share their rows. */
    CHECK(r.f.band_best >= GRAPHICS_OWN_SETTINGS);
    CHECK_EQ_INT(r.f.band_best_tops[GRAPHICS_OWN_SCALE], 391);
    CHECK(r.f.band_best_bottoms[GRAPHICS_OWN_SCALE] <= 0x1a4);
}

/* the eleventh row is reachable and its step writes the setting */
static void test_render_scale_row_is_reachable(void)
{
    Rig r;
    rig_init(&r);
    run_script(&r, script_walk(GRAPHICS_OWN_SCALE, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(r.port.render_scale, 2);   /* 1x -> 2x */
    CHECK_EQ_INT(r.f.saves, 1);
}

/* the tenth row is reachable and its step writes the setting */
static void test_render_threads_row_is_reachable(void)
{
    Rig r;
    rig_init(&r);
    run_script(&r, script_walk(GRAPHICS_OWN_BANDS, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(r.port.render_bands, 2);   /* Auto -> 2 */
    CHECK_EQ_INT(r.f.saves, 1);
}

/* the order the owner named: Distance Fog, Light Effects right after it, Scaling right after Full screen; and no
 * Time of day, Weather, PS1 rows or page indicator */
static void test_reorganised_screen(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    static const char *const order[] = { "Perspective Correction", "Texture Filter", "View Distance", "Distance Fog",
                                         "Light Effects", "Field Of View", "Screen Format", "Refresh Rate",
                                         "Full Screen", "Scaling", "Render Threads", "Render Scale" };
    int last_y = -1;
    for (size_t i = 0; i < sizeof order / sizeof *order; i++) {
        int y = -1;
        for (int t = 0; t < r.f.text_count && t < TEXTS; t++)
            if (strcmp(r.f.texts[t], order[i]) == 0 && r.f.text_x[t] == GRAPHICS_OWN_LABEL_X) y = r.f.text_y[t];
        CHECK(y > last_y);
        last_y = y;
    }
    CHECK(!drew(&r.f, "Fog"));
    CHECK(!drew(&r.f, "Time of day"));
    CHECK(!drew(&r.f, "Weather"));
    CHECK(!drew(&r.f, "PS1 light"));
    CHECK(!drew(&r.f, "PS1 shadow"));
    CHECK_EQ_INT(r.f.nav_last[0], 0);   /* one page: no indicator */
}

/* every row is the same distance from the one before it, the first included */
static void test_rows_are_evenly_spaced(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    static const char *const order[] = { "Perspective Correction", "Texture Filter", "View Distance", "Distance Fog",
                                         "Light Effects", "Field Of View", "Screen Format", "Refresh Rate",
                                         "Full Screen", "Scaling", "Render Threads", "Render Scale" };
    int last_y = -1, pitch = -1;
    for (size_t i = 0; i < sizeof order / sizeof *order; i++) {
        int y = -1;
        for (int t = 0; t < r.f.text_count && t < TEXTS; t++)
            if (strcmp(r.f.texts[t], order[i]) == 0 && r.f.text_x[t] == GRAPHICS_OWN_LABEL_X) y = r.f.text_y[t];
        if (i > 0) {
            if (pitch < 0) pitch = y - last_y;
            CHECK_EQ_INT(y - last_y, pitch);
        }
        last_y = y;
    }
}

/* the last row ends above the buttons' band, and the bands do not overlap */
static void test_rows_fit_above_the_buttons(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    for (int t = 0; t < r.f.text_count && t < TEXTS; t++)
        if (r.f.text_x[t] == GRAPHICS_OWN_LABEL_X && strcmp(r.f.texts[t], "Render Scale") == 0)
            CHECK(r.f.text_y[t] + 10 < 0x1a4);
}

int main(void)
{
    test_rows_in_order();
    test_done_and_cancel_light_up();
    test_title_top_left();
    test_palette_reaches_the_display();
    test_palette_is_not_rewritten_in_place();
    test_palette_is_put_raw();
    test_done_and_cancel_are_not_drawn_twice();
    test_text_is_drawn_over_the_background();
    test_rows_are_drawn_without_a_smooth_op();
    test_view_distance_has_ultra();
    test_view_distance_cycles();
    test_values_write_settings();
    test_done_saves_cancel_restores();
    test_down_walks_every_row();
    test_fov_value();
    test_value_texts();
    test_render_threads_values();
    test_render_threads_step();
    test_render_threads_row_on_the_page();
    test_render_threads_row_is_reachable();
    test_render_scale_values();
    test_render_scale_step();
    test_render_scale_row_on_the_page();
    test_render_scale_row_is_reachable();
    test_step_writes_only_its_row();
    test_fullscreen_takes_desktop_format();
    test_the_three_exits_that_are_not_a_decision_save();
    test_reorganised_screen();
    test_rows_fit_above_the_buttons();
    test_rows_are_evenly_spaced();
    return TEST_REPORT();
}

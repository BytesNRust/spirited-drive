/* The port's own Graphics screen (graphics_own.c), split from test_graphics_own.c: what each mode holds (dimmed, not changed) and the rows
 * the race's pause menu leaves live, dims or puts back. Same rig (graphics_own_rig.h). */
#include "graphics_own_rig.h"

/* what each mode holds: dimmed, not changed; Stock leaves only Full Screen, Scaling and the view distance without Ultra */
static void test_the_mode_holds_its_rows(void)
{
    static const int STOCK_HELD[] = { GRAPHICS_OWN_PERSPECTIVE, GRAPHICS_OWN_FILTER, GRAPHICS_OWN_FOG, GRAPHICS_OWN_LIGHT, GRAPHICS_OWN_FOV, GRAPHICS_OWN_FORMAT, GRAPHICS_OWN_REFRESH,
                                      GRAPHICS_OWN_BANDS, GRAPHICS_OWN_SCALE };
    static const int RESTORED_FREE[] = { GRAPHICS_OWN_FORMAT, GRAPHICS_OWN_REFRESH, GRAPHICS_OWN_SCALING, GRAPHICS_OWN_BANDS };
    for (size_t i = 0; i < sizeof STOCK_HELD / sizeof STOCK_HELD[0]; i++) {
        Rig r;
        rig_init(&r);
        r.port.profile = PROFILE_STOCK;
        const PortSettings was = r.port;
        run_script(&r, script_walk(STOCK_HELD[i], RIGHT, 1, GRAPHICS_OWN_DONE));
        CHECK(memcmp(&r.port, &was, sizeof was) == 0);
        CHECK(r.f.dim_draws > 0);
        CHECK(!drew(&r.f, "Off (next race)") && !drew(&r.f, "4:3 (next race)"));   /* the mode's, not the next race's */
    }
    Rig full;
    rig_init(&full);
    full.port.profile = PROFILE_STOCK;
    run_script(&full, script_walk(GRAPHICS_OWN_FULLSCREEN, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(full.port.fullscreen, 1);
    Rig scaling;   /* Scaling is free in Stock: Whole -> Fill (a retina's whole scale is the player's to leave) */
    rig_init(&scaling);
    scaling.port.profile = PROFILE_STOCK, scaling.port.scale_integer = 1;
    run_script(&scaling, script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(scaling.port.scale_integer, 0);
    for (size_t i = 0; i < sizeof RESTORED_FREE / sizeof RESTORED_FREE[0]; i++) {
        Rig r;
        rig_init(&r);
        r.port.profile = PROFILE_RESTORED;
        const PortSettings was = r.port;
        run_script(&r, script_walk(RESTORED_FREE[i], RIGHT, 1, GRAPHICS_OWN_DONE));
        CHECK(memcmp(&r.port, &was, sizeof was) != 0);
    }
    /* the view distance: Far, Medium, Close and round, no Ultra, in Stock */
    for (int p = PROFILE_STOCK; p <= PROFILE_RESTOMOD; p++) {
        Rig r;
        rig_init(&r);
        r.port.profile = p;
        run_script(&r, script_walk(GRAPHICS_OWN_VIEW, RIGHT, 3, GRAPHICS_OWN_DONE));
        CHECK_EQ_INT(r.port.view_ultra, p == PROFILE_RESTOMOD);
    }
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* 7: the pause menu's Graphics (plus.race): the rows the next race reads are dimmed and not edited there. The rule is
 * a pure one, so it is pinned first, then the screen's own behaviour under it. */
static void test_race_live_rows(void)
{
    /* the live rows: the three that moved to EXTRAS, fog, the field, refresh and the view's three near values */
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_PERSPECTIVE, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FILTER, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FOG, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FOV, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_REFRESH, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 2, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 1, 1), 1);
    /* the next race's: scaling, the format, full screen, the render threads, and Ultra (view 0 with the flag) */
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_SCALING, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FORMAT, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FULLSCREEN, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_BANDS, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_SCALE, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 0, 1), 0);
}

/* the view distance in the race goes Far, Medium, Close and back to Far; landing on Far drops the Ultra flag, so the
 * row never sits on a value it cannot leave */
static void test_race_view_steps_without_ultra(void)
{
    int16_t view = 0;
    int ultra = 0;
    graphics_own_race_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 1);
    graphics_own_race_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 2);
    CHECK_EQ_INT(ultra, 0);
    int16_t from_close = 2;
    int flag = 1;
    graphics_own_race_view_step(&from_close, &flag);
    CHECK_EQ_INT(from_close, 0);
    CHECK_EQ_INT(flag, 0);
}

/* the pause menu's walk: a dimmed row does not change, a live one does, and the dimmed ones are drawn dimmed */
static void test_race_dims_next_race_rows(void)
{
    Rig scaling;
    rig_init(&scaling);
    scaling.plus.race = 1;
    run_script(&scaling, script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(scaling.port.scale_integer, 0);
    CHECK(scaling.f.dim_draws > 0);
    CHECK(drew(&scaling.f, "Fill (next race)"));

    Rig format;
    rig_init(&format);
    format.plus.race = 1;
    run_script(&format, script_walk(GRAPHICS_OWN_FORMAT, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(format.port.format, SCREEN_4_3);

    Rig full;
    rig_init(&full);
    full.plus.race = 1;
    run_script(&full, script_walk(GRAPHICS_OWN_FULLSCREEN, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(full.port.fullscreen, 0);

    /* Options draws nothing dimmed and does change scaling */
    Rig options;
    rig_init(&options);
    run_script(&options, script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(options.f.dim_draws, 0);
    CHECK_EQ_INT(options.port.scale_integer, 1);

    /* the live rows change in the race: fog is written and saved */
    Rig fog;
    rig_init(&fog);
    fog.plus.race = 1;
    run_script(&fog, script_walk(GRAPHICS_OWN_FOG, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(fog.port.fog, 1);
    CHECK_EQ_INT(fog.f.saves, 1);

    /* Ultra is dimmed and holds still in the race */
    Rig view;
    rig_init(&view);
    view.plus.race = 1;
    view.port.view_ultra = 1;
    view.f.kept = view.port;
    run_script(&view, script_walk(GRAPHICS_OWN_VIEW, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(view.port.view_ultra, 1);
    CHECK(drew(&view.f, "Ultra (next race)"));

    /* the render threads are read when the race starts, so the pause menu shows them dimmed and holds them still */
    Rig bands;
    rig_init(&bands);
    bands.plus.race = 1;
    run_script(&bands, script_walk(GRAPHICS_OWN_BANDS, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(bands.port.render_bands, -1);   /* dimmed: it holds still at the default, Auto */
    CHECK(drew(&bands.f, "Auto (next race)"));

    /* and so is the render scale: the framebuffer is sized when the race starts, not while it runs */
    Rig scale;
    rig_init(&scale);
    scale.plus.race = 1;
    run_script(&scale, script_walk(GRAPHICS_OWN_SCALE, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(scale.port.render_scale, 1);   /* dimmed: it holds still at the default, 1x */
    CHECK(drew(&scale.f, "1x (next race)"));
}

/* Escape in the race puts the settings back, the way Options does */
static void test_race_cancel_puts_back(void)
{
    Rig r;
    rig_init(&r);
    r.plus.race = 1;
    run_script(&r, script_walk(GRAPHICS_OWN_FOG, RIGHT, 1, ESCAPE));
    CHECK_EQ_INT(r.f.put_backs, 1);
    CHECK_EQ_INT(r.port.fog, 0);
    CHECK_EQ_INT(r.f.saves, 0);
}

int main(void)
{
    test_race_live_rows();
    test_race_view_steps_without_ultra();
    test_race_dims_next_race_rows();
    test_race_cancel_puts_back();
    test_the_mode_holds_its_rows();
    return TEST_REPORT();
}

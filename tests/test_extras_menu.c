/* The EXTRAS screen (extras_menu.c): two pages of switches driven from config.cfg and to it on Done. The plan's steps
 * are exercised with fake ops and a key script, the same shape as test_graphics_plus.c. */
#include "extras_rig.h"

/* Edit Mirror (on page 1, then Done) turns it on and turns Rally off when Rally was on */
static void test_mirror_toggle_excludes_rally(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_MIRROR);
    keys[n++] = ENTER;                                             /* Mirror on */
    n = downs(keys, n, downs_to_done(ROW_MIRROR));
    keys[n++] = ENTER;                                             /* Done */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    r.e.rally = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.mirror, 1);
    CHECK_EQ_INT(r.e.rally, 0);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* Escape restores the input PortExtras */
static void test_escape_restores_input(void)
{
    Rig r;
    const int keys[] = { DOWN, SPACE, DOWN, SPACE, DOWN, SPACE, ESCAPE };  /* to Explode, toggle off it, Nitro, Turbo and quit */
    setup(&r);
    r.e.explode = 1;
    r.e.nitro = 1;
    r.e.turbo = 1;
    run(&r, keys, 7);
    CHECK_EQ_INT(r.e.explode, 1);
    CHECK_EQ_INT(r.e.nitro, 1);
    CHECK_EQ_INT(r.e.turbo, 1);
}

/* Right on the last row (Cancel) turns to page 2: Down from the first switch round to Done, Right Cancel, Right page 2 */
static void test_right_turns_to_page_two(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P2);
    keys[n++] = ESCAPE;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "< 2/3 >"));
    CHECK(shown(&r.fake, "Speed units"));
}

/* Rally on with Mirror on turns Mirror off (both on page 1) */
static void test_rally_excludes_mirror(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_RALLY);
    keys[n++] = SPACE;                                             /* Rally on */
    n = downs(keys, n, downs_to_done(ROW_RALLY));
    keys[n++] = ENTER;                                             /* Done */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    r.e.mirror = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.rally, 1);
    CHECK_EQ_INT(r.fake.saved.mirror, 0);
}

/* Warrior Player Car (CHEATS, page 1) and Start changed, Done saves them; Start Screen is the Miscellaneous row */
static void test_page_two_switches_saved(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_WARRIOR);                 /* Warrior Player Car */
    keys[n++] = ENTER;                                     /* on */
    n = downs(keys, n, downs_to_done(ROW_WARRIOR));         /* page 1's Done */
    keys[n++] = RIGHT, keys[n++] = RIGHT;                  /* Done to Cancel, Cancel to page 2's first row */
    n = downs(keys, n, ROWS_P2);                           /* page 2's Done */
    keys[n++] = RIGHT, keys[n++] = RIGHT;                  /* and on to page 3's first row, Start Screen */
    for (int i = 0; i < 3; i++) keys[n++] = LEFT;          /* Start's three values, round back to Game Mode */
    n = downs(keys, n, downs_to_done(ROW_START));          /* down to Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.warrior, 1);
    CHECK_EQ_INT((int)r.fake.saved.start, START_GAME_MODE);
}

/* Unlock Level 1 or more gives the Warrior as the exe does (extras_apply.c's settle): the row shows that, dimmed and
 * with its reason, and a toggle does not turn it off behind the player's back */
static void test_warrior_row_is_fixed_by_the_level(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_WARRIOR);
    keys[n++] = SPACE;                                       /* tries to turn it on or off */
    n = downs(keys, n, downs_to_done(ROW_WARRIOR));
    keys[n++] = ENTER;
    setup(&r);
    r.unlock = 1;
    r.e.warrior = 0;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.warrior, 0);                   /* the player's switch is untouched */
    CHECK(shown(&r.fake, "On (level)"));
    CHECK_EQ_INT(style_in(&r.fake, "On (level)"), STYLE_DIM);
}

/* at level 0 the row is an ordinary switch */
static void test_warrior_row_toggles_at_level_zero(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_WARRIOR);
    keys[n++] = SPACE;
    n = downs(keys, n, downs_to_done(ROW_WARRIOR));
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saved.warrior, 1);
    CHECK(!shown(&r.fake, "On (level)"));
}

/* The three settings that moved to the port's own Graphics screen are not here any more. */
static void test_the_graphics_settings_are_gone(void)
{
    Rig r;
    /* page 2 on Intro video and turn it on; Down to Done */
    int keys[48];
    int n = keys_to_row(keys, ROW_MOVIES);
    keys[n++] = RIGHT;                                     /* off: no movies */
    n = downs(keys, n, downs_to_done(ROW_MOVIES)), keys[n++] = ENTER;   /* down to Done, save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Intro video"));
    CHECK(!shown(&r.fake, "Modern 3D"));
    CHECK(!shown(&r.fake, "Texture Filter"));
    CHECK(!shown(&r.fake, "Fog"));
    CHECK_EQ_INT(r.fake.saved.no_movies, 1);
}

/* CHEATS and UNLOCKS & MODS on page 1: thirteen switches, Car Colours the last one. Walk the whole page down
 * and they are all of them */
static void test_page_one_has_thirteen_rows(void)
{
    Rig r;
    int keys[48];
    int n = downs(keys, 0, 1 + ROWS_P1);                   /* to the first row, then down to Done past the thirteen */
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROWS_P1, 12);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Explode mode"));
    CHECK(shown(&r.fake, "Nitro mode"));
    CHECK(shown(&r.fake, "Turbo mode"));
    CHECK(shown(&r.fake, "Warrior Player Car"));
    CHECK(shown(&r.fake, "Warrior Rival Car"));
    CHECK(shown(&r.fake, "Mirror track mode"));
    CHECK(shown(&r.fake, "Rally track mode"));
    CHECK(shown(&r.fake, "Lost Vegas Track"));
    CHECK(shown(&r.fake, "Lunar Springs Track"));
    CHECK(shown(&r.fake, "Unlock Level"));
    CHECK(!shown(&r.fake, "Time with variants"));
    CHECK(shown(&r.fake, "Car Colours"));
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: Car Colours is the last row of Unlocks & mods (page 1): Enter turns it on, and Done saves it, the rest as it was */
static void test_second_livery_row_is_saved_on_done(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_EXTRA_LIVERY);
    keys[n++] = ENTER;                                     /* on */
    n = downs(keys, n, downs_to_done(ROW_EXTRA_LIVERY));     /* down to Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.extra_liveries, 1);
    CHECK_EQ_INT(r.fake.saved.no_movies, 0);
}

/* IN-RACE OPTIONS and MISCELLANEOUS on page 2: thirteen rows, Open the log folder the last of them. Walk the whole page down */
static void test_page_two_has_ten_rows(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P2);
    n = downs(keys, n, ROWS_P2);                           /* down to Done (the row past the eleven) */
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROWS_P2, 10);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Speed units"));
    CHECK(shown(&r.fake, "No driving assists"));
    CHECK(shown(&r.fake, "Bumper view mirror"));
    CHECK(shown(&r.fake, "Exterior views police radar"));
    CHECK(shown(&r.fake, "Replay cameras in race mode"));
    CHECK(shown(&r.fake, "Rival car markers"));
    CHECK(shown(&r.fake, "3D render stats"));
    CHECK(shown(&r.fake, "Dash box"));
    CHECK(shown(&r.fake, "360 Camera"));
    CHECK(shown(&r.fake, "Difficulty"));
    CHECK(!shown(&r.fake, "Start Screen"));   /* the Miscellaneous section is page 3's */
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* page 3 is the whole Miscellaneous section: Start Screen, Intro video, Updates and anonymous stats, Open the log folder; four rows */
/* the radar's row has one name now: it shows in every outside view */
static void test_the_radar_row_name(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P2);
    n = downs(keys, n, ROWS_P2);
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Exterior views police radar"));
    CHECK(!shown(&r.fake, "Bumper view police radar"));
    CHECK(!shown(&r.fake, "Rain with storm"));
}

/* a walk over row `row` of the whole list: change it with the arrows, then Done */
static void change_then_done(Rig *r, int row, int profile)
{
    int keys[64];
    int n = keys_to_row(keys, row);
    keys[n++] = RIGHT;
    const int first = row >= ROW_FIRST_P3 ? ROW_FIRST_P3 : row >= ROW_FIRST_P2 ? ROW_FIRST_P2 : 0;
    const int rows = first == ROW_FIRST_P3 ? ROWS_P3 : first == ROW_FIRST_P2 ? ROWS_P2 : ROWS_P1;
    n = downs(keys, n, rows - (row - first));
    keys[n++] = ENTER;
    setup(r);
    r->profile = profile;
    run(r, keys, n);
}

/* what a mode holds is dimmed, keeps its value and does not change; what it leaves free does */
static void test_the_mode_holds_its_rows(void)
{
    static const int HELD_IN_STOCK[] = { ROW_RIVAL, ROW_LUNAR, ROW_LUNAR_GRAVITY, ROW_EXTRA_LIVERY, ROW_ASSISTS, ROW_BUMPER_MIRROR, ROW_BUMPER_RADAR, ROW_REPLAY, ROW_MARKERS, ROW_STATS, ROW_DASH_BOX,
                                         ROW_FOLLOW_CAMERA, ROW_DIFFICULTY, ROW_START, ROW_MOVIES };
    static const int FREE_IN_STOCK[] = { ROW_EXPLODE, ROW_NITRO, ROW_TURBO, ROW_MIRROR, ROW_RALLY, ROW_VEGAS, ROW_UNITS, ROW_UPDATE };
    for (size_t i = 0; i < sizeof HELD_IN_STOCK / sizeof HELD_IN_STOCK[0]; i++) {
        Rig r;
        change_then_done(&r, HELD_IN_STOCK[i], PROFILE_STOCK);
        CHECK_EQ_INT(r.fake.saves, 1);
        Rig was;
        setup(&was);
        CHECK(memcmp(&r.fake.saved, &was.e, sizeof was.e) == 0);   /* Done saves it as it was */
    }
    for (size_t i = 0; i < sizeof FREE_IN_STOCK / sizeof FREE_IN_STOCK[0]; i++) {
        Rig r;
        change_then_done(&r, FREE_IN_STOCK[i], PROFILE_STOCK);
        Rig was;
        setup(&was);
        CHECK(memcmp(&r.fake.saved, &was.e, sizeof was.e) != 0);
    }
    Rig restored;
    change_then_done(&restored, ROW_STATS, PROFILE_RESTORED);   /* the one Restored adds here */
    CHECK_EQ_INT(restored.fake.saved.stats_overlay, 1);
    for (size_t i = 0; i < sizeof HELD_IN_STOCK / sizeof HELD_IN_STOCK[0]; i++) {
        Rig r;
        change_then_done(&r, HELD_IN_STOCK[i], PROFILE_RESTOMOD);
        Rig was;
        setup(&was);
        CHECK(memcmp(&r.fake.saved, &was.e, sizeof was.e) != 0);   /* Restomod leaves them all free */
    }
}

/* Restore profile defaults: the first Return says what the second does; the second acts; moving off the row forgets */
static void test_restore_takes_two_returns(void)
{
    Rig r;
    int keys[64];
    int n = keys_to_row(keys, ROW_RESET);
    keys[n++] = ENTER;
    keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.resets, 0);
    CHECK(shown(&r.fake, "Return again"));

    n = keys_to_row(keys, ROW_RESET);
    keys[n++] = ENTER, keys[n++] = ENTER;
    keys[n++] = ESCAPE;
    setup(&r);
    r.e.units = UNITS_KMH;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.resets, 1);

    n = keys_to_row(keys, ROW_RESET);
    keys[n++] = ENTER, keys[n++] = UP, keys[n++] = DOWN, keys[n++] = ENTER;
    keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.resets, 0);
}

static void test_page_three_is_miscellaneous(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P3);
    n = downs(keys, n, ROWS_P3);
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROWS_P3, 6);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Miscellaneous"));
    CHECK(shown(&r.fake, "Start Screen"));
    CHECK(shown(&r.fake, "Intro video"));
    CHECK(shown(&r.fake, "Updates and anonymous stats"));
    CHECK(shown(&r.fake, "Open the log folder"));
    CHECK(shown(&r.fake, "Star us on GitHub"));
    CHECK(shown(&r.fake, "Restore profile defaults"));
    CHECK(!shown(&r.fake, "Speed units"));
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: the two race switches read On / Off from extras.cfg's values (the markers On by default, the overlay Off), and
 * Done saves a flip of the markers row, the rest of the page as it was */
static void test_race_switch_rows_save_with_done(void)
{
    Rig r;
    int keys[48] = { 0 };
    int n = keys_to_row(keys, ROW_MARKERS);                /* Rival car markers, page 2 */
    keys[n++] = ENTER;                                     /* Car markers: On -> Off */
    n = downs(keys, n, downs_to_done(ROW_MARKERS));        /* past 3D render stats and Dash box, down to Done */
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Off"));
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.car_markers, 0);
    CHECK_EQ_INT(r.fake.saved.stats_overlay, 0);
    CHECK_EQ_INT(r.fake.saved.start, START_GAME_MODE);
}

/* the keys that walk from the screen's first row to its Speed units row (page 2's first row) */
static int keys_to_speed_units(int *keys)
{
    return keys_to_row(keys, ROW_UNITS);
}

/* port: the units row sits in IN-RACE OPTIONS, above the switches under it, and reads "mph" while the language is
 * English */
static void test_speed_units_row_reads_mph_in_english(void)
{
    Rig r;
    int keys[48] = { 0 };
    const int n = keys_to_speed_units(keys);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Speed units"));
    CHECK(shown(&r.fake, "mph"));
    CHECK(!shown(&r.fake, "km/h"));
    CHECK_EQ_INT(style_in(&r.fake, "Speed units"), STYLE_YELLOW);   /* the selection is on it */
    CHECK(y_in(&r.fake, "Speed units") < y_in(&r.fake, "3D render stats"));
}

/* Enter on the units row, from the default, turns the English units to km/h, and Done saves that */
static void test_speed_units_toggle_saves_kmh(void)
{
    Rig r;
    int keys[48] = { 0 };
    int n = keys_to_speed_units(keys);
    keys[n++] = ENTER;
    n = downs(keys, n, downs_to_done(ROW_UNITS));          /* down to Done */
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "km/h"));
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.units, UNITS_KMH);
    CHECK_EQ_INT(r.fake.saved.start, START_GAME_MODE);   /* the rest of the screen is as it was */
}

/* port: "Intro video" reads On by default (no_movies 0) and Off once the movies are switched off */
static void test_intro_video_reads_the_other_way(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_MOVIES);
    keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    const int32_t y = y_in(&r.fake, "Intro video");
    int on = 0, off = 0;
    for (int i = 0; i < r.fake.drawn_count; i++)
        if (r.fake.y_of[i] == y) on += strcmp(r.fake.drawn[i], "On") == 0, off += strcmp(r.fake.drawn[i], "Off") == 0;
    CHECK(on > 0);
    CHECK_EQ_INT(off, 0);
}

/* port: the buttons walk across the pages the way they did with three: Right on Cancel turns to the next page, Left
 * on page 2's Done goes back to page 1's Cancel, and Left again lands on page 1's Done, which saves. */
static void test_left_on_page_two_done_goes_back(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P2);               /* page 2, its first row */
    n = downs(keys, n, ROWS_P2);                           /* page 2's Done */
    keys[n++] = LEFT;                                      /* page 1's Cancel */
    keys[n++] = LEFT;                                      /* page 1's Done */
    keys[n++] = ENTER;                                     /* save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* Up from Cancel is the last row (Open the log folder), and the one over it the last switch (Updates and anonymous stats) */
static void test_up_from_cancel_reaches_the_last_switch(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P3);               /* page 3, its first row */
    n = downs(keys, n, ROWS_P3);                           /* page 3's Done */
    keys[n++] = RIGHT;                                     /* page 3's Cancel */
    keys[n++] = UP;                                        /* Restore profile defaults, the last row */
    keys[n++] = UP;                                        /* Star us on GitHub */
    keys[n++] = UP;                                        /* Open the log folder */
    keys[n++] = UP;                                        /* Updates and anonymous stats, the last switch */
    keys[n++] = RIGHT;                                     /* off */
    keys[n++] = DOWN;                                      /* the log folder row */
    keys[n++] = DOWN;                                      /* the Star row */
    keys[n++] = DOWN;                                      /* the Restore row */
    keys[n++] = DOWN;                                      /* Done */
    keys[n++] = ENTER;                                     /* save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.update_check, 0);
}

/* the Done/Cancel walk: Right from Done is Cancel, as it was before the regrouping, and Cancel saves nothing */
static void test_right_from_done_reaches_cancel(void)
{
    Rig r;
    int keys[48];
    int n = downs(keys, 0, 1 + ROWS_P1);                   /* the first row, then Done */
    keys[n++] = RIGHT;                                     /* Cancel */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 0);
}

/* Enter with the pointer on the page indicator turns the page */
static void test_nav_click_turns_the_page(void)
{
    Rig r;
    const int keys[] = { ENTER, ESCAPE };
    setup(&r);
    r.fake.pointer_in_nav = 1;
    run(&r, keys, 2);
    CHECK(shown(&r.fake, "< 2/3 >"));
}

/* port: PageDown turns the page, as the screen's own words promise (a click on the page indicator or PageDown turns
 * to the next). The screen answers the BIOS code 0x5100, which is what the port's page key is reported as; where
 * that code comes from -- a scripted PGDN read by input_get_key while the page keys are on -- is
 * test_input_page_keys's, and the screen's own caller turning the page keys on is menu_glue.c's. This rig hands
 * the screen the code itself, so it covers the screen's half of it. */
static void test_page_down_turns_the_page(void)
{
    Rig r;
    const int keys[] = { PAGE_DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK(shown(&r.fake, "< 2/3 >"));
}

/* port: page 1's Unlock Level still steps through its three values, under Unlocks & mods */
static void test_unlock_steps_through_three_values(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_UNLOCK);                 /* Unlock Level, below Lunar Springs Track */
    keys[n++] = RIGHT, keys[n++] = RIGHT;                  /* 0 to 2 */
    n = downs(keys, n, downs_to_done(ROW_UNLOCK));         /* Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved_unlock, 2);
}

/* Escape on page 1 puts the input back */
static void test_escape_on_page_one_restores(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_RIVAL);
    keys[n++] = SPACE, keys[n++] = ESCAPE;                 /* Warrior Rival Car on, then quit */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.warrior_rival, 0);
    CHECK_EQ_INT(r.fake.saves, 0);
}

/* Lunar Springs Track is the Unlocks row under Lost Vegas: Enter turns it on and Done keeps it */
static void test_lunar_row_is_saved_on_done(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_LUNAR);                  /* down to Lunar Springs Track */
    keys[n++] = ENTER;
    n = downs(keys, n, downs_to_done(ROW_LUNAR));
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.lunar_springs, 1);
    CHECK_EQ_INT(r.e.vegas, 0);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: 360 Camera, the last row of In-race options: off to start with, Enter turns it on and Done saves it (the
 * file the race reads it from), another Enter turns it off again */
static void test_follow_camera_row_toggles_and_saves(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_FOLLOW_CAMERA);
    keys[n++] = ENTER;
    n = downs(keys, n, downs_to_done(ROW_FOLLOW_CAMERA));
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROW_FOLLOW_CAMERA, EXTRAS_ROWS - 8);   /* before Difficulty, the last row of In-race options, and Miscellaneous' six */
    setup(&r);
    CHECK_EQ_INT(r.e.follow_camera, 0);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.follow_camera, 1);
    CHECK_EQ_INT(r.fake.saved.follow_camera, 1);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK(shown(&r.fake, "On"));

    Rig again;
    setup(&again);
    again.e.follow_camera = 1;
    run(&again, keys, n);
    CHECK_EQ_INT(again.e.follow_camera, 0);
    CHECK_EQ_INT(again.fake.saved.follow_camera, 0);
}

/* Escape puts 360 Camera back like the other rows */
static void test_follow_camera_row_escape_restores(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_FOLLOW_CAMERA);
    keys[n++] = SPACE, keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.follow_camera, 0);
    CHECK_EQ_INT(r.fake.saves, 0);
}

/* port: Difficulty, the last row of In-race options: Enter / Right step Easy -> Original -> Hard and round, Left goes
 * back, Done saves the number the race reads (0 Easy, 1 Original, 2 Hard), and Escape puts it back */
static void test_difficulty_row_cycles_saves_and_escapes(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_DIFFICULTY);
    keys[n++] = RIGHT;                                     /* Original -> Hard */
    n = downs(keys, n, downs_to_done(ROW_DIFFICULTY));
    keys[n++] = ENTER;
    setup(&r);
    r.e.difficulty = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.difficulty, 2);
    CHECK_EQ_INT(r.fake.saved.difficulty, 2);
    CHECK(shown(&r.fake, "Difficulty"));
    CHECK(shown(&r.fake, "Hard"));

    Rig wrap;                                              /* Hard -> Easy on Right, Easy -> Hard on Left */
    int k2[48] = {0};
    int m = keys_to_row(k2, ROW_DIFFICULTY);
    k2[m++] = RIGHT;
    m = downs(k2, m, downs_to_done(ROW_DIFFICULTY));
    k2[m++] = ENTER;
    setup(&wrap);
    wrap.e.difficulty = 2;
    run(&wrap, k2, m);
    CHECK_EQ_INT(wrap.e.difficulty, 0);
    CHECK(shown(&wrap.fake, "Easy"));
    k2[keys_to_row(k2, ROW_DIFFICULTY)] = LEFT;
    setup(&wrap);
    wrap.e.difficulty = 0;
    run(&wrap, k2, m);
    CHECK_EQ_INT(wrap.e.difficulty, 2);

    Rig esc;
    int k3[48] = {0};
    int q = keys_to_row(k3, ROW_DIFFICULTY);
    k3[q++] = RIGHT, k3[q++] = ESCAPE;
    setup(&esc);
    esc.e.difficulty = 1;
    run(&esc, k3, q);
    CHECK_EQ_INT(esc.e.difficulty, 1);
    CHECK_EQ_INT(esc.fake.saves, 0);
}

/* Done writes the on-disk file (saves++ in the rig) */
static void test_done_writes_extras_cfg(void)
{
    Rig r;
    /* the first DOWN selects the first row (Explode), so ROWS_P1 more land on Done. */
    int keys[48] = {0};
    int n = downs(keys, 0, 1 + ROWS_P1);
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.saved_unlock, 0);
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* Updates and anonymous stats: On by default, Right turns it Off and Done saves it */
static void test_check_for_updates_switch(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_UPDATE);
    keys[n++] = RIGHT;                                     /* off */
    n = downs(keys, n, downs_to_done(ROW_UPDATE)), keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    CHECK_EQ_INT(r.e.update_check, 1);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Updates and anonymous stats"));
    CHECK_EQ_INT(r.fake.saved.update_check, 0);
}

/* port (bug reports): Open the log folder is an action row, the last of page 3's Miscellaneous. Return runs the op once; the arrows do not; Done still saves */
static void test_extras_has_open_log_folder_row(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_LOG_FOLDER);
    keys[n++] = ENTER;
    keys[n++] = RIGHT, keys[n++] = LEFT;                     /* the arrows turn nothing on an action */
    n = downs(keys, n, downs_to_done(ROW_LOG_FOLDER));
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Open the log folder"));
    CHECK(shown(&r.fake, "Miscellaneous"));
    CHECK_EQ_INT(r.fake.log_opens, 1);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(ROW_RESET, EXTRAS_ROWS - 1);
}

/* without the op the row does nothing and the screen goes on */
static void test_the_log_folder_row_without_the_op(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_LOG_FOLDER);
    keys[n++] = ENTER;
    keys[n++] = DOWN, keys[n++] = ENTER;                     /* the Star row, with no op either */
    n = downs(keys, n, downs_to_done(ROW_STAR));
    keys[n++] = ENTER;
    setup(&r);
    r.fake.keys = keys;
    r.fake.key_count = n;
    ExtrasMenuOps ops = { .pointer_move_to = f_move, .load_art = f_art, .shape = f_shape, .palette_set = f_palette, .pointer_in_rect = f_in_rect, .select = f_select,
                          .blit_own = f_blit, .picture_shape = f_picture, .select_default = f_void, .pointer_hide = f_void, .flip = f_flip, .pointer_show = f_void,
                          .pointer_buttons = f_buttons, .text_styles = f_styles, .fade_in = f_fade_in, .deadline_set = f_one, .fade_out = f_fade_out,
                          .deadline_passed = f_zero, .sound = f_one, .ticks = f_ticks, .pointer_activate_if_moved = f_void, .key = f_key, .pointer_click = f_void,
                          .pointer_off = f_void, .music = f_void, .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel, .plus_bgnd = f_bgnd,
                          .plus_bgnd_shape = f_bgnd_shape, .plus_text_smooth = f_text_smooth, .plus_title = f_title, .plus_header = f_header,
                          .extras_save = f_save, .ctx = &r.fake };
    (void)extras_menu_run(&r.e, &r.unlock, 1, &ops);
    CHECK_EQ_INT(r.fake.log_opens, 0);
    CHECK_EQ_INT(r.fake.repo_opens, 0);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: Star us on GitHub is an action row, between Open the log folder and Restore profile defaults. Return runs the op once; the arrows do not; it shows no
 * value and Done still saves */
static void test_extras_has_a_star_us_row(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_STAR);
    keys[n++] = ENTER;
    keys[n++] = RIGHT, keys[n++] = LEFT;                     /* the arrows turn nothing on an action */
    n = downs(keys, n, downs_to_done(ROW_STAR));
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Star us on GitHub"));
    CHECK_EQ_INT(r.fake.repo_opens, 1);
    CHECK_EQ_INT(r.fake.log_opens, 0);
    CHECK_EQ_INT(r.fake.resets, 0);
    CHECK_EQ_INT(r.fake.saved.start, port_extras_defaults().start);   /* the arrows on it did not reach the Start Screen's value */
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(ROW_STAR, ROW_LOG_FOLDER + 1);
    CHECK_EQ_INT(ROW_RESET, ROW_STAR + 1);
}

int main(void)
{
    test_check_for_updates_switch();
    test_mirror_toggle_excludes_rally();
    test_the_radar_row_name();
    test_the_mode_holds_its_rows();
    test_restore_takes_two_returns();
    test_page_three_is_miscellaneous();
    test_extras_has_open_log_folder_row();
    test_the_log_folder_row_without_the_op();
    test_extras_has_a_star_us_row();
    test_escape_restores_input();
    test_right_turns_to_page_two();
    test_rally_excludes_mirror();
    test_warrior_row_is_fixed_by_the_level();
    test_warrior_row_toggles_at_level_zero();
    test_page_two_switches_saved();
    test_the_graphics_settings_are_gone();
    test_page_one_has_thirteen_rows();
    test_page_two_has_ten_rows();
    test_second_livery_row_is_saved_on_done();
    test_race_switch_rows_save_with_done();
    test_lunar_row_is_saved_on_done();
    test_follow_camera_row_toggles_and_saves();
    test_follow_camera_row_escape_restores();
    test_difficulty_row_cycles_saves_and_escapes();
    test_speed_units_row_reads_mph_in_english();
    test_speed_units_toggle_saves_kmh();
    test_intro_video_reads_the_other_way();
    test_left_on_page_two_done_goes_back();
    test_unlock_steps_through_three_values();
    test_up_from_cancel_reaches_the_last_switch();
    test_right_from_done_reaches_cancel();
    test_nav_click_turns_the_page();
    test_page_down_turns_the_page();
    test_escape_on_page_one_restores();
    test_done_writes_extras_cfg();
    return TEST_REPORT();
}

/* The port's own settings file (port.cfg): defaults, text round trip, parsing, loading and saving, and what a damaged file leaves alone. */
#include "port_settings_fixture.h"

static void test_defaults(void)
{
    const PortSettings d = port_settings_defaults();
    CHECK_EQ_INT(d.format, SCREEN_4_3);
    CHECK_EQ_INT(d.refresh_hz, 60);
    CHECK_EQ_INT(d.scale_integer, 0);
    CHECK_EQ_INT(d.fullscreen, 0);
    CHECK_EQ_INT(d.fov_adjust, 0);
    CHECK_EQ_INT(d.view_ultra, 0);
    CHECK_EQ_INT(d.perspective, 0);
    CHECK_EQ_INT(d.texture_filter, 0);
    CHECK_EQ_INT(d.fog, 0);
    CHECK_EQ_INT(d.car_colours, 0);     /* the exe's palette slots until asked */
    CHECK_EQ_INT(d.fog_sample, 0);      /* the fog from the art's foot until asked */
    CHECK_EQ_INT(d.replay_no_ffb, 0);   /* a replay shakes the stick, as the exe's, until asked */
    CHECK_EQ_INT(d.last_camera, 0);     /* the dash at the start, as the exe's, until asked */
    CHECK_EQ_INT(d.camera_kind, -1);    /* and no camera used yet */
    CHECK_EQ_INT(d.sky_fill, 0);        /* the exe's flat colour above the sky art until asked */
    CHECK_EQ_INT(d.rally_gravel_shoulder, 0);   /* the rally's crunch as it was, until asked */
    CHECK_EQ_INT(d.close_houses, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.brake_over_gas, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.gauge_light, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.finish_gate_light, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.rain_grip, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.rain_fx, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.vegas_lift, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.rain_lamps, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.gauge_exterior, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.sign_glow, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.rally_dry_lamps, 0);
    CHECK_EQ_INT(d.traffic_halo, 0);
    CHECK_EQ_INT(d.rain_v2, 0);   /* as it was, until asked */
    CHECK_EQ_INT(d.rain_tyre_roll, 0);     /* no wet-tyre roll until asked */
    CHECK_EQ_INT(d.snow_weather, 0);       /* the rain as it was until asked */
    CHECK_EQ_INT(d.reflectors, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.handbrake_lamps, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.rally_spray, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.snow_fx, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.lamp_halos, 0);
    CHECK_EQ_INT(d.snow_fx, 0);
    CHECK_EQ_INT(d.wipers, 0);
    CHECK_EQ_INT(d.menu_pad_cancel, 0);   /* the exe's pad, until asked */
    CHECK_EQ_INT(d.tunnel_lamps, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.wrong_way, 0);   /* the exe's: no warning */
    CHECK_EQ_INT(d.light_edit, 0);   /* off, until asked */
    CHECK_EQ_INT(d.weather_fog, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.spray_v4, 0);
    CHECK_EQ_INT(d.analog_pad, 0);   /* the exe's key paths, until a profile or the file asks */
    CHECK_EQ_INT(d.city_lift, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.reverse_rows, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.lamp_flares, 0);      /* no halo round the lit tail lights until asked */
    CHECK_EQ_INT(d.free_records, 0);    /* the free races keep no total for a lap count the exe lacks, until asked */
    CHECK_EQ_INT(d.render_bands, -1);   /* the render threads are Auto by default: as many as the logical cores */
    CHECK_EQ_INT(d.body_shine, 0);      /* the exe's frames until asked */
}

static void test_round_trip(void)
{
    const PortSettings s = { .format = SCREEN_21_9, .refresh_hz = 144, .scale_integer = 1, .fullscreen = 1,
                             .fov_adjust = -7, .view_ultra = 1, .perspective = 1, .texture_filter = 1, .fog = 1, .render_scale = 3, .laps = 25, .light_effects = 1, .car_soft_shadow = 1, .light_time = LIGHT_TIME_DUSK, .weather = WEATHER_RAIN, .render_bands = 4, .body_shine = 4, .free_records = 1, .car_colours = 1, .fog_sample = 1, .sky_fill = 1, .lamp_flares = 1, .replay_no_ffb = 1, .last_camera = 1, .camera_kind = 4 };
    char text[PORT_CFG_TEXT_MAX];
    const size_t n = port_settings_format(&s, text, sizeof text);
    CHECK(n > 0 && n < sizeof text);
    check_same(port_settings_parse(text, n), s);
}

static void test_parse_keys(void)
{
    const char *thirty = "refresh_hz=30\n", *bad_hz = "refresh_hz=45\n";
    CHECK_EQ_INT(port_settings_parse(thirty, strlen(thirty)).refresh_hz, 30);
    CHECK_EQ_INT(port_settings_parse(bad_hz, strlen(bad_hz)).refresh_hz, 60);
    const char *text = "format=21:9\nrefresh_hz=144\n";
    const PortSettings s = port_settings_parse(text, strlen(text));
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    CHECK_EQ_INT(s.refresh_hz, 144);
    CHECK_EQ_INT(s.fov_adjust, 0);   /* untouched keys keep the default */
    CHECK_EQ_INT(s.render_scale, 1);
    CHECK_EQ_INT(s.laps, 0);
    const char *many = "laps=99\n", *too_many = "laps=100\n", *negative = "laps=-1\n";
    CHECK_EQ_INT(port_settings_parse(many, strlen(many)).laps, 99);
    CHECK_EQ_INT(port_settings_parse(too_many, strlen(too_many)).laps, 0);
    CHECK_EQ_INT(port_settings_parse(negative, strlen(negative)).laps, 0);
    CHECK_EQ_INT(s.weather, WEATHER_CLEAR);
    const char *foggy = "weather=Fog\n", *stormy = "weather=storm\n";
    CHECK_EQ_INT(port_settings_parse(foggy, strlen(foggy)).weather, WEATHER_FOG);
    CHECK_EQ_INT(port_settings_parse(stormy, strlen(stormy)).weather, WEATHER_STORM);
    const char *scaled = "render_scale=2\n";
    CHECK_EQ_INT(port_settings_parse(scaled, strlen(scaled)).render_scale, 2);
    CHECK_EQ_INT(s.light_effects, 0);
    CHECK_EQ_INT(s.car_soft_shadow, 0);
    const char *shadow = "car_soft_shadow=1\n";
    CHECK_EQ_INT(port_settings_parse(shadow, strlen(shadow)).car_soft_shadow, 1);
    const char *bad_shadow = "car_soft_shadow=7\n";
    CHECK_EQ_INT(port_settings_parse(bad_shadow, strlen(bad_shadow)).car_soft_shadow, 0);
    const char *shine = "body_shine=1\n", *strong = "body_shine=3\n", *envmap = "body_shine=4\n", *bad_shine = "body_shine=5\n";
    CHECK_EQ_INT(port_settings_parse(shine, strlen(shine)).body_shine, 4);   /* off or the envmap: an older file's strengths 1..3 are the envmap */
    CHECK_EQ_INT(port_settings_parse(strong, strlen(strong)).body_shine, 4);
    CHECK_EQ_INT(port_settings_parse(envmap, strlen(envmap)).body_shine, 4);
    CHECK_EQ_INT(port_settings_parse("body_shine=2\n", 13).body_shine, 4);
    CHECK_EQ_INT(port_settings_parse("body_shine=0\n", 13).body_shine, 0);
    CHECK_EQ_INT(port_settings_parse(bad_shine, strlen(bad_shine)).body_shine, 0);
    const char *lit = "light_effects=1\n";
    CHECK_EQ_INT(port_settings_parse(lit, strlen(lit)).light_effects, 1);
    const char *bad = "light_effects=2\n";
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).light_effects, 0);
    CHECK_EQ_INT(s.light_time, LIGHT_TIME_DAY);
    const char *at = "light_time=A\n", *ct = "light_time=c\n", *bt = "light_time=day\n", *no = "light_time=noon\n";
    const char *dawn = "light_time=Dawn\n", *dusk = "light_time=DUSK\n", *b = "light_time=B\n";
    CHECK_EQ_INT(port_settings_parse(at, strlen(at)).light_time, LIGHT_TIME_DAWN);
    CHECK_EQ_INT(port_settings_parse(ct, strlen(ct)).light_time, LIGHT_TIME_DUSK);
    CHECK_EQ_INT(port_settings_parse(bt, strlen(bt)).light_time, LIGHT_TIME_DAY);
    CHECK_EQ_INT(port_settings_parse(b, strlen(b)).light_time, LIGHT_TIME_DAY);
    CHECK_EQ_INT(port_settings_parse(dawn, strlen(dawn)).light_time, LIGHT_TIME_DAWN);
    CHECK_EQ_INT(port_settings_parse(dusk, strlen(dusk)).light_time, LIGHT_TIME_DUSK);
    const char *night = "light_time=Night\n";
    CHECK_EQ_INT(port_settings_parse(night, strlen(night)).light_time, LIGHT_TIME_NIGHT);
    /* written as the words */
    char out[PORT_CFG_TEXT_MAX];
    PortSettings w = port_settings_defaults();
    w.light_time = LIGHT_TIME_DAWN;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "light_time=dawn\n") != NULL);
    w.light_time = LIGHT_TIME_DUSK;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "light_time=dusk\n") != NULL);
    w.light_time = LIGHT_TIME_NIGHT;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "light_time=night\n") != NULL);
    w.light_time = LIGHT_TIME_DAY;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "light_time=day\n") != NULL);
    CHECK_EQ_INT(port_settings_parse(no, strlen(no)).light_time, LIGHT_TIME_DAY);
}

static void test_bad_input_keeps_defaults(void)
{
    const char *text = "refresh_hz=75\nrender_scale=4\nfov_adjust=99\nformat=5:4\nno equals sign\nunknown=3\n";
    check_same(port_settings_parse(text, strlen(text)), port_settings_defaults());
    check_same(port_settings_parse("", 0), port_settings_defaults());
}

static void test_load_save(void)
{
    clear_files();
    put_file("port.cfg", "profile=restomod\n");   /* the mode that leaves everything free; port_profile's test has the others */
    PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.profile, PROFILE_RESTOMOD);
    s.format = SCREEN_21_9, s.refresh_hz = 120, s.scale_integer = 1, s.fullscreen = 0, s.fov_adjust = 15, s.view_ultra = 0, s.perspective = 0, s.texture_filter = 0, s.fog = 0, s.render_scale = 3, s.render_bands = 2;
    CHECK(port_settings_save(&s));
    check_same(port_settings_load(), s);
    clear_files();
}

static void test_a_longest_psx_dir_round_trips(void)
{
    PortSettings s = { .format = SCREEN_21_9, .refresh_hz = 144, .scale_integer = 1, .fullscreen = 1, .fov_adjust = -7, .view_ultra = 1,
                       .perspective = 1, .texture_filter = 1, .fog = 1, .render_scale = 3, .laps = 25, .light_effects = 1, .car_soft_shadow = 1,
                       .light_time = LIGHT_TIME_DUSK, .weather = WEATHER_RAIN, .render_bands = 4, .body_shine = 4, .free_records = 1, .car_colours = 1, .fog_sample = 1, .sky_fill = 1, .lamp_flares = 1, .rally_gravel_shoulder = 1, .snow_weather = 1, .lamp_halos = 1, .snow_fx = 1, .close_houses = 1, .brake_over_gas = 1, .reverse_rows = 1, .city_lift = 2, .vegas_lift = 1, .rain_fx = 1, .rain_grip = 1, .gauge_light = 3, .finish_gate_light = 1, .rain_lamps = 1, .gauge_exterior = 1, .sign_glow = 1, .reflectors = 1, .handbrake_lamps = 1, .rally_spray = 1, .wipers = 1, .weather_fog = 2, .spray_v4 = 1, .menu_pad_cancel = 1, .tunnel_lamps = 1, .wrong_way = 1 };
    memset(s.psx_dir, 'd', PSX_DIR_MAX - 1);
    s.psx_dir[0] = '/';
    s.psx_dir[PSX_DIR_MAX - 1] = '\0';
    char text[PORT_CFG_TEXT_MAX];
    const size_t n = port_settings_format(&s, text, sizeof text);
    CHECK(n > 0);
    CHECK(n > 480);   /* about the size of the older 512 buffer: one more key would not have fitted */
    const PortSettings back = port_settings_parse(text + 1, n);
    CHECK(strcmp(back.psx_dir, s.psx_dir) == 0);
    CHECK_EQ_INT(back.body_shine, 4);
    CHECK_EQ_INT(back.free_records, 1);
    CHECK_EQ_INT(back.car_colours, 1);
    CHECK_EQ_INT(back.fog_sample, 1);
    CHECK_EQ_INT(back.sky_fill, 1);
    CHECK_EQ_INT(back.lamp_flares, 1);
    CHECK_EQ_INT(back.rally_gravel_shoulder, 1);
    CHECK_EQ_INT(back.snow_weather, 1);
    CHECK_EQ_INT(back.close_houses, 1);
    CHECK_EQ_INT(back.rain_fx, 1);
    CHECK_EQ_INT(back.rain_grip, 1);
    CHECK_EQ_INT(back.gauge_light, 3);   /* a level 0..5 */
    CHECK_EQ_INT(back.finish_gate_light, 1);
    CHECK_EQ_INT(back.reverse_rows, 1);
    CHECK_EQ_INT(back.city_lift, 1);   /* a switch: the 2 written reads as 1 */
    CHECK_EQ_INT(back.vegas_lift, 1);
    CHECK_EQ_INT(back.rain_lamps, 1);
    CHECK_EQ_INT(back.gauge_exterior, 1);
    CHECK_EQ_INT(back.sign_glow, 1);
    CHECK_EQ_INT(back.reflectors, 1);
    CHECK_EQ_INT(back.handbrake_lamps, 1);
    CHECK_EQ_INT(back.rally_spray, 1);
    CHECK_EQ_INT(back.snow_fx, 1);
    CHECK_EQ_INT(back.lamp_halos, 1);
    CHECK_EQ_INT(back.wipers, 1);
    CHECK_EQ_INT(back.weather_fog, 1);   /* a switch: the 2 written reads as 1 */
    CHECK_EQ_INT(back.spray_v4, 1);
    CHECK_EQ_INT(back.menu_pad_cancel, 1);
    CHECK_EQ_INT(back.tunnel_lamps, 1);
    CHECK_EQ_INT(back.wrong_way, 1);
    CHECK_EQ_INT(back.brake_over_gas, 1);
    CHECK_EQ_INT(back.render_bands, 4);
    /* the old car_draw_distance line is ignored, and no longer written */
    const char *const old = "car_draw_distance=0\nfog_sample=1\n";
    CHECK_EQ_INT(port_settings_parse(old, strlen(old)).fog_sample, 1);
    CHECK(strstr(text, "car_draw_distance") == NULL);
}

/* two-sided scenery has no port.cfg key: a reversed track or the 360 camera turns it on (the `automatic` the race passes), TNFS_TWO_SIDED=0/1
 * is the dev override, and an old two_sided_scenery line is ignored and not written back */
static void test_two_sided_is_automatic(void)
{
    unsetenv("TNFS_TWO_SIDED");
    CHECK_EQ_INT(port_two_sided_scenery(0), 0);
    CHECK_EQ_INT(port_two_sided_scenery(1), 1);
    setenv("TNFS_TWO_SIDED", "0", 1);
    CHECK_EQ_INT(port_two_sided_scenery(1), 0);
    setenv("TNFS_TWO_SIDED", "1", 1);
    CHECK_EQ_INT(port_two_sided_scenery(0), 1);
    setenv("TNFS_TWO_SIDED", "x", 1);
    CHECK_EQ_INT(port_two_sided_scenery(1), 1);   /* not 0 or 1: the automatic one */
    unsetenv("TNFS_TWO_SIDED");
    const char *const old = "two_sided_scenery=1\nfog_sample=1\n";
    const PortSettings s = port_settings_parse(old, strlen(old));
    CHECK_EQ_INT(s.fog_sample, 1);
    char text[PORT_CFG_TEXT_MAX];
    CHECK(port_settings_format(&s, text, sizeof text) > 0);
    CHECK(strstr(text, "two_sided") == NULL);
}

/* the selectors that became switches: the top step an older file may hold (read as 1); `max` for any other key */
static int older_top(const char *key, int max)
{
    static const struct { const char *key; int top; } OLD[] = { { "weather_fog", 2 }, { "city_lift", 3 }, { "vegas_lift", 2 }, { "tunnel_lamps", 2 } };
    for (size_t i = 0; i < sizeof OLD / sizeof OLD[0]; i++)
        if (strcmp(key, OLD[i].key) == 0) return OLD[i].top;
    return max;
}

/* every key of the table of whole numbers: set to a value other than its default, written by port_settings_format and read back by port_settings_parse, it comes back; a value one past
 * its range is refused and the field stays what it was; no other field moves */
static void test_every_int_key_round_trips(void)
{
    int count = 0;
    const PortSettingsKey *keys = port_settings_int_keys(&count);
    CHECK(count > 50);
    const PortSettings defaults = port_settings_defaults();
    for (int i = 0; i < count; i++) {
        PortSettings s = port_settings_defaults();
        int *const field = (int *)(void *)((char *)&s + keys[i].offset);
        const int was = *field, want = keys[i].max != was ? keys[i].max : keys[i].min;
        *field = want;
        char text[PORT_CFG_TEXT_MAX + 1] = "\n", named[64];   /* the written text after a line end: a key is found by its whole name */
        const size_t n = port_settings_format(&s, text + 1, sizeof text - 1);
        snprintf(named, sizeof named, "\n%s=", keys[i].key);
        if (n == 0 || strstr(text, named) == NULL) {
            fprintf(stderr, "key %s is not written by port_settings_format\n", keys[i].key);
            CHECK(0);
            continue;
        }
        const PortSettings back = port_settings_parse(text, n);
        if (*(const int *)(const void *)((const char *)&back + keys[i].offset) != want) {
            fprintf(stderr, "key %s did not come back (%d)\n", keys[i].key, want);
            CHECK(0);
        }
        char over[96];   /* one past the top (a switch's: past the steps an older file held, which read as 1): refused */
        snprintf(over, sizeof over, "%s=%d\n", keys[i].key, older_top(keys[i].key, keys[i].max) + 1);
        const PortSettings kept = port_settings_parse(over, strlen(over));
        CHECK_EQ_INT(*(const int *)(const void *)((const char *)&kept + keys[i].offset), *(const int *)(const void *)((const char *)&defaults + keys[i].offset));
    }
}

/* The effects were called psx_* once: a port.cfg, a mode file or an extras.cfg that still says so loads the same values, and a save writes the new names; the old environment variables are read as well. */
static void test_old_psx_keys_load_as_the_new_names(void)
{
    const char *old = "profile=restomod\npsx_dir=/cd\npsx_light=1\npsx_shadow=1\npsx_time=dusk\npsx_headlights=1\npsx_flares=1\n";
    const char *now = "profile=restomod\npsx_dir=/cd\nlight_effects=1\ncar_soft_shadow=1\nlight_time=dusk\nheadlight_beams=1\nlamp_flares=1\n";
    const PortSettings a = port_settings_parse(old, strlen(old)), b = port_settings_parse(now, strlen(now));
    CHECK(a.light_effects == b.light_effects && a.car_soft_shadow == b.car_soft_shadow && a.light_time == b.light_time && a.lamp_flares == b.lamp_flares);   /* psx_headlights and headlight_beams: gone with the beams' fold, both ignored */
    CHECK_EQ_INT(a.light_effects, 1);
    CHECK_EQ_INT(a.car_soft_shadow, 1);
    CHECK_EQ_INT(a.light_time, LIGHT_TIME_DUSK);
    CHECK_EQ_INT(a.lamp_flares, 1);
    CHECK(strcmp(a.psx_dir, "/cd") == 0);   /* the folder of the CD keeps its name */
    char saved[PORT_CFG_TEXT_MAX];
    CHECK(port_settings_format(&a, saved, sizeof saved) > 0);
    CHECK(strstr(saved, "light_effects=1") != NULL && strstr(saved, "psx_light") == NULL && strstr(saved, "psx_time") == NULL && strstr(saved, "psx_dir=/cd") != NULL);
    const PortExtras e = port_extras_parse("psx_liveries=1\n", 15), f = port_extras_parse("extra_liveries=1\n", 17);
    CHECK(e.extra_liveries == 1 && f.extra_liveries == 1);
    PortSettings m = port_settings_defaults();   /* a mode's file read through the profile's reader: the old key is free where the new one is */
    port_profile_read_free(PROFILE_RESTOMOD, old, strlen(old), port_settings_take, &m);
    CHECK_EQ_INT(m.light_effects, 1);
    CHECK_EQ_INT(m.light_time, LIGHT_TIME_DUSK);
    unsetenv("TNFS_LIGHT_TIME"), setenv("TNFS_PSX_TIME", "night", 1);   /* the old variable still works, and the new one wins */
    CHECK_EQ_INT(port_light_time(), LIGHT_TIME_NIGHT);
    setenv("TNFS_LIGHT_TIME", "dawn", 1);
    CHECK_EQ_INT(port_light_time(), LIGHT_TIME_DAWN);
    unsetenv("TNFS_LIGHT_TIME"), unsetenv("TNFS_PSX_TIME");
}

int main(void)
{
    test_every_int_key_round_trips();
    clear_files();
    test_defaults();
    test_round_trip();
    test_parse_keys();
    test_bad_input_keeps_defaults();
    test_load_save();
    test_a_longest_psx_dir_round_trips();
    test_two_sided_is_automatic();
    test_old_psx_keys_load_as_the_new_names();
    return TEST_REPORT();
}

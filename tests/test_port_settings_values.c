/* The port's settings, one by one: what each value of a setting reads as, what a bad one leaves alone, and the environment over the file. (The fixture is port_settings_fixture.h.) */
#include "port_settings_fixture.h"

#include <stdio.h>

/* the render threads: the number of threads the bands use, 0 for off (the plain path) and -1 for "auto" (as many as
 * the logical cores, at most eight). The file names them "render_bands=0|auto|2|4". */
static void test_gauge_light_values(void)
{
    const char *car = "gauge_light=5\n", *over = "gauge_light=6\n", *four = "gauge_light=4\n";
    CHECK_EQ_INT(port_settings_parse(car, strlen(car)).gauge_light, 5);    /* "Del coche": the dash's own colours */
    CHECK_EQ_INT(port_settings_parse(four, strlen(four)).gauge_light, 4);
    CHECK_EQ_INT(port_settings_parse(over, strlen(over)).gauge_light, 0);  /* out of range: the default */
}

static void test_fog_sample_values(void)
{
    const char *const on = "fog_sample=1\n", *const bad = "fog_sample=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).fog_sample, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).fog_sample, 0);
}

/* lamp_flares: 0/1 in the file, TNFS_LAMP_FLARES over it, and only with the Light Effects switch on (the halo is the light effects') */
static void test_lamp_flares_values(void)
{
    const char *const on = "lamp_flares=1\n", *const bad = "lamp_flares=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).lamp_flares, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).lamp_flares, 0);
    setenv("TNFS_LIGHT_EFFECTS", "1", 1);
    setenv("TNFS_LAMP_FLARES", "1", 1);
    CHECK_EQ_INT(port_lamp_flares(), 1);
    setenv("TNFS_LIGHT_EFFECTS", "0", 1);
    CHECK_EQ_INT(port_lamp_flares(), 0);   /* no Light Effects, no halo */
    setenv("TNFS_LIGHT_EFFECTS", "1", 1);
    setenv("TNFS_LAMP_FLARES", "0", 1);
    CHECK_EQ_INT(port_lamp_flares(), 0);
    unsetenv("TNFS_LAMP_FLARES");
    unsetenv("TNFS_LIGHT_EFFECTS");
}

/* port (replay conditions): a replay's time of day and weather stand in for the settings' while forced; the variables still win, and unforced they are the settings' again */
static void test_conditions_force(void)
{
    unsetenv("TNFS_LIGHT_TIME"), unsetenv("TNFS_WEATHER");
    const int time = port_light_time(), weather = port_weather();
    port_conditions_force(1, LIGHT_TIME_NIGHT == time ? LIGHT_TIME_DAY : LIGHT_TIME_NIGHT, weather == 2 ? 0 : 2);
    CHECK(port_light_time() != time && port_weather() != weather);
    setenv("TNFS_LIGHT_TIME", "dusk", 1);
    CHECK_EQ_INT(port_light_time(), LIGHT_TIME_DUSK);   /* the variable wins */
    unsetenv("TNFS_LIGHT_TIME");
    port_conditions_force(0, 0, 0);
    CHECK(port_light_time() == time && port_weather() == weather);
}

/* reflectors: 0/1 in the file, TNFS_REFLECTORS over it, and only with the headlight beams on (the shine is the headlights') */
static void test_lamp_halos_values(void)
{
    const char *const on = "lamp_halos=1\n", *const bad = "lamp_halos=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).lamp_halos, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).lamp_halos, 0);
    setenv("TNFS_LAMP_HALOS", "1", 1);
    CHECK_EQ_INT(port_lamp_halos(), 1);
    setenv("TNFS_LAMP_HALOS", "0", 1);
    CHECK_EQ_INT(port_lamp_halos(), 0);
    unsetenv("TNFS_LAMP_HALOS");
}

static void test_wrong_way_values(void)
{
    const char *const on = "wrong_way=1\n", *const bad = "wrong_way=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).wrong_way, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).wrong_way, 0);
}

static void test_tunnel_lamps_values(void)
{
    const char *const on = "tunnel_lamps=1\n", *const day = "tunnel_lamps=2\n", *const bad = "tunnel_lamps=3\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).tunnel_lamps, 1);
    CHECK_EQ_INT(port_settings_parse(day, strlen(day)).tunnel_lamps, 1);   /* an older file's "by day too" is the switch's 1 */
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).tunnel_lamps, 0);
    setenv("TNFS_TUNNEL_LAMPS", "1", 1);
    CHECK_EQ_INT(port_tunnel_lamps(), 1);
    setenv("TNFS_TUNNEL_LAMPS", "2", 1);
    CHECK_EQ_INT(port_tunnel_lamps(), 1);
    setenv("TNFS_TUNNEL_LAMPS", "3", 1);
    CHECK_EQ_INT(port_tunnel_lamps(), 0);
    unsetenv("TNFS_TUNNEL_LAMPS");
}

static void test_menu_pad_cancel_values(void)
{
    const char *const on = "menu_pad_cancel=1\n", *const bad = "menu_pad_cancel=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).menu_pad_cancel, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).menu_pad_cancel, 0);
    setenv("TNFS_MENU_PAD_CANCEL", "1", 1);
    CHECK_EQ_INT(port_menu_pad_cancel(), 1);
    setenv("TNFS_MENU_PAD_CANCEL", "2", 1);
    CHECK_EQ_INT(port_menu_pad_cancel(), 0);
    unsetenv("TNFS_MENU_PAD_CANCEL");
}


/* the keys that are no longer settings (their effect is one with its parent's, or gone): an older cfg that still has them reads as if they were not there, the keys round them as written */
static void test_retired_keys_are_ignored(void)
{
    static const char *const RETIRED[] = { "tunnel_headlight", "water_mist", "trail_rise", "traffic_spray", "spray_cd_smoke", "headlight_relight", "made_tunnels", "hall_light",
                                           "car_tunnel_light", "canyon_shade", "parking_dark", "cavern_dark", "trans_shadows", "forest_neutral", "forest_dark", "vegas_car_night",
                                           "vegas_objects", "car_tunnel_match", "lamp_pools", "lamp_pool_tint", "lamp_pool_bright", "lamp_car_shade", "shoulder_light",
                                           "track_light_lamp", "night_stretch_clean", "night_sky_hue", "night_tone", "police_halo",
                                           "car_night_lift", "headlight_cone", "chase_height", "chase_steady" };
    const char *const kept = "light_effects=1\nheadlight_beams=1\nlaps=7\nweather_fog=2\n";
    char text[4096] = "light_effects=1\n";
    for (size_t i = 0; i < sizeof RETIRED / sizeof *RETIRED; i++) {
        char line[64];
        snprintf(line, sizeof line, "%s=%d\n", RETIRED[i], (int)(i % 2));
        strcat(text, line);
        if (i == 8) strcat(text, "headlight_beams=1\nlaps=7\n");   /* known keys between the old ones */
    }
    strcat(text, "weather_fog=2\n");
    check_same(port_settings_parse(text, strlen(text)), port_settings_parse(kept, strlen(kept)));
    CHECK_EQ_INT(port_settings_parse(text, strlen(text)).laps, 7);
}


static void test_weather_fog_values(void)
{
    const char *const on = "weather_fog=1\n", *const older = "weather_fog=2\n", *const bad = "weather_fog=3\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).weather_fog, 1);
    CHECK_EQ_INT(port_settings_parse(older, strlen(older)).weather_fog, 1);   /* an older file's 2 is the switch's 1 */
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).weather_fog, 0);
    setenv("TNFS_WEATHER_FOG", "1", 1);
    CHECK_EQ_INT(port_weather_fog(), 1);
    setenv("TNFS_WEATHER_FOG", "2", 1);
    CHECK_EQ_INT(port_weather_fog(), 1);
    setenv("TNFS_WEATHER_FOG", "0", 1);
    CHECK_EQ_INT(port_weather_fog(), 0);
    setenv("TNFS_WEATHER_FOG", "7", 1);   /* not a step: the file's */
    CHECK_EQ_INT(port_weather_fog(), 0);
    unsetenv("TNFS_WEATHER_FOG");
}

/* an older port.cfg with the selectors' higher steps and the keys folded away loads quietly: each selector is the switch's 1 (0 stays 0), the gone keys change nothing */
static void test_old_selector_file_loads_as_switches(void)
{
    const char *const old = "night_tone=1\ncity_lift=2\nvegas_lift=2\nweather_fog=1\ntunnel_lamps=1\npolice_halo=1\ncar_night_lift=2\nheadlight_cone=1\nchase_height=1\nchase_steady=0\nlaps=6\n";
    const char *const now = "city_lift=1\nvegas_lift=1\nweather_fog=1\ntunnel_lamps=1\nlaps=6\n";
    check_same(port_settings_parse(old, strlen(old)), port_settings_parse(now, strlen(now)));
    const char *const top = "city_lift=3\nvegas_lift=2\nweather_fog=2\ntunnel_lamps=2\n", *const off = "city_lift=0\nvegas_lift=0\nweather_fog=0\ntunnel_lamps=0\n";
    const PortSettings t = port_settings_parse(top, strlen(top)), o = port_settings_parse(off, strlen(off));
    CHECK(t.city_lift == 1 && t.vegas_lift == 1 && t.weather_fog == 1 && t.tunnel_lamps == 1);
    CHECK(o.city_lift == 0 && o.vegas_lift == 0 && o.weather_fog == 0 && o.tunnel_lamps == 0);
    setenv("TNFS_LIGHT_EFFECTS", "1", 1);   /* the lifts act under the light effects */
    setenv("TNFS_CITY_LIFT", "3", 1);
    CHECK_EQ_INT(port_city_lift(), 1);
    setenv("TNFS_VEGAS_LIFT", "2", 1);
    CHECK_EQ_INT(port_vegas_lift(), 1);
    unsetenv("TNFS_CITY_LIFT"), unsetenv("TNFS_VEGAS_LIFT"), unsetenv("TNFS_LIGHT_EFFECTS");
}

static void test_spray_v4_values(void)
{
    const char *const on = "spray_v4=1\n", *const bad = "spray_v4=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).spray_v4, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).spray_v4, 0);
    setenv("TNFS_SPRAY_V4", "1", 1);
    CHECK_EQ_INT(port_spray_v4(), 1);
    setenv("TNFS_SPRAY_V4", "0", 1);
    CHECK_EQ_INT(port_spray_v4(), 0);
    setenv("TNFS_SPRAY_V4", "5", 1);   /* not a switch: the file's */
    CHECK_EQ_INT(port_spray_v4(), 0);
    unsetenv("TNFS_SPRAY_V4");
}


static void test_wipers_values(void)
{
    const char *const on = "wipers=1\n", *const bad = "wipers=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).wipers, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).wipers, 0);
    setenv("TNFS_WIPERS", "1", 1);
    CHECK_EQ_INT(port_wipers(), 1);
    setenv("TNFS_WIPERS", "0", 1);
    CHECK_EQ_INT(port_wipers(), 0);
    unsetenv("TNFS_WIPERS");
}

static void test_snow_fx_values(void)
{
    const char *const on = "snow_fx=1\n", *const bad = "snow_fx=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).snow_fx, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).snow_fx, 0);
    setenv("TNFS_SNOW_FX", "1", 1);
    CHECK_EQ_INT(port_snow_fx(), 1);
    setenv("TNFS_SNOW_FX", "0", 1);
    CHECK_EQ_INT(port_snow_fx(), 0);
    unsetenv("TNFS_SNOW_FX");
}



static void test_rally_spray_values(void)
{
    const char *const on = "rally_spray=1\n", *const bad = "rally_spray=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).rally_spray, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).rally_spray, 0);
    setenv("TNFS_RALLY_SPRAY", "1", 1);
    CHECK_EQ_INT(port_rally_spray(), 1);
    setenv("TNFS_RALLY_SPRAY", "0", 1);
    CHECK_EQ_INT(port_rally_spray(), 0);
    unsetenv("TNFS_RALLY_SPRAY");
}

static void test_reflectors_values(void)
{
    const char *const on = "reflectors=1\n", *const bad = "reflectors=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).reflectors, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).reflectors, 0);
    setenv("TNFS_HEADLIGHT_BEAMS", "1", 1);
    setenv("TNFS_REFLECTORS", "1", 1);
    CHECK_EQ_INT(port_reflectors(), 1);
    setenv("TNFS_HEADLIGHT_BEAMS", "0", 1);
    CHECK_EQ_INT(port_reflectors(), 0);   /* no headlights, no reflectors */
    setenv("TNFS_HEADLIGHT_BEAMS", "1", 1);
    setenv("TNFS_REFLECTORS", "0", 1);
    CHECK_EQ_INT(port_reflectors(), 0);
    unsetenv("TNFS_REFLECTORS");
    unsetenv("TNFS_HEADLIGHT_BEAMS");
}

static void test_replay_no_ffb_values(void)
{
    const char *const on = "replay_no_ffb=1\n", *const bad = "replay_no_ffb=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).replay_no_ffb, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).replay_no_ffb, 0);
    setenv("TNFS_REPLAY_NO_FFB", "1", 1);
    CHECK_EQ_INT(port_replay_no_ffb(), 1);
    setenv("TNFS_REPLAY_NO_FFB", "0", 1);
    CHECK_EQ_INT(port_replay_no_ffb(), 0);
    unsetenv("TNFS_REPLAY_NO_FFB");
}

static void test_last_camera_values(void)
{
    const char *const ok = "last_camera=1\ncamera_kind=3\n", *const bad = "last_camera=2\ncamera_kind=6\n";
    CHECK_EQ_INT(port_settings_parse(ok, strlen(ok)).last_camera, 1);
    CHECK_EQ_INT(port_settings_parse(ok, strlen(ok)).camera_kind, 3);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).last_camera, 0);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).camera_kind, -1);   /* out of range: none */
    setenv("TNFS_LAST_CAMERA", "1", 1);
    CHECK_EQ_INT(port_last_camera(), 1);
    unsetenv("TNFS_LAST_CAMERA");
}

/* sky_fill_below: the flag is the exe's (0) until asked, and in 4:3 the exe's fill stays whatever the flag says */
static void test_sky_fill_below_values(void)
{
    CHECK_EQ_INT(port_settings_defaults().sky_fill_below, 0);
    const char *const on = "sky_fill_below=1\n", *const bad = "sky_fill_below=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).sky_fill_below, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).sky_fill_below, 0);
    PortSettings s = port_settings_defaults();
    s.sky_fill_below = 1;
    s.format = SCREEN_4_3;
    CHECK_EQ_INT(sky_fill_below_applies(&s), 0);   /* 4:3: the last pixel, as the exe's */
    s.format = SCREEN_16_9;
    CHECK_EQ_INT(sky_fill_below_applies(&s), 1);
    s.format = SCREEN_21_9;
    CHECK_EQ_INT(sky_fill_below_applies(&s), 1);
    s.sky_fill_below = 0;
    CHECK_EQ_INT(sky_fill_below_applies(&s), 0);
    char text[PORT_CFG_TEXT_MAX];
    s.sky_fill_below = 1;
    const size_t n = port_settings_format(&s, text, sizeof text);
    CHECK(n > 0 && strstr(text, "sky_fill_below=1\n") != NULL);
    CHECK_EQ_INT(port_settings_parse(text, n).sky_fill_below, 1);
}

static void test_police_glow_values(void)
{
    CHECK_EQ_INT(port_settings_defaults().police_glow, 0);
    const char *const on = "police_glow=1\n", *const bad = "police_glow=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).police_glow, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).police_glow, 0);
    setenv("TNFS_POLICE_GLOW", "1", 1);
    CHECK_EQ_INT(port_police_glow(), 1);
    unsetenv("TNFS_POLICE_GLOW");
}

static void test_gauge_wheel_values(void)
{
    CHECK_EQ_INT(port_settings_defaults().gauge_wheel, 0);
    const char *const on = "gauge_wheel=1\n", *const bad = "gauge_wheel=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).gauge_wheel, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).gauge_wheel, 0);
    setenv("TNFS_GAUGE_WHEEL", "1", 1);
    CHECK_EQ_INT(port_gauge_wheel(), 1);
    unsetenv("TNFS_GAUGE_WHEEL");
}

static void test_tach_dim_values(void)
{
    CHECK_EQ_INT(port_settings_defaults().tach_dim, 0);
    const char *const on = "tach_dim=1\n", *const bad = "tach_dim=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).tach_dim, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).tach_dim, 0);
    setenv("TNFS_TACH_DIM", "1", 1);
    CHECK_EQ_INT(port_tach_dim(), 1);
    unsetenv("TNFS_TACH_DIM");
}

static void test_radar_all_views_values(void)
{
    CHECK_EQ_INT(port_settings_defaults().radar_all_views, 0);
    const char *const on = "radar_all_views=1\n", *const bad = "radar_all_views=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).radar_all_views, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).radar_all_views, 0);
    setenv("TNFS_RADAR_ALL_VIEWS", "1", 1);
    CHECK_EQ_INT(port_radar_all_views(), 1);
    unsetenv("TNFS_RADAR_ALL_VIEWS");
}

static void test_sky_fill_values(void)
{
    unsetenv("TNFS_SKY_FILL_MODE");
    CHECK_EQ_INT(port_sky_fill_mode(), SKY_FILL_MODE_DEFAULT);
    setenv("TNFS_SKY_FILL_MODE", "a", 1);
    CHECK_EQ_INT(port_sky_fill_mode(), 1);
    setenv("TNFS_SKY_FILL_MODE", "2", 1);
    CHECK_EQ_INT(port_sky_fill_mode(), 2);
    setenv("TNFS_SKY_FILL_MODE", "x", 1);
    CHECK_EQ_INT(port_sky_fill_mode(), SKY_FILL_MODE_DEFAULT);
    unsetenv("TNFS_SKY_FILL_MODE");
    const char *const on = "sky_fill=1\n", *const bad = "sky_fill=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).sky_fill, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).sky_fill, 0);
}

static void test_free_records_values(void)
{
    const char *const on = "free_records=1\n", *const bad = "free_records=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).free_records, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).free_records, 0);
}

static void test_render_bands_values(void)
{
    char text[PORT_CFG_TEXT_MAX];
    const char *const text_in = "render_bands=auto\n";
    PortSettings s = port_settings_parse(text_in, strlen(text_in));
    CHECK_EQ_INT(s.render_bands, -1);   /* "auto" is the sentinel, not a count */

    const char *const two = "render_bands=2\n";
    s = port_settings_parse(two, strlen(two));
    CHECK_EQ_INT(s.render_bands, 2);

    const char *const four = "render_bands=4\n";
    s = port_settings_parse(four, strlen(four));
    CHECK_EQ_INT(s.render_bands, 4);

    const char *const off = "render_bands=0\n";
    s = port_settings_parse(off, strlen(off));
    CHECK_EQ_INT(s.render_bands, 0);

    /* the written form: "auto" for the sentinel, the count for a number */
    PortSettings a = port_settings_defaults();
    a.render_bands = -1;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_bands=auto\n") != NULL);
    a.render_bands = 4;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_bands=4\n") != NULL);
    a.render_bands = 0;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_bands=0\n") != NULL);
}

/* a damaged value keeps the default, like every other key: the render threads stay on Auto */
static void test_bad_render_bands_keeps_the_default(void)
{
    static const char *const bad[] = { "render_bands=9\n", "render_bands=-2\n", "render_bands=two\n",
                                       "render_bands=Auto\n", "render_bands=\n" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        const PortSettings s = port_settings_parse(bad[i], strlen(bad[i]));
        CHECK_EQ_INT(s.render_bands, -1);
    }
}

/* the render scale: the many times the world is drawn over the frame's size, 1, 2 or 3. The file names them
 * "render_scale=1|2|3". */
static void test_render_scale_values(void)
{
    static const char *const good[] = { "render_scale=1\n", "render_scale=2\n", "render_scale=3\n" };
    for (int i = 0; i < 3; i++) {
        const PortSettings s = port_settings_parse(good[i], strlen(good[i]));
        CHECK_EQ_INT(s.render_scale, i + 1);
    }
    /* the written form: the number on its own */
    char text[PORT_CFG_TEXT_MAX];
    PortSettings a = port_settings_defaults();
    a.render_scale = 3;
    CHECK(port_settings_format(&a, text, sizeof text) > 0);
    CHECK(strstr(text, "render_scale=3\n") != NULL);
}

/* a damaged value keeps the default, like every other key: the render scale stays at 1x */
static void test_bad_render_scale_keeps_the_default(void)
{
    static const char *const bad[] = { "render_scale=4\n", "render_scale=0\n", "render_scale=-1\n",
                                       "render_scale=two\n", "render_scale=\n" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        const PortSettings s = port_settings_parse(bad[i], strlen(bad[i]));
        CHECK_EQ_INT(s.render_scale, 1);
    }
}

/* the environment wins over the file: TNFS_RENDER_SCALE (1..3) when set, else port.cfg's render_scale. It is what
 * the checks run with, so a file that says 1 must not stop the environment from saying 3. */
static void test_the_environment_wins_over_the_setting(void)
{
    unsetenv("TNFS_RENDER_SCALE");
    CHECK_EQ_INT(port_render_scale(), 1);   /* the default, with no file and no environment */
    setenv("TNFS_RENDER_SCALE", "3", 1);
    CHECK_EQ_INT(port_render_scale(), 3);
    setenv("TNFS_RENDER_SCALE", "", 1);
    CHECK_EQ_INT(port_render_scale(), 1);   /* an empty value is not a setting */
    setenv("TNFS_RENDER_SCALE", "9", 1);
    CHECK_EQ_INT(port_render_scale(), 1);   /* nor is one outside the range */
    unsetenv("TNFS_RENDER_SCALE");
    CHECK_EQ_INT(port_render_scale(), 1);
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* a psx_dir of the longest length kept with every other key set: the text the file is written from is about 270 bytes past it, which the save's
 * buffer holds (1024) and the older 512 did not */
static void test_emissive_lamps_follow_light_effects(void)
{
    unsetenv("TNFS_EMISSIVE_LAMPS");
    setenv("TNFS_LIGHT_EFFECTS", "1", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 1);   /* the Light Effects switch on */
    setenv("TNFS_LIGHT_EFFECTS", "0", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 0);   /* off: the exe's */
    setenv("TNFS_LIGHT_EFFECTS", "1", 1);
    setenv("TNFS_EMISSIVE_LAMPS", "0", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 0);   /* the dev override */
    setenv("TNFS_LIGHT_EFFECTS", "0", 1);
    setenv("TNFS_EMISSIVE_LAMPS", "1", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 1);
    unsetenv("TNFS_EMISSIVE_LAMPS");
    unsetenv("TNFS_LIGHT_EFFECTS");
}

int main(void)
{
    clear_files();
    test_render_bands_values();
    test_free_records_values();
    test_gauge_light_values();
    test_fog_sample_values();
    test_sky_fill_values();
    test_replay_no_ffb_values();
    test_last_camera_values();
    test_sky_fill_below_values();
    test_radar_all_views_values();
    test_tach_dim_values();
    test_gauge_wheel_values();
    test_police_glow_values();
    test_lamp_flares_values();
    test_conditions_force();
    test_reflectors_values();
    test_rally_spray_values();
    test_snow_fx_values();
    test_lamp_halos_values();
    test_wipers_values();
    test_weather_fog_values();
    test_retired_keys_are_ignored();
    test_old_selector_file_loads_as_switches();
    test_spray_v4_values();
    test_menu_pad_cancel_values();
    test_tunnel_lamps_values();
    test_wrong_way_values();
    test_emissive_lamps_follow_light_effects();
    test_bad_render_bands_keeps_the_default();
    test_render_scale_values();
    test_bad_render_scale_keeps_the_default();
    test_the_environment_wins_over_the_setting();
    return TEST_REPORT();
}

/* The port's own settings file (port.cfg): defaults, text round trip, and what a damaged file leaves alone. */
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"
#include "game/race/weather.h"

#include <stdlib.h>
#include <string.h>

/* Two in-memory user files stand in for the platform's storage: port.cfg is the one port_settings writes, and
 * extras.cfg is only read, as the fallback for the three settings that moved in from there. */
typedef struct {
    char name[32];
    uint8_t bytes[PORT_CFG_TEXT_MAX];
    size_t size;
    int present;
} Stored;

static Stored stored[2];

/* the slot holding `name`, or the first free one; a second file takes stored[1] rather than overwriting the first */
static Stored *slot_for(const char *name)
{
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] != '\0' && strcmp(stored[i].name, name) == 0) return &stored[i];
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] == '\0') {
            strcpy(stored[i].name, name);
            return &stored[i];
        }
    strcpy(stored[0].name, name);
    return &stored[0];
}

static void put_file(const char *name, const char *text)
{
    Stored *s = slot_for(name);
    snprintf((char *)s->bytes, sizeof s->bytes, "%s", text);
    s->size = strlen(text);
    s->present = 1;
}

static void clear_files(void)
{
    for (int i = 0; i < 2; i++) {
        stored[i].name[0] = '\0';
        stored[i].size = 0;
        stored[i].present = 0;
    }
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    for (int i = 0; i < 2; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) {
            uint8_t *copy = malloc(stored[i].size);
            memcpy(copy, stored[i].bytes, stored[i].size);
            *size = stored[i].size;
            return copy;
        }
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    Stored *s = slot_for(name);
    if (size > sizeof s->bytes)
        return false;
    memcpy(s->bytes, data, size);
    s->size = size;
    s->present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static void check_same(PortSettings a, PortSettings b)
{
    CHECK_EQ_INT(a.format, b.format);
    CHECK_EQ_INT(a.refresh_hz, b.refresh_hz);
    CHECK_EQ_INT(a.scale_integer, b.scale_integer);
    CHECK_EQ_INT(a.fullscreen, b.fullscreen);
    CHECK_EQ_INT(a.fov_adjust, b.fov_adjust);
    CHECK_EQ_INT(a.view_ultra, b.view_ultra);
    CHECK_EQ_INT(a.perspective, b.perspective);
    CHECK_EQ_INT(a.texture_filter, b.texture_filter);
    CHECK_EQ_INT(a.fog, b.fog);
    CHECK_EQ_INT(a.psx_light, b.psx_light);
    CHECK_EQ_INT(a.psx_shadow, b.psx_shadow);
    CHECK_EQ_INT(a.psx_headlights, b.psx_headlights);
    CHECK_EQ_INT(a.psx_time, b.psx_time);
    CHECK_EQ_INT(a.render_scale, b.render_scale);
    CHECK_EQ_INT(a.laps, b.laps);
    CHECK_EQ_INT(a.weather, b.weather);
    CHECK_EQ_INT(a.render_bands, b.render_bands);
    CHECK_EQ_INT(a.body_shine, b.body_shine);
    CHECK_EQ_INT(a.free_records, b.free_records);
    CHECK_EQ_INT(a.car_colours, b.car_colours);
    CHECK_EQ_INT(a.fog_sample, b.fog_sample);
    CHECK_EQ_INT(a.sky_fill, b.sky_fill);
    CHECK_EQ_INT(a.night_sky_hue, b.night_sky_hue);
    CHECK_EQ_INT(a.forest_dark, b.forest_dark);
    CHECK_EQ_INT(a.night_stretch_clean, b.night_stretch_clean);
    CHECK_EQ_INT(a.replay_no_ffb, b.replay_no_ffb);
    CHECK_EQ_INT(a.last_camera, b.last_camera);
    CHECK_EQ_INT(a.camera_kind, b.camera_kind);
    CHECK_EQ_INT(a.psx_flares, b.psx_flares);
    CHECK_EQ_INT(a.hall_light, b.hall_light);
    CHECK_EQ_INT(a.made_tunnels, b.made_tunnels);
    CHECK_EQ_INT(a.headlight_relight, b.headlight_relight);
    CHECK_EQ_INT(a.car_tunnel_light, b.car_tunnel_light);
    CHECK_EQ_INT(a.vegas_car_night, b.vegas_car_night);
    CHECK_EQ_INT(a.rally_gravel_shoulder, b.rally_gravel_shoulder);
    CHECK_EQ_INT(a.chase_height, b.chase_height);
    CHECK_EQ_INT(a.lamp_pool_tint, b.lamp_pool_tint);
    CHECK_EQ_INT(a.trans_shadows, b.trans_shadows);
    CHECK_EQ_INT(a.canyon_shade, b.canyon_shade);
    CHECK_EQ_INT(a.tunnel_headlight, b.tunnel_headlight);
    CHECK_EQ_INT(a.parking_dark, b.parking_dark);
    CHECK_EQ_INT(a.forest_neutral, b.forest_neutral);
}

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
    CHECK_EQ_INT(d.night_sky_hue, 0);   /* the night's sky as it was until asked */
    CHECK_EQ_INT(d.forest_dark, 0);     /* the made light's forest as it was until asked */
    CHECK_EQ_INT(d.night_stretch_clean, 0);   /* the stretched night sky as it was until asked */
    CHECK_EQ_INT(d.car_tunnel_light, 0);   /* the car's light as it was, until asked */
    CHECK_EQ_INT(d.vegas_car_night, 0);   /* Lost Vegas' night tint as it was, until asked */
    CHECK_EQ_INT(d.rally_gravel_shoulder, 0);   /* the rally's crunch as it was, until asked */
    CHECK_EQ_INT(d.chase_height, 0);   /* the exe's slope lift, until asked */
    CHECK_EQ_INT(d.lamp_pool_tint, 0);   /* the pools only lift the light, until asked */
    CHECK_EQ_INT(d.forest_neutral, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.parking_dark, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.tunnel_headlight, 0);   /* the beam as the exe has it, until asked */
    CHECK_EQ_INT(d.canyon_shade, 0);   /* the canyon in full sun, until asked */
    CHECK_EQ_INT(d.trans_shadows, 0);   /* Transtropolis' made light as it was, until asked */
    CHECK_EQ_INT(d.headlight_relight, 0);   /* the road's light as built, until asked */
    CHECK_EQ_INT(d.made_tunnels, 0);    /* a made light keeps no tunnel flags until asked */
    CHECK_EQ_INT(d.psx_flares, 0);      /* no halo round the lit tail lights until asked */
    CHECK_EQ_INT(d.hall_light, 0);      /* the hall is a tunnel to the car until asked */
    CHECK_EQ_INT(d.free_records, 0);    /* the free races keep no total for a lap count the exe lacks, until asked */
    CHECK_EQ_INT(d.render_bands, -1);   /* the render threads are Auto by default: as many as the logical cores */
    CHECK_EQ_INT(d.body_shine, 0);      /* the exe's frames until asked */
}

static void test_round_trip(void)
{
    const PortSettings s = { .format = SCREEN_21_9, .refresh_hz = 144, .scale_integer = 1, .fullscreen = 1,
                             .fov_adjust = -7, .view_ultra = 1, .perspective = 1, .texture_filter = 1, .fog = 1, .render_scale = 3, .laps = 25, .psx_light = 1, .psx_shadow = 1, .psx_headlights = 1, .psx_time = PSX_TIME_DUSK, .weather = WEATHER_RAIN, .render_bands = 4, .body_shine = 4, .free_records = 1, .car_colours = 1, .fog_sample = 1, .sky_fill = 1, .night_sky_hue = 1, .psx_flares = 1, .hall_light = 1, .forest_dark = 1, .night_stretch_clean = 1, .made_tunnels = 1, .headlight_relight = 1, .replay_no_ffb = 1, .last_camera = 1, .camera_kind = 4 };
    char text[PORT_CFG_TEXT_MAX];
    const size_t n = port_settings_format(&s, text, sizeof text);
    CHECK(n > 0 && n < sizeof text);
    check_same(port_settings_parse(text, n), s);
}

/* the render threads: the number of threads the bands use, 0 for off (the plain path) and -1 for "auto" (as many as
 * the logical cores, at most eight). The file names them "render_bands=0|auto|2|4". */
static void test_fog_sample_values(void)
{
    const char *const on = "fog_sample=1\n", *const bad = "fog_sample=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).fog_sample, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).fog_sample, 0);
}

/* psx_flares: 0/1 in the file, TNFS_PSX_FLARES over it, and only with the Light Effects switch on (the halo is the PS1 look's) */
static void test_psx_flares_values(void)
{
    const char *const on = "psx_flares=1\n", *const bad = "psx_flares=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).psx_flares, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).psx_flares, 0);
    setenv("TNFS_PSX_LIGHT", "1", 1);
    setenv("TNFS_PSX_FLARES", "1", 1);
    CHECK_EQ_INT(port_psx_flares(), 1);
    setenv("TNFS_PSX_LIGHT", "0", 1);
    CHECK_EQ_INT(port_psx_flares(), 0);   /* no Light Effects, no halo */
    setenv("TNFS_PSX_LIGHT", "1", 1);
    setenv("TNFS_PSX_FLARES", "0", 1);
    CHECK_EQ_INT(port_psx_flares(), 0);
    unsetenv("TNFS_PSX_FLARES");
    unsetenv("TNFS_PSX_LIGHT");
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
    CHECK_EQ_INT(port_settings_parse(stormy, strlen(stormy)).weather, WEATHER_CLEAR);
    const char *scaled = "render_scale=2\n";
    CHECK_EQ_INT(port_settings_parse(scaled, strlen(scaled)).render_scale, 2);
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.psx_shadow, 0);
    const char *shadow = "psx_shadow=1\n";
    CHECK_EQ_INT(port_settings_parse(shadow, strlen(shadow)).psx_shadow, 1);
    CHECK_EQ_INT(s.psx_headlights, 0);   /* off by default: the night's first look */
    const char *headlights = "psx_headlights=1\n";
    CHECK_EQ_INT(port_settings_parse(headlights, strlen(headlights)).psx_headlights, 1);
    const char *bad_headlights = "psx_headlights=2\n";
    CHECK_EQ_INT(port_settings_parse(bad_headlights, strlen(bad_headlights)).psx_headlights, 0);
    const char *bad_shadow = "psx_shadow=7\n";
    CHECK_EQ_INT(port_settings_parse(bad_shadow, strlen(bad_shadow)).psx_shadow, 0);
    const char *shine = "body_shine=1\n", *strong = "body_shine=3\n", *envmap = "body_shine=4\n", *bad_shine = "body_shine=5\n";
    CHECK_EQ_INT(port_settings_parse(shine, strlen(shine)).body_shine, 4);   /* off or the envmap: an older file's strengths 1..3 are the envmap */
    CHECK_EQ_INT(port_settings_parse(strong, strlen(strong)).body_shine, 4);
    CHECK_EQ_INT(port_settings_parse(envmap, strlen(envmap)).body_shine, 4);
    CHECK_EQ_INT(port_settings_parse("body_shine=2\n", 13).body_shine, 4);
    CHECK_EQ_INT(port_settings_parse("body_shine=0\n", 13).body_shine, 0);
    CHECK_EQ_INT(port_settings_parse(bad_shine, strlen(bad_shine)).body_shine, 0);
    const char *lit = "psx_light=1\n";
    CHECK_EQ_INT(port_settings_parse(lit, strlen(lit)).psx_light, 1);
    const char *bad = "psx_light=2\n";
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).psx_light, 0);
    CHECK_EQ_INT(s.psx_time, PSX_TIME_DAY);
    const char *at = "psx_time=A\n", *ct = "psx_time=c\n", *bt = "psx_time=day\n", *no = "psx_time=noon\n";
    const char *dawn = "psx_time=Dawn\n", *dusk = "psx_time=DUSK\n", *b = "psx_time=B\n";
    CHECK_EQ_INT(port_settings_parse(at, strlen(at)).psx_time, PSX_TIME_DAWN);
    CHECK_EQ_INT(port_settings_parse(ct, strlen(ct)).psx_time, PSX_TIME_DUSK);
    CHECK_EQ_INT(port_settings_parse(bt, strlen(bt)).psx_time, PSX_TIME_DAY);
    CHECK_EQ_INT(port_settings_parse(b, strlen(b)).psx_time, PSX_TIME_DAY);
    CHECK_EQ_INT(port_settings_parse(dawn, strlen(dawn)).psx_time, PSX_TIME_DAWN);
    CHECK_EQ_INT(port_settings_parse(dusk, strlen(dusk)).psx_time, PSX_TIME_DUSK);
    const char *night = "psx_time=Night\n";
    CHECK_EQ_INT(port_settings_parse(night, strlen(night)).psx_time, PSX_TIME_NIGHT);
    /* written as the words */
    char out[PORT_CFG_TEXT_MAX];
    PortSettings w = port_settings_defaults();
    w.psx_time = PSX_TIME_DAWN;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "psx_time=dawn\n") != NULL);
    w.psx_time = PSX_TIME_DUSK;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "psx_time=dusk\n") != NULL);
    w.psx_time = PSX_TIME_NIGHT;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "psx_time=night\n") != NULL);
    w.psx_time = PSX_TIME_DAY;
    CHECK(port_settings_format(&w, out, sizeof out) > 0 && strstr(out, "psx_time=day\n") != NULL);
    CHECK_EQ_INT(port_settings_parse(no, strlen(no)).psx_time, PSX_TIME_DAY);
}

static void test_bad_input_keeps_defaults(void)
{
    const char *text = "refresh_hz=75\nrender_scale=4\nfov_adjust=99\nformat=5:4\nno equals sign\nunknown=3\n";
    check_same(port_settings_parse(text, strlen(text)), port_settings_defaults());
    check_same(port_settings_parse("", 0), port_settings_defaults());
}

static void test_load_save(void)
{
    check_same(port_settings_load(), port_settings_defaults());   /* no file yet */
    const PortSettings s = { .format = SCREEN_16_9, .refresh_hz = 120, .scale_integer = 1, .fullscreen = 1,
                             .fov_adjust = 15, .view_ultra = 1, .perspective = 1, .texture_filter = 1, .fog = 1,
                             .render_scale = 2, .render_bands = 2 };
    CHECK(port_settings_save(&s));
    check_same(port_settings_load(), s);
}

/* The three settings that moved in from EXTRAS (perspective correction, texture filter, fog) live in port.cfg now,
 * but an extras.cfg written before the move keeps its value: only when port.cfg has no key for one does its value
 * come from extras.cfg. A key in port.cfg always wins, so a save from Graphics is not undone by a stale extras.cfg. */
static void test_moved_settings_fall_back_to_extras(void)
{
    clear_files();

    /* neither file: the defaults */
    check_same(port_settings_load(), port_settings_defaults());

    /* extras.cfg alone: its three values are taken */
    put_file("extras.cfg", "modern_3d=1\ntexture_filter=1\nfog=0\n");
    PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 1);
    CHECK_EQ_INT(s.texture_filter, 1);
    CHECK_EQ_INT(s.fog, 0);

    /* port.cfg names them: it wins over extras.cfg */
    put_file("port.cfg", "perspective=0\ntexture_filter=0\nfog=1\n");
    s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 0);
    CHECK_EQ_INT(s.texture_filter, 0);
    CHECK_EQ_INT(s.fog, 1);

    /* port.cfg names one and not the others: the named one wins, the rest still come from extras.cfg */
    put_file("port.cfg", "fog=1\n");
    s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 1);   /* from extras.cfg */
    CHECK_EQ_INT(s.texture_filter, 1);   /* from extras.cfg */
    CHECK_EQ_INT(s.fog, 1);              /* from port.cfg */

    /* saving from Graphics writes all three to port.cfg, and they survive the fallback afterwards */
    s.perspective = 0;
    s.texture_filter = 0;
    s.fog = 0;
    CHECK(port_settings_save(&s));
    /* the saved text carries all three, so a later load never has to consult extras.cfg for them */
    char text[PORT_CFG_TEXT_MAX] = "";
    CHECK(port_settings_format(&s, text, sizeof text) > 0);
    CHECK(strstr(text, "perspective=") != NULL);
    CHECK(strstr(text, "texture_filter=") != NULL);
    CHECK(strstr(text, "fog=") != NULL);
    s = port_settings_load();
    CHECK_EQ_INT(s.perspective, 0);
    CHECK_EQ_INT(s.texture_filter, 0);
    CHECK_EQ_INT(s.fog, 0);

    clear_files();
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* a psx_dir of the longest length kept with every other key set: the text the file is written from is about 270 bytes past it, which the save's
 * buffer holds (1024) and the older 512 did not */
static void test_emissive_lamps_follow_light_effects(void)
{
    unsetenv("TNFS_EMISSIVE_LAMPS");
    setenv("TNFS_PSX_LIGHT", "1", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 1);   /* the Light Effects switch on */
    setenv("TNFS_PSX_LIGHT", "0", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 0);   /* off: the exe's */
    setenv("TNFS_PSX_LIGHT", "1", 1);
    setenv("TNFS_EMISSIVE_LAMPS", "0", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 0);   /* the dev override */
    setenv("TNFS_PSX_LIGHT", "0", 1);
    setenv("TNFS_EMISSIVE_LAMPS", "1", 1);
    CHECK_EQ_INT(port_emissive_lamps(), 1);
    unsetenv("TNFS_EMISSIVE_LAMPS");
    unsetenv("TNFS_PSX_LIGHT");
}

static void test_a_longest_psx_dir_round_trips(void)
{
    PortSettings s = { .format = SCREEN_21_9, .refresh_hz = 144, .scale_integer = 1, .fullscreen = 1, .fov_adjust = -7, .view_ultra = 1,
                       .perspective = 1, .texture_filter = 1, .fog = 1, .render_scale = 3, .laps = 25, .psx_light = 1, .psx_shadow = 1,
                       .psx_time = PSX_TIME_DUSK, .weather = WEATHER_RAIN, .render_bands = 4, .body_shine = 4, .free_records = 1, .car_colours = 1, .fog_sample = 1, .sky_fill = 1, .psx_flares = 1, .made_tunnels = 1, .headlight_relight = 1, .chase_height = 1, .rally_gravel_shoulder = 1, .car_tunnel_light = 1, .vegas_car_night = 1, .lamp_pool_tint = 1, .trans_shadows = 1, .canyon_shade = 1, .tunnel_headlight = 1, .parking_dark = 1, .forest_neutral = 1 };
    memset(s.psx_dir, 'd', PSX_DIR_MAX - 1);
    s.psx_dir[0] = '/';
    s.psx_dir[PSX_DIR_MAX - 1] = '\0';
    char text[1024];
    const size_t n = port_settings_format(&s, text, sizeof text);
    CHECK(n > 0);
    CHECK(n > 480);   /* about the size of the older 512 buffer: one more key would not have fitted */
    const PortSettings back = port_settings_parse(text, n);
    CHECK(strcmp(back.psx_dir, s.psx_dir) == 0);
    CHECK_EQ_INT(back.body_shine, 4);
    CHECK_EQ_INT(back.free_records, 1);
    CHECK_EQ_INT(back.car_colours, 1);
    CHECK_EQ_INT(back.fog_sample, 1);
    CHECK_EQ_INT(back.sky_fill, 1);
    CHECK_EQ_INT(back.psx_flares, 1);
    CHECK_EQ_INT(back.made_tunnels, 1);
    CHECK_EQ_INT(back.headlight_relight, 1);
    CHECK_EQ_INT(back.car_tunnel_light, 1);
    CHECK_EQ_INT(back.vegas_car_night, 1);
    CHECK_EQ_INT(back.rally_gravel_shoulder, 1);
    CHECK_EQ_INT(back.chase_height, 1);
    CHECK_EQ_INT(back.lamp_pool_tint, 1);
    CHECK_EQ_INT(back.trans_shadows, 1);
    CHECK_EQ_INT(back.canyon_shade, 1);
    CHECK_EQ_INT(back.tunnel_headlight, 1);
    CHECK_EQ_INT(back.parking_dark, 1);
    CHECK_EQ_INT(back.forest_neutral, 1);
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
    char text[1024];
    CHECK(port_settings_format(&s, text, sizeof text) > 0);
    CHECK(strstr(text, "two_sided") == NULL);
}

int main(void)
{
    clear_files();
    test_defaults();
    test_round_trip();
    test_parse_keys();
    test_bad_input_keeps_defaults();
    test_render_bands_values();
    test_free_records_values();
    test_fog_sample_values();
    test_sky_fill_values();
    test_replay_no_ffb_values();
    test_last_camera_values();
    test_sky_fill_below_values();
    test_radar_all_views_values();
    test_tach_dim_values();
    test_gauge_wheel_values();
    test_police_glow_values();
    test_psx_flares_values();
    test_emissive_lamps_follow_light_effects();
    test_bad_render_bands_keeps_the_default();
    test_render_scale_values();
    test_bad_render_scale_keeps_the_default();
    test_the_environment_wins_over_the_setting();
    test_load_save();
    test_moved_settings_fall_back_to_extras();
    test_a_longest_psx_dir_round_trips();
    test_two_sided_is_automatic();
    return TEST_REPORT();
}

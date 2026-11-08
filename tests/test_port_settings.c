/* The port's own settings file (port.cfg): defaults, text round trip, and what a damaged file leaves alone. */
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"
#include "game/race/session_rules.h"
#include "game/race/weather.h"
#include "game/sys/reverse_track.h"

#include <stdio.h>
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
    port_settings_forget();   /* the settings keep what they read */
}

static void clear_files(void)
{
    port_settings_forget();
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
    CHECK_EQ_INT(a.cavern_dark, b.cavern_dark);
    CHECK_EQ_INT(a.vegas_objects, b.vegas_objects);
    CHECK_EQ_INT(a.close_houses, b.close_houses);
    CHECK_EQ_INT(a.lamp_car_shade, b.lamp_car_shade);
    CHECK_EQ_INT(a.headlight_cone, b.headlight_cone);
    CHECK_EQ_INT(a.rain_fx, b.rain_fx);
    CHECK_EQ_INT(a.rain_grip, b.rain_grip);
    CHECK_EQ_INT(a.gauge_light, b.gauge_light);
    CHECK_EQ_INT(a.finish_gate_light, b.finish_gate_light);
    CHECK_EQ_INT(a.reverse_rows, b.reverse_rows);
    CHECK_EQ_INT(a.city_lift, b.city_lift);
    CHECK_EQ_INT(a.vegas_lift, b.vegas_lift);
    CHECK_EQ_INT(a.night_tone, b.night_tone);
    CHECK_EQ_INT(a.rain_lamps, b.rain_lamps);
    CHECK_EQ_INT(a.gauge_exterior, b.gauge_exterior);
    CHECK_EQ_INT(a.sign_glow, b.sign_glow);
    CHECK_EQ_INT(a.headlight_cone, b.headlight_cone);
    CHECK_EQ_INT(a.lamp_pool_bright, b.lamp_pool_bright);
    CHECK_EQ_INT(a.lamp_pools, b.lamp_pools);
    CHECK_EQ_INT(a.brake_over_gas, b.brake_over_gas);
    CHECK_EQ_INT(a.reflectors, b.reflectors);
    CHECK_EQ_INT(a.handbrake_lamps, b.handbrake_lamps);
    CHECK_EQ_INT(a.rally_spray, b.rally_spray);
    CHECK_EQ_INT(a.trail_rise, b.trail_rise);
    CHECK_EQ_INT(a.snow_fx, b.snow_fx);
    CHECK_EQ_INT(a.spray_cd_smoke, b.spray_cd_smoke);
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
    CHECK_EQ_INT(d.close_houses, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.vegas_objects, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.cavern_dark, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.forest_neutral, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.parking_dark, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.brake_over_gas, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.lamp_pools, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.gauge_light, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.finish_gate_light, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.rain_grip, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.rain_fx, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.headlight_cone, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.lamp_pool_bright, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.vegas_lift, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.night_tone, 0);   /* the fallback night, until asked */
    CHECK_EQ_INT(d.rain_lamps, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.gauge_exterior, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.sign_glow, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.traffic_spray, 0);
    CHECK_EQ_INT(d.rally_dry_lamps, 0);
    CHECK_EQ_INT(d.traffic_halo, 0);
    CHECK_EQ_INT(d.police_halo, 0);
    CHECK_EQ_INT(d.rain_v2, 0);   /* as it was, until asked */
    CHECK_EQ_INT(d.shoulder_light, 0);   /* as it was, until asked */
    CHECK_EQ_INT(d.track_light_lamp, 0);   /* the lamp lines are ignored until asked */
    CHECK_EQ_INT(d.rain_tyre_roll, 0);     /* no wet-tyre roll until asked */
    CHECK_EQ_INT(d.snow_weather, 0);       /* the rain as it was until asked */
    CHECK_EQ_INT(d.reflectors, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.handbrake_lamps, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.rally_spray, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.trail_rise, 0);   /* the exe's, until asked */
    CHECK_EQ_INT(d.snow_fx, 0);
    CHECK_EQ_INT(d.trail_rise, 0);
    CHECK_EQ_INT(d.spray_cd_smoke, 0);
    CHECK_EQ_INT(d.analog_pad, 0);   /* the exe's key paths, until a profile or the file asks */
    CHECK_EQ_INT(d.city_lift, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.reverse_rows, 0);   /* as the exe has it, until asked */
    CHECK_EQ_INT(d.lamp_car_shade, 0);   /* the exe's, until asked */
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

/* port (replay conditions): a replay's time of day and weather stand in for the settings' while forced; the variables still win, and unforced they are the settings' again */
static void test_conditions_force(void)
{
    unsetenv("TNFS_PSX_TIME"), unsetenv("TNFS_WEATHER");
    const int time = port_psx_time(), weather = port_weather();
    port_conditions_force(1, PSX_TIME_NIGHT == time ? PSX_TIME_DAY : PSX_TIME_NIGHT, weather == 2 ? 0 : 2);
    CHECK(port_psx_time() != time && port_weather() != weather);
    setenv("TNFS_PSX_TIME", "dusk", 1);
    CHECK_EQ_INT(port_psx_time(), PSX_TIME_DUSK);   /* the variable wins */
    unsetenv("TNFS_PSX_TIME");
    port_conditions_force(0, 0, 0);
    CHECK(port_psx_time() == time && port_weather() == weather);
}

/* reflectors: 0/1 in the file, TNFS_REFLECTORS over it, and only with the PS1 headlights on (the shine is the headlights') */
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

static void test_spray_cd_smoke_values(void)
{
    const char *const on = "spray_cd_smoke=1\n", *const bad = "spray_cd_smoke=3\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).spray_cd_smoke, 1);
    CHECK_EQ_INT(port_settings_parse("spray_cd_smoke=2\n", 17).spray_cd_smoke, 2);   /* the CD's gravel puffs variant */
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).spray_cd_smoke, 0);
    setenv("TNFS_SPRAY_CD_SMOKE", "1", 1);
    CHECK_EQ_INT(port_spray_cd_smoke(), 1);
    setenv("TNFS_SPRAY_CD_SMOKE", "2", 1);
    CHECK_EQ_INT(port_spray_cd_smoke(), 2);
    setenv("TNFS_SPRAY_CD_SMOKE", "0", 1);
    CHECK_EQ_INT(port_spray_cd_smoke(), 0);
    unsetenv("TNFS_SPRAY_CD_SMOKE");
}

static void test_trail_rise_values(void)
{
    const char *const on = "trail_rise=1\n", *const bad = "trail_rise=2\n";
    CHECK_EQ_INT(port_settings_parse(on, strlen(on)).trail_rise, 1);
    CHECK_EQ_INT(port_settings_parse(bad, strlen(bad)).trail_rise, 0);
    setenv("TNFS_TRAIL_RISE", "1", 1);
    CHECK_EQ_INT(port_trail_rise(), 1);
    setenv("TNFS_TRAIL_RISE", "0", 1);
    CHECK_EQ_INT(port_trail_rise(), 0);
    unsetenv("TNFS_TRAIL_RISE");
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
    setenv("TNFS_PSX_HEADLIGHTS", "1", 1);
    setenv("TNFS_REFLECTORS", "1", 1);
    CHECK_EQ_INT(port_reflectors(), 1);
    setenv("TNFS_PSX_HEADLIGHTS", "0", 1);
    CHECK_EQ_INT(port_reflectors(), 0);   /* no headlights, no reflectors */
    setenv("TNFS_PSX_HEADLIGHTS", "1", 1);
    setenv("TNFS_REFLECTORS", "0", 1);
    CHECK_EQ_INT(port_reflectors(), 0);
    unsetenv("TNFS_REFLECTORS");
    unsetenv("TNFS_PSX_HEADLIGHTS");
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
    CHECK_EQ_INT(port_settings_parse(stormy, strlen(stormy)).weather, WEATHER_STORM);
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
    clear_files();
    put_file("port.cfg", "profile=restomod\n");   /* the mode that leaves everything free; port_profile's test has the others */
    PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.profile, PROFILE_RESTOMOD);
    s.format = SCREEN_21_9, s.refresh_hz = 120, s.scale_integer = 1, s.fullscreen = 0, s.fov_adjust = 15, s.view_ultra = 0, s.perspective = 0, s.texture_filter = 0, s.fog = 0, s.render_scale = 3, s.render_bands = 2;
    CHECK(port_settings_save(&s));
    check_same(port_settings_load(), s);
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
                       .psx_time = PSX_TIME_DUSK, .weather = WEATHER_RAIN, .render_bands = 4, .body_shine = 4, .free_records = 1, .car_colours = 1, .fog_sample = 1, .sky_fill = 1, .psx_flares = 1, .made_tunnels = 1, .headlight_relight = 1, .chase_height = 1, .rally_gravel_shoulder = 1, .car_tunnel_light = 1, .vegas_car_night = 1, .lamp_pool_tint = 1, .trans_shadows = 1, .snow_weather = 1, .snow_fx = 1, .trail_rise = 1, .canyon_shade = 1, .tunnel_headlight = 1, .parking_dark = 1, .forest_neutral = 1, .cavern_dark = 1, .vegas_objects = 1, .close_houses = 1, .brake_over_gas = 1, .lamp_pools = 1, .lamp_car_shade = 1, .reverse_rows = 1, .city_lift = 2, .vegas_lift = 1, .headlight_cone = 1, .rain_fx = 1, .rain_grip = 1, .gauge_light = 3, .finish_gate_light = 1, .lamp_pool_bright = 1, .night_tone = 2, .rain_lamps = 1, .gauge_exterior = 1, .sign_glow = 1, .reflectors = 1, .handbrake_lamps = 1, .rally_spray = 1, .spray_cd_smoke = 1 };
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
    CHECK_EQ_INT(back.psx_flares, 1);
    CHECK_EQ_INT(back.made_tunnels, 1);
    CHECK_EQ_INT(back.headlight_relight, 1);
    CHECK_EQ_INT(back.car_tunnel_light, 1);
    CHECK_EQ_INT(back.vegas_car_night, 1);
    CHECK_EQ_INT(back.rally_gravel_shoulder, 1);
    CHECK_EQ_INT(back.chase_height, 1);
    CHECK_EQ_INT(back.lamp_pool_tint, 1);
    CHECK_EQ_INT(back.trans_shadows, 1);
    CHECK_EQ_INT(back.snow_weather, 1);
    CHECK_EQ_INT(back.canyon_shade, 1);
    CHECK_EQ_INT(back.tunnel_headlight, 1);
    CHECK_EQ_INT(back.parking_dark, 1);
    CHECK_EQ_INT(back.forest_neutral, 1);
    CHECK_EQ_INT(back.cavern_dark, 1);
    CHECK_EQ_INT(back.vegas_objects, 1);
    CHECK_EQ_INT(back.close_houses, 1);
    CHECK_EQ_INT(back.lamp_car_shade, 1);
    CHECK_EQ_INT(back.headlight_cone, 1);
    CHECK_EQ_INT(back.rain_fx, 1);
    CHECK_EQ_INT(back.rain_grip, 1);
    CHECK_EQ_INT(back.gauge_light, 3);   /* a level 0..5 */
    CHECK_EQ_INT(back.finish_gate_light, 1);
    CHECK_EQ_INT(back.reverse_rows, 1);
    CHECK_EQ_INT(back.city_lift, 2)   /* a level 0..2, not a flag */;
    CHECK_EQ_INT(back.vegas_lift, 1);
    CHECK_EQ_INT(back.night_tone, 2);   /* a value 0..2 */
    CHECK_EQ_INT(back.rain_lamps, 1);
    CHECK_EQ_INT(back.gauge_exterior, 1);
    CHECK_EQ_INT(back.sign_glow, 1);
    CHECK_EQ_INT(back.reflectors, 1);
    CHECK_EQ_INT(back.handbrake_lamps, 1);
    CHECK_EQ_INT(back.rally_spray, 1);
    CHECK_EQ_INT(back.trail_rise, 1);
    CHECK_EQ_INT(back.snow_fx, 1);
    CHECK_EQ_INT(back.spray_cd_smoke, 1);
    CHECK_EQ_INT(back.headlight_cone, 1);
    CHECK_EQ_INT(back.lamp_pool_bright, 1);
    CHECK_EQ_INT(back.lamp_pools, 1);
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

/* a wet race: rain_grip on and the weather rain, both */
static void test_rain_grip_needs_the_rain(void)
{
    setenv("TNFS_RAIN_GRIP", "1", 1);
    setenv("TNFS_WEATHER", "rain", 1);
    CHECK_EQ_INT(port_rain_grip_active(), 1);
    setenv("TNFS_WEATHER", "clear", 1);
    CHECK_EQ_INT(port_rain_grip_active(), 0);
    setenv("TNFS_WEATHER", "rain", 1);
    setenv("TNFS_RAIN_GRIP", "0", 1);
    CHECK_EQ_INT(port_rain_grip_active(), 0);
    unsetenv("TNFS_RAIN_GRIP");
    unsetenv("TNFS_WEATHER");
}

/* port (online): the settings a link race reads are the host's while its rules are set (session_rules.h), and the machine's own again after */
static void test_the_hosts_rules_win_while_set(void)
{
    clear_files();
    unsetenv("TNFS_WEATHER"), unsetenv("TNFS_REVERSE"), unsetenv("TNFS_RAIN_GRIP"), unsetenv("TNFS_PSX_TIME");
    session_rules_set(NULL);
    const int weather = port_weather(), grip = port_rain_grip(), psx = port_psx_time(), reverse = reverse_track_wanted();
    SessionRules r;
    memset(&r, 0, sizeof r);
    r.weather = (uint8_t)(weather == WEATHER_STORM ? WEATHER_FOG : WEATHER_STORM);
    r.rain_grip = (uint8_t)!grip, r.psx_time = (uint8_t)((psx + 1) % 3), r.reverse = (uint8_t)!reverse;
    session_rules_set(&r);
    CHECK_EQ_INT(port_weather(), r.weather);
    CHECK_EQ_INT(port_rain_grip(), r.rain_grip);
    CHECK_EQ_INT(port_psx_time(), r.psx_time);
    CHECK_EQ_INT(reverse_track_wanted(), r.reverse);
    setenv("TNFS_WEATHER", "clear", 1);   /* a dev override stays the strongest */
    CHECK_EQ_INT(port_weather(), WEATHER_CLEAR);
    unsetenv("TNFS_WEATHER");
    session_rules_set(NULL);
    CHECK_EQ_INT(port_weather(), weather);
    CHECK_EQ_INT(port_rain_grip(), grip);
    CHECK_EQ_INT(port_psx_time(), psx);
    CHECK_EQ_INT(reverse_track_wanted(), reverse);
}

/* port (online): radar and the 360 camera run in a link race only under a Restomod host; the machine's file is never changed by that, and its own values return after */
static void start_restomod_with(int weather);

static void restomod_with_radar_and_camera(void)
{
    start_restomod_with(WEATHER_CLEAR);
    PortExtras e = port_extras_load();
    e.bumper_radar = 1, e.follow_camera = 1;
    CHECK(port_extras_save(&e));
    unsetenv("TNFS_BUMPER_RADAR");
}

static void host_of(uint8_t mode, SessionRules *r)
{
    memset(r, 0, sizeof *r);
    r->mode = mode;
    session_rules_set(r);
}

static void test_a_stock_or_restored_host_turns_radar_and_the_360_camera_off(void)
{
    restomod_with_radar_and_camera();
    CHECK_EQ_INT(port_host_allows_radar_and_camera(), 1);
    SessionRules host;
    for (uint8_t mode = SESSION_STOCK; mode <= SESSION_RESTORED; mode++) {
        host_of(mode, &host);
        CHECK_EQ_INT(port_host_allows_radar_and_camera(), 0);
            const PortExtras e = port_extras_load();   /* the machine's own file is as it was */
        CHECK_EQ_INT(e.bumper_radar, 1);
        CHECK_EQ_INT(e.follow_camera, 1);
    }
    session_rules_set(NULL);
    clear_files();
}

static void test_a_restomod_host_leaves_radar_and_the_360_camera_to_each_player(void)
{
    restomod_with_radar_and_camera();
    SessionRules host;
    host_of(SESSION_RESTOMOD, &host);
    CHECK_EQ_INT(port_host_allows_radar_and_camera(), 1);
    session_rules_set(NULL);
    clear_files();
}

static void test_the_local_radar_and_camera_come_back_after_the_race(void)
{
    restomod_with_radar_and_camera();
    SessionRules host;
    host_of(SESSION_STOCK, &host);
    CHECK_EQ_INT(port_host_allows_radar_and_camera(), 0);
    PortSettings now = port_settings_load();   /* the race's end saves the settings */
    now.camera_kind = 2;
    CHECK(port_settings_save(&now));
    session_rules_set(NULL);
    CHECK_EQ_INT(port_host_allows_radar_and_camera(), 1);
    CHECK_EQ_INT(port_extras_load().bumper_radar, 1);
    clear_files();
}

/* port (online): the frame-budget cache and the host's session rules (session_rules.h) */
static void start_restomod_with(int weather)
{
    clear_files();
    unsetenv("TNFS_WEATHER");
    put_file("port.cfg", "profile=restomod\n");
    session_rules_set(NULL);
    PortSettings s = port_settings_load();   /* the cache is filled */
    s.weather = weather;
    CHECK(port_settings_save(&s));
}

/* rules active, then a save (the race's end keeps the camera): the file keeps the machine's weather, not the host's */
static void test_a_save_during_a_race_keeps_the_machines_weather(void)
{
    start_restomod_with(WEATHER_CLEAR);
    SessionRules host;
    memset(&host, 0, sizeof host);
    host.weather = WEATHER_RAIN;
    session_rules_set(&host);
    CHECK_EQ_INT(port_weather(), WEATHER_RAIN);
    PortSettings now = port_settings_load();
    now.camera_kind = 3;
    CHECK(port_settings_save(&now));
    CHECK_EQ_INT(port_settings_load().weather, WEATHER_CLEAR);
    session_rules_set(NULL);
    CHECK_EQ_INT(port_weather(), WEATHER_CLEAR);
    clear_files();
}

/* race 1's rules, cleared, a menu save, race 2's rules, cleared: each read is the one in force */
static void test_a_menu_change_between_two_races_takes_effect(void)
{
    start_restomod_with(WEATHER_CLEAR);
    SessionRules host;
    memset(&host, 0, sizeof host);
    host.weather = WEATHER_RAIN;
    session_rules_set(&host);
    CHECK_EQ_INT(port_weather(), WEATHER_RAIN);
    session_rules_set(NULL);
    CHECK_EQ_INT(port_weather(), WEATHER_CLEAR);
    PortSettings menu = port_settings_load();
    menu.weather = WEATHER_FOG;
    CHECK(port_settings_save(&menu));
    CHECK_EQ_INT(port_weather(), WEATHER_FOG);   /* at once: the writers drop the cache */
    host.weather = WEATHER_STORM;
    session_rules_set(&host);
    CHECK_EQ_INT(port_weather(), WEATHER_STORM);
    session_rules_set(NULL);
    CHECK_EQ_INT(port_weather(), WEATHER_FOG);
    clear_files();
}

/* every key of the table of whole numbers: set to a value other than its default, written by port_settings_format and read back by port_settings_parse, it comes back; a value one past
 * its range is refused and the field stays what it was; no other field moves */
static void test_every_int_key_round_trips(void)
{
    int count = 0;
    const PortSettingsKey *keys = port_settings_int_keys(&count);
    CHECK(count > 60);
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
        char over[96];   /* one past the top: refused */
        snprintf(over, sizeof over, "%s=%d\n", keys[i].key, keys[i].max + 1);
        const PortSettings kept = port_settings_parse(over, strlen(over));
        CHECK_EQ_INT(*(const int *)(const void *)((const char *)&kept + keys[i].offset), *(const int *)(const void *)((const char *)&defaults + keys[i].offset));
    }
}

/* the host's value wins in a link race (port_snow_weather), as the rain's grip does; the local setting rules without one */
static void test_the_host_decides_the_snow(void)
{
    unsetenv("TNFS_SNOW_WEATHER");
    SessionRules r = { .mode = SESSION_RESTOMOD, .snow_weather = 1 };
    session_rules_set(&r);
    CHECK_EQ_INT(port_snow_weather(), 1);
    r.snow_weather = 0;
    session_rules_set(&r);
    CHECK_EQ_INT(port_snow_weather(), 0);
    session_rules_set(NULL);
}

int main(void)
{
    test_the_host_decides_the_snow();
    test_every_int_key_round_trips();
    test_a_save_during_a_race_keeps_the_machines_weather();
    test_a_menu_change_between_two_races_takes_effect();
    test_the_hosts_rules_win_while_set();
    test_a_stock_or_restored_host_turns_radar_and_the_360_camera_off();
    test_a_restomod_host_leaves_radar_and_the_360_camera_to_each_player();
    test_the_local_radar_and_camera_come_back_after_the_race();
    test_rain_grip_needs_the_rain();
    clear_files();
    test_defaults();
    test_round_trip();
    test_parse_keys();
    test_bad_input_keeps_defaults();
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
    test_psx_flares_values();
    test_conditions_force();
    test_reflectors_values();
    test_rally_spray_values();
    test_trail_rise_values();
    test_snow_fx_values();
    test_spray_cd_smoke_values();
    test_emissive_lamps_follow_light_effects();
    test_bad_render_bands_keeps_the_default();
    test_render_scale_values();
    test_bad_render_scale_keeps_the_default();
    test_the_environment_wins_over_the_setting();
    test_load_save();
    test_a_longest_psx_dir_round_trips();
    test_two_sided_is_automatic();
    return TEST_REPORT();
}

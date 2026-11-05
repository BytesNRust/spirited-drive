/* The profiles (port_profile.h): the tables, the monitor's format, and the files: the profile's name kept instead of values, a hand-made change making it custom. */
#include "game/sys/port_profile.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[32];
    uint8_t bytes[PORT_CFG_TEXT_MAX];
    size_t size;
    int present;
} Stored;

static Stored stored[2];

static Stored *slot_for(const char *name)
{
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] != '\0' && strcmp(stored[i].name, name) == 0) return &stored[i];
    for (int i = 0; i < 2; i++)
        if (stored[i].name[0] == '\0') {
            strcpy(stored[i].name, name);
            return &stored[i];
        }
    return &stored[0];
}

static void clear_files(void)
{
    memset(stored, 0, sizeof stored);
}

static void put_file(const char *name, const char *text)
{
    Stored *s = slot_for(name);
    snprintf((char *)s->bytes, sizeof s->bytes, "%s", text);
    s->size = strlen(text);
    s->present = 1;
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
    if (size > sizeof s->bytes) return false;
    memcpy(s->bytes, data, size);
    s->size = size;
    s->present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static int display_w = 3440, display_h = 1440;   /* a 21:9 display */

static bool probe(int *w, int *h)
{
    *w = display_w;
    *h = display_h;
    return true;
}

static void test_names_and_monitor(void)
{
    CHECK_EQ_INT(port_profile_named("stock"), PROFILE_STOCK);
    CHECK_EQ_INT(port_profile_named("Restored"), PROFILE_RESTORED);
    CHECK_EQ_INT(port_profile_named("RESTOMOD"), PROFILE_RESTOMOD);
    CHECK_EQ_INT(port_profile_named("custom"), PROFILE_CUSTOM);
    CHECK_EQ_INT(port_profile_named("fast"), -1);
    CHECK(strcmp(port_profile_title(PROFILE_STOCK), "Stock '96") == 0);
    CHECK(strcmp(port_profile_line1(PROFILE_RESTOMOD), "Original block, period-correct mods.") == 0);
    CHECK_EQ_INT(port_profile_monitor_format(3440, 1440), SCREEN_21_9);
    CHECK_EQ_INT(port_profile_monitor_format(1920, 1080), SCREEN_16_9);
    CHECK_EQ_INT(port_profile_monitor_format(2560, 1600), SCREEN_16_9);   /* 16:10 */
    CHECK_EQ_INT(port_profile_monitor_format(1024, 768), SCREEN_4_3);
}

static void test_tables(void)
{
    const PortSettings base = port_settings_defaults();
    PortSettings s = port_profile_settings(PROFILE_STOCK, base, SCREEN_21_9);
    CHECK_EQ_INT(s.format, SCREEN_4_3);
    CHECK_EQ_INT(s.refresh_hz, 30);   /* the exe's 30 steps a second, drawn once each */
    CHECK_EQ_INT(s.scale_integer, 1);   /* Whole */
    CHECK_EQ_INT(s.last_camera + s.replay_no_ffb, 0);
    CHECK_EQ_INT(s.f3_ultra, 0);   /* the exe's three views */
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.made_tunnels + s.hall_light + s.headlight_relight + s.night_stretch_clean, 0);
    CHECK_EQ_INT(s.forest_dark, 0);
    CHECK_EQ_INT(s.night_sky_hue, 0);
    CHECK_EQ_INT(s.sky_fill_below, 0);
    CHECK_EQ_INT(s.radar_all_views, 0);
    CHECK_EQ_INT(s.tach_dim, 0);
    CHECK_EQ_INT(s.gauge_wheel, 0);
    CHECK_EQ_INT(s.police_glow, 0);
    CHECK_EQ_INT(s.night_tone, 0);   /* the exe's fallback night, in Stock and Restored */
    CHECK_EQ_INT(s.car_tunnel_light + s.vegas_car_night + s.rally_gravel_shoulder + s.chase_height + s.lamp_pool_tint + s.trans_shadows + s.canyon_shade + s.tunnel_headlight + s.parking_dark + s.forest_neutral + s.cavern_dark + s.vegas_objects + s.close_houses + s.brake_over_gas + s.lamp_pools + s.lamp_car_shade + s.reverse_rows + s.city_lift + s.vegas_lift + s.headlight_cone + s.rain_fx + s.rain_grip + s.gauge_light, 0);
    s = port_profile_settings(PROFILE_RESTORED, base, SCREEN_16_9);
    CHECK_EQ_INT(s.format, SCREEN_16_9);   /* the display's own */
    CHECK_EQ_INT(s.refresh_hz, 144);
    CHECK_EQ_INT(s.scale_integer, 1);   /* Whole */
    CHECK_EQ_INT(s.last_camera + s.replay_no_ffb, 0);
    CHECK_EQ_INT(s.render_scale, 1);
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.body_shine, 0);
    CHECK_EQ_INT(s.made_tunnels + s.hall_light + s.headlight_relight + s.night_stretch_clean, 0);
    CHECK_EQ_INT(s.forest_dark, 0);
    CHECK_EQ_INT(s.night_sky_hue, 0);
    CHECK_EQ_INT(s.sky_fill_below, 0);
    CHECK_EQ_INT(s.radar_all_views, 0);
    CHECK_EQ_INT(s.tach_dim, 0);
    CHECK_EQ_INT(s.gauge_wheel, 0);
    CHECK_EQ_INT(s.police_glow, 0);
    CHECK_EQ_INT(s.night_tone, 0);   /* the exe's fallback night, in Stock and Restored */
    CHECK_EQ_INT(s.car_tunnel_light + s.vegas_car_night + s.rally_gravel_shoulder + s.chase_height + s.lamp_pool_tint + s.trans_shadows + s.canyon_shade + s.tunnel_headlight + s.parking_dark + s.forest_neutral + s.cavern_dark + s.vegas_objects + s.close_houses + s.brake_over_gas + s.lamp_pools + s.lamp_car_shade + s.reverse_rows + s.city_lift + s.vegas_lift + s.headlight_cone + s.rain_fx + s.rain_grip + s.gauge_light, 0);
    s = port_profile_settings(PROFILE_RESTOMOD, base, SCREEN_21_9);
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    CHECK_EQ_INT(s.psx_light, 1);
    CHECK_EQ_INT(s.psx_flares, 1);
    CHECK_EQ_INT(s.sky_fill_below, 1);
    CHECK_EQ_INT(s.radar_all_views, 1);
    CHECK_EQ_INT(s.tach_dim, 1);
    CHECK_EQ_INT(s.gauge_wheel, 1);
    CHECK_EQ_INT(s.police_glow, 1);
    CHECK_EQ_INT(s.psx_headlights, 1);
    CHECK_EQ_INT(s.perspective, 1);
    CHECK_EQ_INT(s.body_shine, 4);   /* the envmap */
    CHECK_EQ_INT(s.render_scale, 2);
    CHECK_EQ_INT(s.refresh_hz, 144);
    CHECK_EQ_INT(s.scale_integer, 0);   /* Fill */
    CHECK_EQ_INT(s.last_camera, 1);
    CHECK_EQ_INT(s.replay_no_ffb, 1);
    CHECK_EQ_INT(s.made_tunnels, 1);
    CHECK_EQ_INT(s.hall_light, 1);
    CHECK_EQ_INT(s.headlight_relight, 1);
    CHECK_EQ_INT(s.night_stretch_clean, 1);
    CHECK_EQ_INT(s.forest_dark, 1);
    CHECK_EQ_INT(s.night_sky_hue, 1);
    CHECK_EQ_INT(s.rain_grip, 1);   /* the owner: less grip in the rain is Restomod's default */
    CHECK_EQ_INT(s.gauge_light, 0);
    CHECK_EQ_INT(s.finish_gate_light, 1);   /* the owner: the gates are OK (the current tint, not the neutral one) */
    CHECK_EQ_INT(s.lamp_pool_bright, 0);   /* not until the owner approves it */
    CHECK_EQ_INT(s.city_lift, 3);   /* the owner: the new City night is Restomod's default */
    CHECK_EQ_INT(s.night_tone, 2);   /* the owner picked the cool night (2) for the night with no disc */
    CHECK_EQ_INT(s.rain_lamps, 0);   /* not until the owner approves it */
    CHECK_EQ_INT(s.gauge_exterior, 0);   /* not until the owner approves it */
    CHECK_EQ_INT(s.sign_glow, 0);   /* nor the lit matrix sign */
    CHECK_EQ_INT(s.car_tunnel_light + s.vegas_car_night + s.rally_gravel_shoulder + s.chase_height + s.lamp_pool_tint + s.trans_shadows + s.canyon_shade + s.tunnel_headlight + s.parking_dark + s.forest_neutral + s.cavern_dark + s.vegas_objects + s.close_houses + s.brake_over_gas + s.lamp_pools + s.lamp_car_shade + s.reverse_rows + s.city_lift + s.vegas_lift, 22);   /* the nine fixes of the day: car tunnel light, Vegas car night, rally shoulder gravel, chase height, lamp pool, Transtropolis shadows, canyon shade */
    /* what is the player's own is not touched: laps, the PS1 folder, the time of day, full screen */
    PortSettings own = base;
    own.laps = 7, own.fullscreen = 1, own.psx_time = PSX_TIME_NIGHT;
    snprintf(own.psx_dir, sizeof own.psx_dir, "/ps1");
    s = port_profile_settings(PROFILE_RESTOMOD, own, SCREEN_21_9);
    CHECK_EQ_INT(s.laps, 7);
    CHECK_EQ_INT(s.fullscreen, 1);
    CHECK_EQ_INT(s.psx_time, PSX_TIME_NIGHT);
    CHECK(strcmp(s.psx_dir, "/ps1") == 0);
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    s.psx_flares = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.psx_flares = 1, s.sky_fill_below = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.sky_fill_below = 1;
    s.psx_flares = 1, s.radar_all_views = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.radar_all_views = 1, s.tach_dim = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.tach_dim = 1, s.gauge_wheel = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.gauge_wheel = 1, s.police_glow = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.police_glow = 1;
    s.psx_flares = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_CUSTOM, &s, SCREEN_21_9), 1);   /* custom is never wrong */
    s.psx_flares = 1;
    s.made_tunnels = 0;   /* each of the later keys counts in the match too */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.made_tunnels = 1, s.hall_light = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.hall_light = 1, s.headlight_relight = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.headlight_relight = 1, s.night_stretch_clean = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.night_stretch_clean = 1, s.forest_dark = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.forest_dark = 1, s.night_sky_hue = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.night_sky_hue = 1;
    s.car_tunnel_light = 0;   /* and the six of the day */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.car_tunnel_light = 1, s.trans_shadows = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.trans_shadows = 1, s.canyon_shade = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.canyon_shade = 1, s.tunnel_headlight = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.tunnel_headlight = 1, s.parking_dark = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.parking_dark = 1, s.forest_neutral = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.forest_neutral = 1, s.cavern_dark = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.cavern_dark = 1, s.vegas_objects = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.vegas_objects = 1, s.close_houses = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.close_houses = 1, s.brake_over_gas = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.brake_over_gas = 1, s.lamp_pools = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.lamp_pools = 1, s.lamp_car_shade = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.lamp_car_shade = 1, s.reverse_rows = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.reverse_rows = 1, s.city_lift = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.city_lift = 3, s.vegas_lift = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.vegas_lift = 2, s.headlight_cone = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.headlight_cone = 1, s.lamp_pool_bright = 1;   /* not in Restomod until the owner approves it */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.lamp_pool_bright = 0;
    s.night_tone = 0;   /* the owner's pick: the cool night is Restomod's */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.night_tone = 2;
    s.rain_lamps = 1;   /* not in Restomod until the owner approves it */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.rain_lamps = 0, s.gauge_exterior = 1;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.gauge_exterior = 0, s.sign_glow = 1;   /* not in Restomod until the owner approves it */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.sign_glow = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    s.rain_fx = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.rain_fx = 1;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    s.rain_grip = 0;   /* Restomod has less grip in the rain */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.rain_grip = 1;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    s.gauge_light = 1;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.gauge_light = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    s.finish_gate_light = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.finish_gate_light = 1;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    s.scale_integer = 1;   /* a scaling the profile does not have counts in the match */
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.scale_integer = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    s.last_camera = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.last_camera = 1, s.replay_no_ffb = 0;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 0);
    s.replay_no_ffb = 1;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    PortExtras e = port_profile_extras(PROFILE_STOCK, port_extras_defaults());
    CHECK_EQ_INT(e.movies_wide, 0);
    e = port_profile_extras(PROFILE_RESTOMOD, e);
    CHECK_EQ_INT(e.psx_liveries, 1);
    CHECK_EQ_INT(e.rally_gravel, 1);
    CHECK_EQ_INT(e.bumper_mirror + e.bumper_radar + e.follow_camera, 3);
    CHECK_EQ_INT(port_profile_extras_match(PROFILE_RESTOMOD, &e), 1);
    e.rally_gravel = 0;
    CHECK_EQ_INT(port_profile_extras_match(PROFILE_RESTOMOD, &e), 0);
    e.rally_gravel = 1, e.bumper_radar = 0;
    CHECK_EQ_INT(port_profile_extras_match(PROFILE_RESTOMOD, &e), 0);   /* the three extras count in the match too */
    e.bumper_radar = 1, e.follow_camera = 0;
    CHECK_EQ_INT(port_profile_extras_match(PROFILE_RESTOMOD, &e), 0);
    e.follow_camera = 1, e.bumper_mirror = 0;
    CHECK_EQ_INT(port_profile_extras_match(PROFILE_RESTOMOD, &e), 0);
    e.bumper_mirror = 1;
    /* the personal switches of extras are not the profile's */
    e = port_extras_defaults();
    e.car_markers = 0, e.dash_box = DASH_BOX_LEVER, e.follow_camera = 1;
    e = port_profile_extras(PROFILE_RESTOMOD, e);
    CHECK_EQ_INT(e.car_markers, 0);
    CHECK_EQ_INT(e.dash_box, DASH_BOX_LEVER);
    CHECK_EQ_INT(e.follow_camera, 1);
}

static void test_files(void)
{
    port_settings_set_display_probe(probe);
    clear_files();
    CHECK_EQ_INT(port_settings_profile(), PROFILE_CUSTOM);   /* no file: custom */
    put_file("port.cfg", "format=4:3\npsx_light=0\n");
    CHECK_EQ_INT(port_settings_load().profile, PROFILE_CUSTOM);   /* a file from before the profiles: untouched */
    CHECK_EQ_INT(port_settings_load().psx_light, 0);
    /* choosing Restomod: the name and the table in both files */
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTOMOD);
    PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.psx_light, 1);
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    CHECK_EQ_INT(port_extras_load().psx_liveries, 1);
    /* the profile's name is what lasts: a stale value in the file is not believed */
    put_file("port.cfg", "psx_light=0\nprofile=restomod\n");
    CHECK_EQ_INT(port_settings_load().psx_light, 1);
    /* a display of another shape follows */
    display_w = 1920, display_h = 1080;
    CHECK_EQ_INT(port_settings_load().format, SCREEN_16_9);
    /* Stock stays 4:3 whatever the display is */
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK_EQ_INT(port_settings_load().format, SCREEN_4_3);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 30);
    CHECK_EQ_INT(port_extras_load().movies_wide, 0);
    /* a controlled setting changed by hand in port.cfg: custom, the values as they were left */
    PortSettings t = port_settings_load();
    t.psx_light = 1;
    CHECK(port_settings_save(&t));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_CUSTOM);
    CHECK_EQ_INT(port_settings_load().psx_light, 1);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 30);
    /* and one in extras.cfg */
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTORED);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 144);
    PortExtras x = port_extras_load();
    x.rally_gravel = 1;
    CHECK(port_extras_save(&x));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_CUSTOM);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 144);   /* what Restored had stays */
    CHECK_EQ_INT(port_extras_load().rally_gravel, 1);
    /* a personal switch is no change of profile */
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    x = port_extras_load();
    x.car_markers = 0;
    CHECK(port_extras_save(&x));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTOMOD);
    PortSettings u = port_settings_load();
    u.laps = 3;
    CHECK(port_settings_save(&u));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTOMOD);
    /* an unknown name is ignored; no probe: the format the file has stays */
    put_file("port.cfg", "profile=turbo\n");
    CHECK_EQ_INT(port_settings_profile(), PROFILE_CUSTOM);
    port_settings_set_display_probe(NULL);
    put_file("port.cfg", "format=21:9\nprofile=restored\n");
    CHECK_EQ_INT(port_settings_load().format, SCREEN_21_9);
}

/* Options' Cancel: the files as they were before the first profile step; Done (end) keeps; without begin nothing is kept */
static void test_undo(void)
{
    port_settings_set_display_probe(NULL);
    clear_files();
    put_file("port.cfg", "format=4:3\npsx_light=1\nlaps=7\n");   /* custom, a hand-made mix */
    port_profile_undo_begin();
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTOMOD);
    CHECK_EQ_INT(port_profile_undo_restore(), 1);
    CHECK_EQ_INT(port_settings_profile(), PROFILE_CUSTOM);   /* back to custom, with its own values */
    CHECK_EQ_INT(port_settings_load().laps, 7);
    CHECK_EQ_INT(port_settings_load().psx_light, 1);
    CHECK_EQ_INT(port_profile_undo_restore(), 0);   /* nothing left to put back */
    /* a file that was not there is an empty one again (the defaults, custom) */
    clear_files();
    port_profile_undo_begin();
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK_EQ_INT(port_profile_undo_restore(), 1);
    CHECK_EQ_INT(port_settings_profile(), PROFILE_CUSTOM);
    /* Done: kept */
    put_file("port.cfg", "profile=stock\n");
    port_profile_undo_begin();
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    port_profile_undo_end();
    CHECK_EQ_INT(port_profile_undo_restore(), 0);
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTOMOD);
    /* not armed (Game Mode's row): no undo */
    port_profile_undo_end();
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK_EQ_INT(port_profile_undo_restore(), 0);
    CHECK_EQ_INT(port_settings_profile(), PROFILE_STOCK);
}

/* port_profile_ui: on by default, off with 0, kept by a save */
static void test_profile_ui_key(void)
{
    clear_files();
    CHECK_EQ_INT(port_settings_defaults().profile_ui, 1);
    CHECK_EQ_INT(port_settings_parse("port_profile_ui=0\n", 18).profile_ui, 0);
    CHECK_EQ_INT(port_settings_parse("port_profile_ui=2\n", 18).profile_ui, 1);   /* not a 0 or 1: the default */
    PortSettings s = port_settings_defaults();
    s.profile_ui = 0;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT(port_settings_load().profile_ui, 0);
}

int main(void)
{
    test_undo();
    test_profile_ui_key();
    test_names_and_monitor();
    test_tables();
    test_files();
    return TEST_REPORT();
}

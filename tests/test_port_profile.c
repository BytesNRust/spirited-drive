/* The modes (port_profile.h): the tables, the monitor's format, and the files: port.cfg names the mode, each mode's file keeps only the player's changes to what is free in it. */
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

enum { FILES = 5 };
static Stored stored[FILES];

static Stored *slot_for(const char *name)
{
    for (int i = 0; i < FILES; i++)
        if (stored[i].name[0] != '\0' && strcmp(stored[i].name, name) == 0) return &stored[i];
    for (int i = 0; i < FILES; i++)
        if (stored[i].name[0] == '\0') {
            strcpy(stored[i].name, name);
            return &stored[i];
        }
    return &stored[0];
}

static void clear_files(void)
{
    port_settings_forget();
    memset(stored, 0, sizeof stored);
}

static void put_file(const char *name, const char *text)
{
    Stored *s = slot_for(name);
    snprintf((char *)s->bytes, sizeof s->bytes, "%s", text);
    s->size = strlen(text);
    s->present = 1;
    port_settings_forget();   /* the settings keep what they read */
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    for (int i = 0; i < FILES; i++)
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
    CHECK_EQ_INT(port_profile_named("custom"), -1);   /* gone */
    CHECK_EQ_INT(port_profile_named("fast"), -1);
    CHECK(strcmp(port_profile_title(PROFILE_STOCK), "Stock '96") == 0);
    CHECK(strcmp(port_profile_file(PROFILE_RESTORED), "restored.cfg") == 0);
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
    CHECK_EQ_INT(s.analog_pad, 1);   /* the owner: an axis-bound control is analog in all three, as the exe was with a joystick */
    CHECK_EQ_INT(s.lamp_pool_bright, 0);   /* the exe's lamp pools */
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
    CHECK_EQ_INT(s.track_light_lamp, 0);   /* the exe's: no lamp lines */
    CHECK_EQ_INT(s.rain_tyre_roll, 0);     /* the exe's: no wet-tyre roll */
    CHECK_EQ_INT(s.night_tone, 0);   /* the exe's fallback night, in Stock and Restored */
    CHECK_EQ_INT(s.car_tunnel_light + s.vegas_car_night + s.rally_gravel_shoulder + s.chase_height + s.lamp_pool_tint + s.trans_shadows + s.canyon_shade + s.tunnel_headlight + s.parking_dark + s.forest_neutral + s.cavern_dark + s.vegas_objects + s.close_houses + s.brake_over_gas + s.lamp_pools + s.lamp_car_shade + s.reverse_rows + s.city_lift + s.vegas_lift + s.headlight_cone + s.rain_fx + s.rain_grip + s.gauge_light, 0);
    s = port_profile_settings(PROFILE_RESTORED, base, SCREEN_16_9);
    CHECK_EQ_INT(s.analog_pad, 1);
    CHECK_EQ_INT(s.lamp_pool_bright, 0);   /* the exe's lamp pools */
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
    CHECK_EQ_INT(s.track_light_lamp, 0);   /* the exe's: no lamp lines */
    CHECK_EQ_INT(s.rain_tyre_roll, 0);     /* the exe's: no wet-tyre roll */
    CHECK_EQ_INT(s.night_tone, 0);   /* the exe's fallback night, in Stock and Restored */
    CHECK_EQ_INT(s.car_tunnel_light + s.vegas_car_night + s.rally_gravel_shoulder + s.chase_height + s.lamp_pool_tint + s.trans_shadows + s.canyon_shade + s.tunnel_headlight + s.parking_dark + s.forest_neutral + s.cavern_dark + s.vegas_objects + s.close_houses + s.brake_over_gas + s.lamp_pools + s.lamp_car_shade + s.reverse_rows + s.city_lift + s.vegas_lift + s.headlight_cone + s.rain_fx + s.rain_grip + s.gauge_light, 0);
    s = port_profile_settings(PROFILE_RESTOMOD, base, SCREEN_21_9);
    CHECK_EQ_INT(s.analog_pad, 1);
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
    CHECK_EQ_INT(s.gauge_light, 5);   /* the owner: the dash's own colours ("Car") are Restomod's default; orange where the car paints none */
    CHECK_EQ_INT(s.police_halo, 1);   /* the owner: one size under the 2 he tried */
    CHECK_EQ_INT(s.rain_v2, 1);   /* the owner: the lighter rain, the storm's thunder, the carriageway reflections */
    CHECK_EQ_INT(s.track_light_lamp, 1);   /* the lamp lines of the track light files */
    CHECK_EQ_INT(s.rain_tyre_roll, 1);     /* the wet-tyre roll in the rain */
    CHECK_EQ_INT(s.shoulder_light, 1);   /* the third lane and the shoulders lit like the road */
    CHECK_EQ_INT(s.finish_gate_light, 1);   /* the owner: the gates are OK (the current tint, not the neutral one) */
    CHECK_EQ_INT(s.lamp_pool_bright, 1);   /* the owner approved the stronger lamp pools: Restomod's default (Stock and Restored keep 0) */
    CHECK_EQ_INT(s.city_lift, 3);   /* the owner: the new City night is Restomod's default */
    CHECK_EQ_INT(s.night_tone, 2);   /* the owner picked the cool night (2) for the night with no disc */
    CHECK_EQ_INT(s.rain_lamps, 0);   /* not until the owner approves it */
    CHECK_EQ_INT(s.gauge_exterior, 0);   /* not until the owner approves it */
    CHECK_EQ_INT(s.sign_glow, 1);   /* the owner OKs the lit matrix sign */
    CHECK_EQ_INT(s.car_tunnel_light + s.vegas_car_night + s.rally_gravel_shoulder + s.chase_height + s.lamp_pool_tint + s.trans_shadows + s.canyon_shade + s.tunnel_headlight + s.parking_dark + s.forest_neutral + s.cavern_dark + s.vegas_objects + s.close_houses + s.brake_over_gas + s.lamp_pools + s.lamp_car_shade + s.reverse_rows + s.city_lift + s.vegas_lift, 22);   /* the nine fixes of the day: car tunnel light, Vegas car night, rally shoulder gravel, chase height, lamp pool, Transtropolis shadows, canyon shade */
    PortExtras e = port_profile_extras(PROFILE_STOCK, port_extras_defaults());
    CHECK_EQ_INT(e.movies_wide, 0);
    e = port_profile_extras(PROFILE_RESTOMOD, e);
    CHECK_EQ_INT(e.psx_liveries, 1);
    CHECK_EQ_INT(e.rally_gravel, 1);
    CHECK_EQ_INT(e.bumper_mirror + e.bumper_radar + e.follow_camera, 3);
    CHECK_EQ_INT(e.variant_time, 1);   /* the time of day also with Mirror and Rally */
    CHECK_EQ_INT(port_profile_extras(PROFILE_RESTORED, port_extras_defaults()).variant_time, 0);
    CHECK_EQ_INT(port_profile_settings(PROFILE_STOCK, base, SCREEN_4_3).fullscreen, 1);   /* the three start full screen */
}

/* what is free, by mode */
static void test_free_keys(void)
{
    static const char *const ALL[] = { "fullscreen", "explode", "nitro", "turbo", "warrior", "mirror", "rally", "vegas", "units", "update_check" };
    static const char *const RESTORED[] = { "format", "refresh_hz", "scale", "render_bands", "stats_overlay" };
    static const char *const RESTOMOD[] = { "perspective", "texture_filter", "fog", "psx_light", "fov_adjust", "render_scale", "view_ultra", "weather", "psx_time", "laps", "reverse", "car_paint", "psx_liveries",
                                            "lunar_springs", "lunar_gravity", "warrior_rival", "no_assists", "bumper_mirror", "bumper_radar", "replay_cameras", "car_markers", "dash_box", "follow_camera",
                                            "difficulty", "start", "no_movies", "body_shine" };
    for (size_t i = 0; i < sizeof ALL / sizeof ALL[0]; i++)
        for (int p = 0; p < PROFILE_COUNT; p++) CHECK(port_profile_key_free((Profile)p, ALL[i]));
    for (size_t i = 0; i < sizeof RESTORED / sizeof RESTORED[0]; i++) {
        CHECK(!port_profile_key_free(PROFILE_STOCK, RESTORED[i]));
        CHECK(port_profile_key_free(PROFILE_RESTORED, RESTORED[i]));
        CHECK(port_profile_key_free(PROFILE_RESTOMOD, RESTORED[i]));
    }
    for (size_t i = 0; i < sizeof RESTOMOD / sizeof RESTOMOD[0]; i++) {
        CHECK(!port_profile_key_free(PROFILE_STOCK, RESTOMOD[i]));
        CHECK(!port_profile_key_free(PROFILE_RESTORED, RESTOMOD[i]));
        CHECK(port_profile_key_free(PROFILE_RESTOMOD, RESTOMOD[i]));
    }
    for (int p = 0; p < PROFILE_COUNT; p++) {
        CHECK(!port_profile_key_free((Profile)p, "profile"));
        CHECK(!port_profile_key_free((Profile)p, "psx_dir"));
    }
}

/* the bytes of a user file, or "" */
static const char *file_text(const char *name)
{
    for (int i = 0; i < FILES; i++)
        if (stored[i].present && strcmp(stored[i].name, name) == 0) {
            stored[i].bytes[stored[i].size < sizeof stored[i].bytes ? stored[i].size : sizeof stored[i].bytes - 1] = 0;
            return (const char *)stored[i].bytes;
        }
    return "";
}

static void test_reading(void)
{
    port_settings_set_display_probe(probe);
    display_w = 3440, display_h = 1440;
    clear_files();
    /* no file, no mode chosen: Stock, and it is said so */
    CHECK_EQ_INT(port_settings_profile(), PROFILE_STOCK);
    CHECK(!port_settings_profile_chosen());
    CHECK_EQ_INT(port_settings_load().format, SCREEN_4_3);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 30);
    /* a fixed key in the file of Stock is not believed; a free one is */
    put_file("stock.cfg", "format=16:9\nfog=1\nfullscreen=0\nmirror=1\ndifficulty=2\n");
    CHECK_EQ_INT(port_settings_load().format, SCREEN_4_3);
    CHECK_EQ_INT(port_settings_load().fog, 0);
    CHECK_EQ_INT(port_settings_load().fullscreen, 0);
    CHECK_EQ_INT(port_extras_load().mirror, 1);
    CHECK_EQ_INT(port_extras_load().difficulty, 1);
    /* the same in Restored: its own free ones count */
    put_file("port.cfg", "profile=restored\n");
    CHECK(port_settings_profile_chosen());
    put_file("restored.cfg", "refresh_hz=60\nscale=fill\nfog=1\nstats_overlay=1\nlunar_gravity=1\n");
    CHECK_EQ_INT(port_settings_load().format, SCREEN_21_9);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 60);
    CHECK_EQ_INT(port_settings_load().scale_integer, 0);
    CHECK_EQ_INT(port_settings_load().fog, 0);
    CHECK_EQ_INT(port_extras_load().stats_overlay, 1);
    CHECK_EQ_INT(port_extras_load().lunar_gravity, 0);
    /* in Restomod the file rules, also what has no menu */
    put_file("port.cfg", "profile=restomod\npsx_dir=/ps1\n");
    put_file("restomod.cfg", "psx_light=0\nbody_shine=0\nweather=rain\nlunar_gravity=1\nprofile=stock\npsx_dir=/other\n");
    PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.body_shine, 0);
    CHECK_EQ_INT(s.perspective, 1);   /* what it does not name stays the table's */
    CHECK_EQ_INT(port_extras_load().lunar_gravity, 1);
    CHECK_EQ_INT(s.profile, PROFILE_RESTOMOD);   /* the machine's keys are port.cfg's alone */
    CHECK(strcmp(s.psx_dir, "/ps1") == 0);
    /* a name that is no mode, with no settings to move: Stock */
    put_file("port.cfg", "profile=custom\n");
    CHECK_EQ_INT(port_settings_profile(), PROFILE_STOCK);
    CHECK_EQ_INT(port_settings_load().psx_light, 0);
    /* extras.cfg means nothing any more */
    clear_files();
    put_file("port.cfg", "profile=restomod\n");
    put_file("extras.cfg", "lunar_springs=1\n");
    CHECK_EQ_INT(port_extras_load().lunar_springs, 0);
}

static void test_saving(void)
{
    port_settings_set_display_probe(probe);
    clear_files();
    put_file("port.cfg", "profile=restomod\npsx_dir=/ps1\n");
    /* only what differs goes to the mode's file, and the machine's stays in port.cfg */
    PortSettings s = port_settings_load();
    s.fog = 0, s.laps = 3;
    CHECK(port_settings_save(&s));
    CHECK(strcmp(file_text("restomod.cfg"), "fog=0\nlaps=3\n") == 0 || strcmp(file_text("restomod.cfg"), "laps=3\nfog=0\n") == 0);
    CHECK(strstr(file_text("port.cfg"), "psx_dir=/ps1") != NULL);
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTOMOD);   /* a change is no change of mode */
    /* extras and settings share the file and keep each other's */
    PortExtras e = port_extras_load();
    e.lunar_springs = 1, e.car_paint[3] = 2;
    CHECK(port_extras_save(&e));
    CHECK(strstr(file_text("restomod.cfg"), "lunar_springs=1\n") != NULL);
    CHECK(strstr(file_text("restomod.cfg"), "car_paint=000200000000\n") != NULL);
    CHECK(strstr(file_text("restomod.cfg"), "laps=3\n") != NULL);
    s = port_settings_load();
    s.fullscreen = 0;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT(port_extras_load().lunar_springs, 1);
    CHECK_EQ_INT(port_settings_load().fog, 0);
    CHECK_EQ_INT(port_settings_load().fullscreen, 0);
    /* back to its value: out of the file */
    s = port_settings_load();
    s.fog = 1;
    CHECK(port_settings_save(&s));
    CHECK(strstr(file_text("restomod.cfg"), "fog=") == NULL);
    /* a better table value reaches who did not touch it: nothing of the table is in the file */
    CHECK(strstr(file_text("restomod.cfg"), "psx_light") == NULL);
    /* what is fixed in the mode is not written */
    clear_files();
    put_file("port.cfg", "profile=stock\n");
    s = port_settings_load();
    s.format = SCREEN_16_9, s.fullscreen = 0, s.fog = 1;
    CHECK(port_settings_save(&s));   /* the rest is written, what is fixed goes to the log */
    CHECK(strcmp(file_text("stock.cfg"), "fullscreen=0\n") == 0);
    e = port_extras_load();
    e.lunar_springs = 1, e.mirror = 1;
    CHECK(port_extras_save(&e));
    CHECK(strstr(file_text("stock.cfg"), "lunar_springs") == NULL);
    CHECK(strstr(file_text("stock.cfg"), "mirror=1\n") != NULL);
    /* F6 in Stock: the race's own, nothing kept */
    CHECK(!port_extras_set_race_switch(RACE_SWITCH_MARKERS, 0));
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_MARKERS), 1);
}

/* a value equal to the table stays while the file has it, and leaves when the player moves it back */
static void test_kept_lines(void)
{
    port_settings_set_display_probe(probe);
    clear_files();
    put_file("port.cfg", "profile=restomod\n");
    put_file("restomod.cfg", "fog=1\nlaps=3\n");
    PortSettings s = port_settings_load();
    CHECK(port_settings_save(&s));
    CHECK(strstr(file_text("restomod.cfg"), "fog=1\n") != NULL);
    put_file("restomod.cfg", "fog=0\n");
    s = port_settings_load();
    s.fog = 1;
    CHECK(port_settings_save(&s));
    CHECK(strstr(file_text("restomod.cfg"), "fog=") == NULL);
}

/* no room is not "no differences": the mode's file is not emptied */
static void test_no_room(void)
{
    char out[8];
    int dropped = 0;
    CHECK_EQ_INT((int)port_profile_differences(PROFILE_RESTOMOD, "", "laps=3\nfog=0\n", NULL, out, sizeof out, &dropped), -1);
    CHECK_EQ_INT((int)port_profile_differences(PROFILE_RESTOMOD, "fog=0\n", "fog=0\n", NULL, out, sizeof out, &dropped), 0);
    CHECK_EQ_INT(dropped, 0);
    CHECK_EQ_INT((int)port_profile_differences(PROFILE_STOCK, "", "laps=3\n", NULL, out, sizeof out, &dropped), 0);
    CHECK_EQ_INT(dropped, 1);
}

static void test_machine_file(void)
{
    port_settings_set_display_probe(probe);
    clear_files();
    put_file("port.cfg", "profile=restored\npsx_dir=/ps1\n");
    PortSettings s = port_settings_load();
    /* the settings of another mode are not saved over this one */
    s.profile = PROFILE_RESTOMOD;
    CHECK(!port_settings_save(&s));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTORED);
    CHECK(file_text("restomod.cfg")[0] == '\0');
    /* a line end in the folder would write another key */
    s = port_settings_load();
    snprintf(s.psx_dir, sizeof s.psx_dir, "/ps1\nprofile=stock");
    CHECK(!port_settings_save(&s));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTORED);
    CHECK(strcmp(file_text("port.cfg"), "profile=restored\npsx_dir=/ps1\n") == 0);
    /* a new folder is written, the same one is not */
    s = port_settings_load();
    snprintf(s.psx_dir, sizeof s.psx_dir, "/other");
    CHECK(port_settings_save(&s));
    CHECK(strcmp(port_settings_load().psx_dir, "/other") == 0);
    put_file("port.cfg", "profile=restored\npsx_dir=/other\n\n");   /* a mark: rewriting would take the empty line away */
    CHECK(port_settings_save(&s));
    CHECK(strcmp(file_text("port.cfg"), "profile=restored\npsx_dir=/other\n\n") == 0);
}

/* each mode remembers its own; a change of mode writes only the name; back to default empties that mode's file alone */
static void test_modes_remember(void)
{
    port_settings_set_display_probe(probe);
    clear_files();
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    PortSettings s = port_settings_load();
    s.fog = 0;
    CHECK(port_settings_save(&s));
    CHECK(port_profile_select(PROFILE_RESTORED));
    s = port_settings_load();
    CHECK_EQ_INT(s.fog, 0);
    CHECK_EQ_INT(s.refresh_hz, 144);
    s.refresh_hz = 60;
    CHECK(port_settings_save(&s));
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK_EQ_INT(port_settings_load().refresh_hz, 30);
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK_EQ_INT(port_settings_load().fog, 0);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 144);
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK_EQ_INT(port_settings_load().refresh_hz, 60);
    CHECK(port_profile_reset(PROFILE_RESTORED));
    CHECK_EQ_INT(port_settings_load().refresh_hz, 144);
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTORED);   /* the mode stays */
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK_EQ_INT(port_settings_load().fog, 0);   /* the others keep theirs */
    CHECK(!port_profile_select(PROFILE_COUNT));
    CHECK(!port_profile_reset(-1));
    /* the folder survives a change of mode */
    put_file("port.cfg", "profile=stock\npsx_dir=/ps1\n");
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(strcmp(port_settings_load().psx_dir, "/ps1") == 0);
}

int main(void)
{
    test_names_and_monitor();
    test_tables();
    test_free_keys();
    test_reading();
    test_saving();
    test_modes_remember();
    test_kept_lines();
    test_no_room();
    test_machine_file();
    return TEST_REPORT();
}

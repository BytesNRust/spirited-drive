/* The profiles (port_profile.h): the tables, the monitor's format, and the files: the profile's name kept instead of values, a hand-made change making it custom. */
#include "game/sys/port_profile.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[32];
    uint8_t bytes[1024];
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
    CHECK_EQ_INT(s.refresh_hz, 60);
    CHECK_EQ_INT(s.f3_ultra, 0);   /* the exe's three views */
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.made_tunnels + s.hall_light + s.headlight_relight + s.night_stretch_clean, 0);
    s = port_profile_settings(PROFILE_RESTORED, base, SCREEN_16_9);
    CHECK_EQ_INT(s.format, SCREEN_16_9);   /* the display's own */
    CHECK_EQ_INT(s.refresh_hz, 144);
    CHECK_EQ_INT(s.render_scale, 1);
    CHECK_EQ_INT(s.psx_light, 0);
    CHECK_EQ_INT(s.body_shine, 0);
    CHECK_EQ_INT(s.made_tunnels + s.hall_light + s.headlight_relight + s.night_stretch_clean, 0);
    s = port_profile_settings(PROFILE_RESTOMOD, base, SCREEN_21_9);
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    CHECK_EQ_INT(s.psx_light, 1);
    CHECK_EQ_INT(s.psx_flares, 1);
    CHECK_EQ_INT(s.psx_headlights, 1);
    CHECK_EQ_INT(s.perspective, 1);
    CHECK_EQ_INT(s.body_shine, 4);   /* the envmap */
    CHECK_EQ_INT(s.made_tunnels, 1);
    CHECK_EQ_INT(s.hall_light, 1);
    CHECK_EQ_INT(s.headlight_relight, 1);
    CHECK_EQ_INT(s.night_stretch_clean, 1);
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
    s.night_stretch_clean = 1;
    CHECK_EQ_INT(port_profile_settings_match(PROFILE_RESTOMOD, &s, SCREEN_21_9), 1);
    PortExtras e = port_profile_extras(PROFILE_STOCK, port_extras_defaults());
    CHECK_EQ_INT(e.movies_wide, 0);
    e = port_profile_extras(PROFILE_RESTOMOD, e);
    CHECK_EQ_INT(e.psx_liveries, 1);
    CHECK_EQ_INT(e.rally_gravel, 1);
    CHECK_EQ_INT(port_profile_extras_match(PROFILE_RESTOMOD, &e), 1);
    e.rally_gravel = 0;
    CHECK_EQ_INT(port_profile_extras_match(PROFILE_RESTOMOD, &e), 0);
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
    CHECK_EQ_INT(port_settings_load().refresh_hz, 60);
    CHECK_EQ_INT(port_extras_load().movies_wide, 0);
    /* a controlled setting changed by hand in port.cfg: custom, the values as they were left */
    PortSettings t = port_settings_load();
    t.psx_light = 1;
    CHECK(port_settings_save(&t));
    CHECK_EQ_INT(port_settings_profile(), PROFILE_CUSTOM);
    CHECK_EQ_INT(port_settings_load().psx_light, 1);
    CHECK_EQ_INT(port_settings_load().refresh_hz, 60);
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

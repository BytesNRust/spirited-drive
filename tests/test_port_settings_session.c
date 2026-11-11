/* The port's settings in a session: the host's rules over the player's own settings while one is set, the settings a race keeps, and the menu's changes between races. */
#include "port_settings_fixture.h"

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
    unsetenv("TNFS_WEATHER"), unsetenv("TNFS_REVERSE"), unsetenv("TNFS_RAIN_GRIP"), unsetenv("TNFS_LIGHT_TIME");
    session_rules_set(NULL);
    const int weather = port_weather(), grip = port_rain_grip(), lt = port_light_time(), reverse = reverse_track_wanted();
    SessionRules r;
    memset(&r, 0, sizeof r);
    r.weather = (uint8_t)(weather == WEATHER_STORM ? WEATHER_FOG : WEATHER_STORM);
    r.rain_grip = (uint8_t)!grip, r.light_time = (uint8_t)((lt + 1) % 3), r.reverse = (uint8_t)!reverse;
    session_rules_set(&r);
    CHECK_EQ_INT(port_weather(), r.weather);
    CHECK_EQ_INT(port_rain_grip(), r.rain_grip);
    CHECK_EQ_INT(port_light_time(), r.light_time);
    CHECK_EQ_INT(reverse_track_wanted(), r.reverse);
    setenv("TNFS_WEATHER", "clear", 1);   /* a dev override stays the strongest */
    CHECK_EQ_INT(port_weather(), WEATHER_CLEAR);
    unsetenv("TNFS_WEATHER");
    session_rules_set(NULL);
    CHECK_EQ_INT(port_weather(), weather);
    CHECK_EQ_INT(port_rain_grip(), grip);
    CHECK_EQ_INT(port_light_time(), lt);
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
    test_a_save_during_a_race_keeps_the_machines_weather();
    test_a_menu_change_between_two_races_takes_effect();
    test_the_hosts_rules_win_while_set();
    test_a_stock_or_restored_host_turns_radar_and_the_360_camera_off();
    test_a_restomod_host_leaves_radar_and_the_360_camera_to_each_player();
    test_the_local_radar_and_camera_come_back_after_the_race();
    test_rain_grip_needs_the_rain();
    clear_files();
    return TEST_REPORT();
}

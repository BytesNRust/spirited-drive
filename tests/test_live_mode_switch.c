/* port (live settings, port_follow.h): a change of the settings with no restart -- a change of mode in Game Mode, or one row changed inside the mode -- reaches every value
 * the game keeps from them: the next race behaves as the settings are now, never as they were when the game started. The settings files are kept in memory here (the
 * platform's user files); the mode is changed with port_profile_select, as Game Mode's Yes and Options' row do (menu_glue.c's profile_apply), and a row with
 * port_settings_save / port_extras_save, as Options, Graphics, CUSTOMIZE and EXTRAS do. */
#include "game/frontend/records_route.h"
#include "game/race/dash_mask.h"
#include "game/race/dash_nose.h"
#include "game/race/race_sim.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_follow.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
#include "game/viewer/spray_v4_switch.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FILES = 5, FILE_MAX = 16384 };
static struct {
    char name[32];
    uint8_t bytes[FILE_MAX];
    size_t size;
    int present;
} stored[FILES];

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    for (int i = 0; i < FILES; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) {
            uint8_t *copy = malloc(stored[i].size + 1);
            memcpy(copy, stored[i].bytes, stored[i].size);
            *size = stored[i].size;
            return copy;
        }
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    int at = -1;
    for (int i = 0; i < FILES && at < 0; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) at = i;
    for (int i = 0; i < FILES && at < 0; i++)
        if (!stored[i].present) at = i;
    if (at < 0 || size > FILE_MAX) return false;
    snprintf(stored[at].name, sizeof stored[at].name, "%s", name);
    memcpy(stored[at].bytes, data, size);
    stored[at].size = size, stored[at].present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }
bool platform_user_file_exists(const char *name)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data != NULL) platform_free_file(data);
    return data != NULL;
}

static void mode(Profile p)
{
    CHECK(port_profile_select(p));
    CHECK_EQ_INT(port_settings_profile(), p);
}

static void tick(const RaceState *race) { (void)race; }

/* every way the game writes its settings moves the generation on: a kept value cannot outlive a change */
static void test_every_write_moves_the_generation(void)
{
    mode(PROFILE_STOCK);
    uint32_t was = port_settings_generation();
    CHECK(was != 0);
    mode(PROFILE_RESTOMOD);   /* a change of mode */
    CHECK(port_settings_generation() != was);
    was = port_settings_generation();
    PortSettings s = port_settings_load();
    s.fog = !s.fog;
    CHECK(port_settings_save(&s));   /* a row of Options or Graphics */
    CHECK(port_settings_generation() != was);
    was = port_settings_generation();
    PortExtras e = port_extras_load();
    e.rally = !e.rally;
    CHECK(port_extras_save(&e));   /* a row of CUSTOMIZE or EXTRAS */
    CHECK(port_settings_generation() != was);
    was = port_settings_generation();
    CHECK(port_profile_reset(PROFILE_RESTOMOD));   /* the mode's Restore */
    CHECK(port_settings_generation() != was);
    was = port_settings_generation();
    (void)port_settings_load(), (void)port_extras_load();   /* reads move nothing */
    CHECK_EQ_INT(port_settings_generation(), was);
}

static int reads, value;
static int counted(void) { return reads++, value; }

/* a followed value is read once, and again only after a change */
static void test_follow_reads_after_a_change(void)
{
    PortFollow f = { 0 };
    reads = 0, value = 7;
    CHECK_EQ_INT(port_follow(&f, counted), 7);
    value = 9;
    CHECK_EQ_INT(port_follow(&f, counted), 7);   /* nothing written: the kept value */
    CHECK_EQ_INT(reads, 1);
    port_settings_forget();
    CHECK_EQ_INT(port_follow(&f, counted), 9);
    CHECK_EQ_INT(reads, 2);
}

/* spray v4 (Restomod's dust and spray): its switch and its emitter on the sim's ticks follow the mode, both ways (the owner's report: Stock, then Restomod, old dust) */
static void test_spray_v4_follows_the_mode(void)
{
    port_profile_reset(PROFILE_STOCK), port_profile_reset(PROFILE_RESTOMOD);
    mode(PROFILE_STOCK);
    spray_v4_switch_hook(tick);   /* a race in Stock */
    CHECK_EQ_INT(spray_v4_switch_on(), 0);
    CHECK(race_sim_tick_hook() == NULL);
    mode(PROFILE_RESTOMOD);
    spray_v4_switch_hook(tick);   /* the next race, in Restomod */
    CHECK_EQ_INT(spray_v4_switch_on(), 1);
    CHECK(race_sim_tick_hook() == tick);
    mode(PROFILE_STOCK);
    spray_v4_switch_hook(tick);   /* and back: no emitter left behind */
    CHECK_EQ_INT(spray_v4_switch_on(), 0);
    CHECK(race_sim_tick_hook() == NULL);
}

/* one row changed inside the mode reaches it too */
static void test_spray_v4_follows_a_row(void)
{
    port_profile_reset(PROFILE_RESTOMOD);
    mode(PROFILE_RESTOMOD);
    CHECK_EQ_INT(spray_v4_switch_on(), 1);
    PortSettings s = port_settings_load();
    s.spray_v4 = 0;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT(spray_v4_switch_on(), 0);
    s.spray_v4 = 1;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT(spray_v4_switch_on(), 1);
}

/* brake_over_gas (the pad's split pedals): kept by the pad's start, which runs once a game; followed, it is the mode's and the row's as they are now */
static void test_brake_over_gas_follows(void)
{
    static PortFollow brake;   /* as drive_joy.c keeps it */
    port_profile_reset(PROFILE_STOCK), port_profile_reset(PROFILE_RESTOMOD);
    mode(PROFILE_STOCK);
    const int stock = port_follow(&brake, port_brake_over_gas);
    mode(PROFILE_RESTOMOD);
    const int restomod = port_follow(&brake, port_brake_over_gas);
    CHECK_EQ_INT(stock, port_settings_of_profile(PROFILE_STOCK).brake_over_gas);
    CHECK_EQ_INT(restomod, port_settings_of_profile(PROFILE_RESTOMOD).brake_over_gas);
    CHECK(stock != restomod);
    PortSettings s = port_settings_load();
    s.brake_over_gas = !restomod;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT(port_follow(&brake, port_brake_over_gas), !restomod);
}

/* the selectors made switches and the keys folded into their parent (city_lift, vegas_lift, weather_fog, tunnel_lamps; police_glow; the beams' cone and the night lift under the light
 * effects): read at each race's load from the settings as they are now, so Restomod, then Stock, then Restomod again gives Restomod's, the exe's, and Restomod's again */
static void test_switches_follow_the_mode(void)
{
    port_profile_reset(PROFILE_STOCK), port_profile_reset(PROFILE_RESTOMOD);
    for (int pass = 0; pass < 3; pass++) {
        const int restomod = pass != 1;
        mode(restomod ? PROFILE_RESTOMOD : PROFILE_STOCK);
        CHECK_EQ_INT(port_light_effects(), restomod);
        CHECK_EQ_INT(port_city_lift(), restomod);
        CHECK_EQ_INT(port_vegas_lift(), restomod);
        CHECK_EQ_INT(port_weather_fog(), restomod);
        CHECK_EQ_INT(port_tunnel_lamps(), restomod);
        CHECK_EQ_INT(port_police_glow(), restomod);
    }
    PortSettings s = port_settings_load();   /* one row inside Restomod: its switch alone */
    s.city_lift = 0;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT(port_city_lift(), 0);
    CHECK_EQ_INT(port_vegas_lift(), 1);
}

/* dep49: the nose under the wipers (dash_nose.h) follows the wipers' switch through a change of mode with no restart: Restomod has them on, Stock off, and back in Restomod
 * the nose is made again; one row (wipers=0) inside Restomod turns it off too */
static void test_wiper_nose_follows_the_mode(void)
{
    enum { NW = 8, NH = 6 };
    static uint8_t art[NW * NH], mask[NW * NH], palette[768];
    for (int i = 0; i < NW * NH; i++) {
        const int y = i / NW;
        art[i] = y < 2 ? 0xff : (uint8_t)(10 + y), mask[i] = y < 2 ? DASH_MASK_AUTO : y == 2 ? DASH_MASK_BLADE : DASH_MASK_NOSE;
    }
    for (int k = 0; k < 256; k++) palette[3 * k] = palette[3 * k + 1] = palette[3 * k + 2] = (uint8_t)k;
    DashNoseCache c;
    memset(&c, 0, sizeof c);
    port_profile_reset(PROFILE_STOCK), port_profile_reset(PROFILE_RESTOMOD);
    unsetenv("TNFS_WIPERS");
    mode(PROFILE_RESTOMOD);
    DashNoseKey k = dash_nose_key(art, mask, palette);
    CHECK(dash_nose_cached(&c, &k, art, mask, NW, NH, 0xff) != NULL);
    const unsigned first = c.builds;
    mode(PROFILE_STOCK);   /* no restart: the key read now has the wipers off */
    k = dash_nose_key(art, mask, palette);
    CHECK(dash_nose_cached(&c, &k, art, mask, NW, NH, 0xff) == NULL);
    mode(PROFILE_RESTOMOD);
    k = dash_nose_key(art, mask, palette);
    CHECK(dash_nose_cached(&c, &k, art, mask, NW, NH, 0xff) != NULL);
    CHECK(c.builds > first);   /* made again, not kept from before */
    PortSettings s = port_settings_load();   /* the row alone */
    s.wipers = 0;
    CHECK(port_settings_save(&s));
    k = dash_nose_key(art, mask, palette);
    CHECK(dash_nose_cached(&c, &k, art, mask, NW, NH, 0xff) == NULL);
    dash_nose_cache_free(&c);
}

static int stored_file(const char *name)
{
    for (int i = 0; i < FILES; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) return 1;
    return 0;
}

/* dep55: the records follow the mode with no restart. Restored keeps the original's (config.dat's image, no file of its own); Restomod
 * the race's table in race_records.dat, the image untouched; back in Restored the image again */
static void test_records_follow_the_mode(void)
{
    static uint8_t image[RECORD_CHECK_IMAGE], game[RECORD_CHECK_GAME], before[RECORD_CHECK_IMAGE];
    enum { TRACK = 0x95, TOP = 0x6d, TOP_ROW = 0xb5 + 0xa6b * 3 + 0x57c + RECORD_ROW_SPEED };
    port_profile_reset(PROFILE_RESTORED), port_profile_reset(PROFILE_RESTOMOD);
    unsetenv("TNFS_RECORDS_UNIFIED");
    race_records_reset();
    memset(image, 0, sizeof image), memset(game, 0, sizeof game);
    image[TRACK] = 3;
    const int32_t top = 123456;
    memcpy(game + TOP, &top, 4);
    const RaceKind kind = { LAYOUT_MIRROR, 0 };
    for (int pass = 0; pass < 3; pass++) {
        const int restomod = pass == 1;
        mode(restomod ? PROFILE_RESTOMOD : PROFILE_RESTORED);
        CHECK_EQ_INT(port_records_unified(), restomod);
        memset(image + TOP_ROW, 0, 4);
        memcpy(before, image, sizeof image);
        CHECK(records_route(kind, ROUTE_TOP, image, game, 0, NULL) >= 0);
        if (restomod) {
            CHECK(memcmp(image, before, sizeof image) == 0);
            const RecordTable *t = race_records_find(3, LAYOUT_MIRROR, 0, records_race_laps(image));
            int32_t kept = 0;
            if (t != NULL) memcpy(&kept, t->top.row + RECORD_ROW_SPEED, 4);
            CHECK_EQ_INT(kept, top);
            CHECK(stored_file("race_records.dat"));
        } else {
            int32_t kept;
            memcpy(&kept, image + TOP_ROW, 4);
            CHECK_EQ_INT(kept, top);
            CHECK_EQ_INT(stored_file("race_records.dat"), pass == 2);   /* Restored writes none of its own */
        }
    }
    race_records_reset();
}

int main(void)
{
    unsetenv("TNFS_SPRAY_V4"), unsetenv("TNFS_BRAKE_OVER_GAS");
    {
        static const char *const ENV[] = { "TNFS_LIGHT_EFFECTS", "TNFS_PSX_LIGHT", "TNFS_CITY_LIFT", "TNFS_VEGAS_LIFT", "TNFS_WEATHER_FOG", "TNFS_TUNNEL_LAMPS", "TNFS_POLICE_GLOW" };
        for (size_t i = 0; i < sizeof ENV / sizeof *ENV; i++) unsetenv(ENV[i]);
    }
    test_every_write_moves_the_generation();
    test_follow_reads_after_a_change();
    test_spray_v4_follows_the_mode();
    test_spray_v4_follows_a_row();
    test_brake_over_gas_follows();
    test_switches_follow_the_mode();
    test_wiper_nose_follows_the_mode();
    test_records_follow_the_mode();
    return TEST_REPORT();
}

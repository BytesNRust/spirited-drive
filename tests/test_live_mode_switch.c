/* port (live settings, port_follow.h): a change of the settings with no restart -- a change of mode in Game Mode, or one row changed inside the mode -- reaches every value
 * the game keeps from them: the next race behaves as the settings are now, never as they were when the game started. The settings files are kept in memory here (the
 * platform's user files); the mode is changed with port_profile_select, as Game Mode's Yes and Options' row do (menu_glue.c's profile_apply), and a row with
 * port_settings_save / port_extras_save, as Options, Graphics, CUSTOMIZE and EXTRAS do. */
#include "game/race/race_sim.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_follow.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
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

int main(void)
{
    unsetenv("TNFS_SPRAY_V4"), unsetenv("TNFS_BRAKE_OVER_GAS");
    test_every_write_moves_the_generation();
    test_follow_reads_after_a_change();
    test_spray_v4_follows_the_mode();
    test_spray_v4_follows_a_row();
    test_brake_over_gas_follows();
    return TEST_REPORT();
}

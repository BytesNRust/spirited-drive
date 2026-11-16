/* port: the unlock level of the mode in use (unlock_level.c): Stock and Restored read and write config.dat's word, as the exe;
 * Restomod keeps its own in restomod.cfg (2 until changed) and never touches config.dat's. Each read is the mode's at the call,
 * so a change of mode in either direction reaches it with nothing kept (the settings files in memory, the mode changed with
 * port_profile_select as Game Mode's Yes does). */
#include "game/frontend/config_dat.h"
#include "game/frontend/unlock_level.h"
#include "game/race/lunar_springs.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FILES = 5, FILE_MAX = 4096 };
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
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, (void)size; return NULL; }
/* edition_file.c's player-folder reads (dep59 step 4): tests/platform_user_tree_stub.c, over the files above */

static const char *file_text(const char *name)
{
    static char text[FILE_MAX + 1];
    text[0] = '\0';
    for (int i = 0; i < FILES; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) {
            memcpy(text, stored[i].bytes, stored[i].size);
            text[stored[i].size] = '\0';
        }
    return text;
}

static void mode(Profile p)
{
    CHECK(port_profile_select(p));
    CHECK_EQ_INT(port_settings_profile(), p);
}

static void fresh(ConfigDat *c)
{
    memset(stored, 0, sizeof stored);
    port_settings_forget();
    memset(c, 0, sizeof *c);   /* only the level word is read here: no defaults (the exe's tables) needed */
}

/* Stock and Restored: config.dat's word, read and written as the exe */
static void test_stock_and_restored_are_config_dats(void)
{
    static ConfigDat c;
    fresh(&c);
    const Profile modes[] = { PROFILE_STOCK, PROFILE_RESTORED };
    for (int i = 0; i < 2; i++) {
        mode(modes[i]);
        config_dat_set_unlock_level(&c, 1);
        CHECK(!unlock_level_own());
        CHECK_EQ_INT(unlock_level_get(&c), 1);
        unlock_level_put(&c, 2);
        CHECK_EQ_INT(config_dat_unlock_level(&c), 2);
        CHECK(strstr(file_text(port_profile_file(modes[i])), "unlock_level") == NULL);   /* the mode's file is not touched */
    }
}

/* Restomod: 2 to start with, its own, config.dat's word neither read nor written */
static void test_restomod_keeps_its_own(void)
{
    static ConfigDat c;
    fresh(&c);
    mode(PROFILE_RESTOMOD);
    config_dat_set_unlock_level(&c, 0);
    CHECK(unlock_level_own());
    CHECK_EQ_INT(unlock_level_get(&c), 2);   /* the default, whatever config.dat says */
    unlock_level_put(&c, 1);
    CHECK_EQ_INT(unlock_level_get(&c), 1);
    CHECK_EQ_INT(config_dat_unlock_level(&c), 0);
    CHECK(strstr(file_text("restomod.cfg"), "unlock_level=1\n") != NULL);
    unlock_level_put(&c, 7);   /* clamped to the three levels */
    CHECK_EQ_INT(unlock_level_get(&c), 2);
    unlock_level_put(&c, -3);
    CHECK_EQ_INT(unlock_level_get(&c), 0);
    CHECK_EQ_INT(config_dat_unlock_level(&c), 0);
}

/* Restomod -> Stock -> Restomod with different levels: each mode reads its own at once, nothing carried over */
static void test_the_round_trip(void)
{
    static ConfigDat c;
    fresh(&c);
    config_dat_set_unlock_level(&c, 0);
    mode(PROFILE_RESTOMOD);
    unlock_level_put(&c, 1);
    CHECK_EQ_INT(unlock_level_get(&c), 1);
    mode(PROFILE_STOCK);
    CHECK_EQ_INT(unlock_level_get(&c), 0);
    unlock_level_put(&c, 2);
    CHECK_EQ_INT(unlock_level_get(&c), 2);
    mode(PROFILE_RESTOMOD);
    CHECK_EQ_INT(unlock_level_get(&c), 1);
    CHECK_EQ_INT(config_dat_unlock_level(&c), 2);
    mode(PROFILE_RESTORED);
    CHECK_EQ_INT(unlock_level_get(&c), 2);
    CHECK(port_profile_reset(PROFILE_RESTOMOD));   /* Restore profile defaults: Restomod's own back to 2 */
    mode(PROFILE_RESTOMOD);
    CHECK_EQ_INT(unlock_level_get(&c), 2);
    CHECK_EQ_INT(config_dat_unlock_level(&c), 2);
}

/* the unlocks the level gives, as 00432ff4: 1 the mirror, the Warrior and Lost Vegas; 2 the rally too */
static void test_what_each_level_gives(void)
{
    static ConfigDat c;
    fresh(&c);
    for (int level = 0; level <= 2; level++) {
        config_dat_set_unlock_level(&c, level);
        const Unlocks u = config_dat_unlocks(&c);
        CHECK_EQ_INT(u.variant_m, level >= 1);
        CHECK_EQ_INT(u.warrior, level >= 1);
        CHECK_EQ_INT(u.lost_vegas, level >= 1);
        CHECK_EQ_INT(u.variant_r, level >= 2);
    }
}

/* Lunar Springs Track is level 2's: below it the rally on Rusty Springs is the desert's (TR5 and its own art), the moon's
 * FAM and picture not taken, whatever the file says; at 2 the switch decides, On the moon, Off the desert */
static void test_lunar_or_desert(void)
{
    PortExtras on = port_extras_defaults();
    on.lunar_springs = 1, on.lunar_gravity = LUNAR_GRAVITY_SPRINGS;
    char fam[256];
    for (int level = 0; level <= 2; level++) {
        const PortExtras g = unlock_level_gate(on, level, NULL);
        CHECK_EQ_INT(g.lunar_springs, level == 2);
        CHECK_EQ_INT(g.lunar_gravity, LUNAR_GRAVITY_SPRINGS);   /* the rest as it was */
        CHECK_EQ_INT(lunar_springs_fam("/cd", LUNAR_TRACK_RUSTY, 1, g.lunar_springs, fam, sizeof fam), level == 2);
        CHECK_EQ_INT(lunar_gravity_active(LUNAR_TRACK_RUSTY, 1, g.lunar_springs, g.lunar_gravity), level == 2);
    }
    PortExtras off = on;
    off.lunar_springs = 0;
    CHECK_EQ_INT(lunar_springs_fam("/cd", LUNAR_TRACK_RUSTY, 1, unlock_level_gate(off, 2, NULL).lunar_springs, fam, sizeof fam), 0);
    /* in Restomod the mode's own level gates the file's switch, read live */
    static ConfigDat c;
    fresh(&c);
    mode(PROFILE_RESTOMOD);
    PortExtras e = port_extras_load();
    e.lunar_springs = 1;
    CHECK(port_extras_save(&e));
    CHECK_EQ_INT(unlock_level_extras(&c).lunar_springs, 1);   /* level 2, Restomod's default */
    unlock_level_put(&c, 1);
    CHECK_EQ_INT(unlock_level_extras(&c).lunar_springs, 0);
    CHECK_EQ_INT(port_extras_load().lunar_springs, 1);   /* the file keeps the player's choice */
}

int main(void)
{
    test_lunar_or_desert();
    test_stock_and_restored_are_config_dats();
    test_restomod_keeps_its_own();
    test_the_round_trip();
    test_what_each_level_gives();
    return TEST_REPORT();
}

/* The name the direct start in Control Central takes (menu_glue.c's glue_start_screen): config_name_saved is 1 only once
 * the player's own config.dat was read. With none, the CD's and the defaults' "Player" is in frontend_player_name, but
 * that is not a name the player saved, so the name box comes first. */
#include "tests/exe_tables_install.h"
#include "game/frontend/config.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/frontend.h"
#include "game/viewer/drive_joy.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* frontend.c's global (config.c reads it; the frontend itself is not part of this test) */
char frontend_player_name[42];

/* the player's config.dat, when a test sets one */
static const uint8_t *saved_bytes;
static size_t saved_size;

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    if (saved_bytes == NULL || strcmp(name, "config.dat") != 0) return NULL;
    uint8_t *copy = malloc(saved_size);
    memcpy(copy, saved_bytes, saved_size);
    *size = saved_size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}

void platform_free_file(uint8_t *data) { free(data); }

/* no joystick: config.c's controls check (drive_joy.c is the viewer's, not this test's) */
const JoyStart *drive_joy_state(void)
{
    static const JoyStart none;
    return &none;
}

/* the CD's config.dat is not read by this test */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, (void)size; return NULL; }

static void test_no_saved_config_no_saved_name(void)
{
    saved_bytes = NULL;
    CHECK_EQ_INT(config_load(), 1);   /* the defaults are in use */
    CHECK_EQ_INT(config_name_saved(), 0);
    CHECK(strcmp(frontend_player_name, "Player") == 0);   /* the default name is there, but it was not saved */
}

static void test_a_saved_config_gives_the_saved_name(void)
{
    ConfigDat c;
    config_dat_defaults(&c, 0);
    config_dat_set_name(&c, "Edgar");
    uint8_t out[CONFIG_DAT_SIZE];
    CHECK_EQ_INT(config_dat_serialise(&c, out, sizeof out), CONFIG_DAT_SIZE);
    saved_bytes = out, saved_size = sizeof out;
    CHECK_EQ_INT(config_load(), 0);
    CHECK_EQ_INT(config_name_saved(), 1);
    CHECK(strcmp(frontend_player_name, "Edgar") == 0);
    saved_bytes = NULL;
}

static int16_t music_setting(void)
{
    int16_t v;
    memcpy(&v, config_current()->image + 0x73, sizeof v);
    return v;
}

/* the Studio's test drive runs with TNFS_MUSIC=0: the music setting (00524e63) is off in this run's config, saved one or defaults; without the variable it is the config's own */
static void test_music_off_for_the_test_drive(void)
{
    ConfigDat c;
    config_dat_defaults(&c, 0);
    uint8_t out[CONFIG_DAT_SIZE];
    CHECK_EQ_INT(config_dat_serialise(&c, out, sizeof out), CONFIG_DAT_SIZE);
    saved_bytes = out, saved_size = sizeof out;
    unsetenv("TNFS_MUSIC");
    config_load();
    const int16_t own = music_setting();
    CHECK(own != 0);   /* the game's default has the music on */
    setenv("TNFS_MUSIC", "0", 1);
    config_load();
    CHECK_EQ_INT(music_setting(), 0);
    saved_bytes = NULL;
    config_load();
    CHECK_EQ_INT(music_setting(), 0);   /* the defaults too */
    setenv("TNFS_MUSIC", "1", 1);
    config_load();
    CHECK_EQ_INT(music_setting(), own);   /* any other value leaves the setting alone */
    unsetenv("TNFS_MUSIC");
}

int main(void)
{
    exe_tables_for_test();
    test_no_saved_config_no_saved_name();
    test_a_saved_config_gives_the_saved_name();
    test_music_off_for_the_test_drive();
    return TEST_REPORT();
}

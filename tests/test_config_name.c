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

int main(void)
{
    exe_tables_for_test();
    test_no_saved_config_no_saved_name();
    test_a_saved_config_gives_the_saved_name();
    return TEST_REPORT();
}

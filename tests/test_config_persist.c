/* config.dat through the real player's-folder files: what config_save writes is the 24402 bytes of the image, a started game reads it back, and the
 * gearbox the player chose (0x00524e87, 1 manual) is still there. The Windows build lost it: the saved file was refused as 0 bytes. */
#include "game/frontend/config.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/frontend.h"
#include "game/viewer/drive_joy.h"
#include "platform/platform.h"
#include "sys/resource.h"
#include "rmtree.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TRANSMISSION_AT = 0x00524e87 - 0x00524df0 };

char frontend_player_name[42];

const JoyStart *drive_joy_state(void)
{
    static const JoyStart none;
    return &none;
}

static int16_t gearbox(void)
{
    int16_t v;
    memcpy(&v, config_current()->image + TRANSMISSION_AT, sizeof v);
    return v;
}

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fclose(f);
    return n;
}

int main(void)
{
    char dir[256];
    snprintf(dir, sizeof dir, "%s/tnfs_config_persist_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    CHECK(mkdtemp(dir) != NULL);
    setenv("TNFS_PREF_DIR", dir, 1);
    resource_init(dir, 0);   /* no game data under it: the CD's config.dat is not there */

    memset(config_current()->image, 0, CONFIG_DAT_SIZE);   /* the exe's tables (the defaults) are not part of this test: an image of its own, automatic */
    const int16_t manual = 1;
    memcpy(config_current()->image + TRANSMISSION_AT, &manual, sizeof manual);
    config_save();

    char path[400];
    snprintf(path, sizeof path, "%s/config.dat", dir);
    CHECK_EQ_INT((int)file_size(path), CONFIG_DAT_SIZE);

    memset(config_current()->image, 0, CONFIG_DAT_SIZE);   /* a new run */
    CHECK_EQ_INT(config_load(), 0);                        /* the saved file was found and accepted */
    CHECK_EQ_INT(config_name_saved(), 1);
    CHECK_EQ_INT(gearbox(), 1);                            /* manual, as saved */

    CHECK_EQ_INT(test_rmtree(dir), 0);
    return TEST_REPORT();
}

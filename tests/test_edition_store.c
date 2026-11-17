/* dep59 step 4b (T2): the editions the CONTENT screen keeps (game/sys/edition_store.h), on a scratch player folder (TNFS_PREF_DIR).
 * New makes the standard set (8 cars, 8 tracks) under the name asked, its id from the name (lower case a-z 0-9 -, -2, -3 on a clash);
 * a save writes the whole file at once and the edition in use is read again after it; Duplicate copies under a new name and id; Rename
 * changes name= alone (the id and the file stay); Delete removes the file, and the edition in use goes back to the standard one. A bad
 * name or id is refused with its reason and nothing is written. */
#include "formats/stock_content.h"
#include "game/sys/edition.h"
#include "game/sys/edition_file.h"
#include "game/sys/edition_store.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "platform/platform.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256], pref[300];

static int exists(const char *rel)
{
    char path[700];
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    return access(path, F_OK) == 0;
}

static void test_ids_from_names(void)
{
    char id[EDITION_ID_MAX + 1];
    edition_store_new_id("Europa 80", id);
    CHECK(strcmp(id, "europa-80") == 0);
    edition_store_new_id("  Max!!Power ", id);
    CHECK(strcmp(id, "max-power") == 0);
    edition_store_new_id("Original", id);   /* the game's own: not an edition's */
    CHECK(strcmp(id, "edition") == 0);
    edition_store_new_id("!!!", id);
    CHECK(strcmp(id, "edition") == 0);
}

static void test_new_save_duplicate_rename(void)
{
    char id[EDITION_ID_MAX + 1], why[128];
    CHECK(edition_store_create("Europa 80", id, why, sizeof why));
    CHECK(strcmp(id, "europa-80") == 0);
    Edition e;
    CHECK(edition_load(id, &e, why, sizeof why));
    CHECK(strcmp(e.name, "Europa 80") == 0);
    CHECK_EQ_INT(e.cars, 8);
    CHECK_EQ_INT(e.tracks, 8);
    CHECK(strcmp(e.car[0], stock_car_id(0)) == 0 && strcmp(e.track[7], stock_track_id(7)) == 0);
    CHECK(edition_store_create("Europa 80", id, why, sizeof why));   /* the same name again: its id goes on with -2 */
    CHECK(strcmp(id, "europa-80-2") == 0);

    e.cars = 1, e.tracks = 1;   /* a save: the whole file, read again by the game at once */
    CHECK(edition_store_save("europa-80", &e, why, sizeof why));
    CHECK(!exists("editions/europa-80.ini.new"));
    CHECK_EQ_INT(edition_set("europa-80"), 1);
    Edition in;
    CHECK(edition_in_use(&in) != NULL && in.cars == 1);
    e.cars = 2;
    CHECK(edition_store_save("europa-80", &e, why, sizeof why));
    CHECK(edition_in_use(&in) != NULL && in.cars == 2);   /* nothing kept from before the save */

    CHECK(edition_store_duplicate("europa-80", "Copy", id, why, sizeof why));
    CHECK(strcmp(id, "copy") == 0);
    Edition copy;
    CHECK(edition_load("copy", &copy, why, sizeof why) && strcmp(copy.name, "Copy") == 0 && copy.cars == 2);

    CHECK(edition_store_rename("copy", "Other", why, sizeof why));   /* name= alone: the id and the file stay */
    CHECK(edition_load("copy", &copy, why, sizeof why) && strcmp(copy.name, "Other") == 0 && copy.cars == 2);
    CHECK(!exists("editions/other.ini"));
}

static void test_refusals(void)
{
    char id[EDITION_ID_MAX + 1] = "x", why[128] = "";
    CHECK(!edition_store_create("Thirteen chars", id, why, sizeof why));   /* 14: over EDITION_NAME_MAX */
    CHECK(why[0] != '\0');
    CHECK(!edition_store_create("Customize game", id, why, sizeof why));
    CHECK(!edition_store_rename("copy", "", why, sizeof why));
    CHECK(!edition_store_rename("not-there", "Name", why, sizeof why));
    CHECK(!edition_store_duplicate("not-there", "Name", id, why, sizeof why));
    Edition e;
    memset(&e, 0, sizeof e);
    snprintf(e.name, sizeof e.name, "Bad");
    CHECK(!edition_store_save("../escape", &e, why, sizeof why));
    CHECK(!edition_store_save("original", &e, why, sizeof why));
    CHECK(!exists("escape.ini"));
    CHECK(!edition_store_delete("not-there", why, sizeof why));
}

static void test_delete(void)
{
    char why[128];
    CHECK(edition_store_delete("copy", why, sizeof why));
    CHECK(!exists("editions/copy.ini"));
    CHECK(strcmp(port_settings_of_profile(PROFILE_RESTOMOD).edition, "europa-80") == 0);   /* not the one in use: left as it is */
    CHECK(edition_store_delete("europa-80", why, sizeof why));   /* the one in use: the standard edition again */
    CHECK(port_settings_of_profile(PROFILE_RESTOMOD).edition[0] == '\0');
    Edition buf;
    CHECK(edition_in_use(&buf) == NULL);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_edition_store_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    mkdir(base, 0777), mkdir(pref, 0777);
    setenv("TNFS_PREF_DIR", pref, 1);
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    test_ids_from_names();
    test_new_save_duplicate_rename();
    test_refusals();
    test_delete();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}

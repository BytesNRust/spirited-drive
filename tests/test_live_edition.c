/* dep59 step 4, T10: an edition changed with no restart (game/sys/edition.h's edition_set, as Game Mode's selector does): A, then B, then
 * the standard edition. Each change writes restomod.cfg's edition=, says mark:edition=<id> (- for the standard one), moves the mod
 * folders' generation on, and what the menus build from the edition in use -- the car list, the track list, the unlocks, the records
 * Best Times shows (its cars' own) -- is the new edition's at the next read: nothing is kept from the one before. The same edition again
 * changes nothing. On a scratch player folder. */
#include "game/frontend/car_list.h"
#include "game/frontend/edition_rules.h"
#include "game/frontend/track_list.h"
#include "game/sys/edition.h"
#include "game/sys/edition_file.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
#include "game/sys/race_records_added.h"
#include "game/sys/race_records_merge.h"
#include "platform/platform.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256], cd[300], pref[300];

static void make_dirs(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void write_file(const char *dir, const char *name, const char *text)
{
    char path[1100];
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

static void edition_file(const char *id, const char *text)
{
    char dir[400], name[80];
    snprintf(dir, sizeof dir, "%s/editions", pref);
    snprintf(name, sizeof name, "%s.ini", id);
    write_file(dir, name, text);
}

static int cars_are(const int16_t *want, int n)
{
    Edition buf;
    CarList l;
    car_list_build(&l, edition_in_use(&buf), 0);
    return l.count == n && memcmp(l.car, want, (size_t)n * sizeof *want) == 0;
}

static int tracks_are(const int16_t *want, int n)
{
    Edition buf;
    TrackList l;
    track_list_build(&l, edition_in_use(&buf), 0, TRACK_LIST_LUNAR_OFF);
    return l.count == n && memcmp(l.track, want, (size_t)n * sizeof *want) == 0;
}

static int warrior_unlocked(void)
{
    Edition buf;
    const Edition *e = edition_in_use(&buf);
    return e != NULL && edition_unlocks(e).warrior;
}

enum { CIRCUIT = 3, LAPS = 2 };

/* a race's total by `car` in `time` into the table `t`, as the original's check puts it: before the first row it beats */
static void put_total(RecordTable *t, int car, int32_t time)
{
    CHECK(t != NULL);
    if (t == NULL) return;
    RecordRow r;
    memset(&r, 0, sizeof r);
    snprintf((char *)r.row, RECORD_ROW_NAME_MAX, "C%d", car);
    const int32_t set = 1;
    memcpy(r.row + RECORD_ROW_CAR, &car, 4), memcpy(r.row + RECORD_ROW_CAR_B, &car, 4);
    memcpy(r.row + RECORD_ROW_TIME, &time, 4), memcpy(r.row + RECORD_ROW_SET, &set, 4);
    for (int i = 0; i < RACE_RECORDS_ROWS; i++) {
        int32_t there;
        memcpy(&there, t->totals[i].row + RECORD_ROW_TIME, 4);
        if (!(time < there)) continue;
        memmove(&t->totals[i + 1], &t->totals[i], sizeof t->totals[0] * (size_t)(RACE_RECORDS_ROWS - 1 - i));
        t->totals[i] = r;
        return;
    }
}

/* the car and time of the best total Best Times shows now; -1 when there is no table */
static int shown_best(int32_t *time)
{
    const RecordTable *t = race_records_shown(CIRCUIT, 0, 0, LAPS);
    if (t == NULL || !race_records_row_set(&t->totals[0])) return -1;
    int32_t car;
    memcpy(&car, t->totals[0].row + RECORD_ROW_CAR, 4), memcpy(time, t->totals[0].row + RECORD_ROW_TIME, 4);
    return car;
}

/* edition_set(id) changes it (1) and says so in the log */
static void set_to(const char *id, const char *mark)
{
    port_log_watch(mark);
    CHECK_EQ_INT(edition_set(id), 1);
    CHECK(port_log_watch_hit());
}

static void test_a_then_b_then_the_standard(void)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/mods/night", pref);
    write_file(dir, "mod.ini", "id=night\nname=Night\nversion=1\ntype=tweak\n");
    edition_file("a", "name=A\ncars=rx7,warrior\ntracks=city,vegas\ntweaks=night\n");
    edition_file("b", "name=B\ncars=nsx\ntracks=coastal\n");
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(mod_set_start(cd, NULL));
    const char *dirs[4];
    CHECK_EQ_INT(mod_set_dirs(dirs, 4), 0);
    const uint32_t g0 = mod_set_generation();
    race_records_reset(), race_records_added_reset();   /* every Restomod race writes race_records.dat and its car's table */
    put_total(race_records_make(CIRCUIT, 0, 0, LAPS), 8, 50000);   /* the PACK's: a column that is no car, in race_records.dat alone */
    put_total(race_records_make(CIRCUIT, 0, 0, LAPS), 1, 60000);
    put_total(race_records_make(CIRCUIT, 0, 0, LAPS), 0, 65000);
    put_total(race_records_added_make(CIRCUIT, 0, 0, LAPS, "nsx", NULL), 1, 60000);
    put_total(race_records_added_make(CIRCUIT, 0, 0, LAPS, "rx7", NULL), 0, 65000);
    int32_t time = 0;
    CHECK_EQ_INT(shown_best(&time), 8);

    set_to("a", "mark:edition=a");
    CHECK(strcmp(port_settings_of_profile(PROFILE_RESTOMOD).edition, "a") == 0);
    CHECK(mod_set_generation() != g0);
    CHECK_EQ_INT(mod_set_dirs(dirs, 4), 1);
    CHECK(strstr(dirs[0], "/mods/night") != NULL);
    CHECK(cars_are((const int16_t[]){ 0, 11 }, 2));
    CHECK(tracks_are((const int16_t[]){ 0, 8 }, 2));
    CHECK(warrior_unlocked());
    CHECK_EQ_INT(shown_best(&time), 0);   /* A's cars: the RX-7's best */
    CHECK_EQ_INT(time, 65000);
    const uint32_t g1 = mod_set_generation();
    CHECK_EQ_INT(edition_set("a"), 0);   /* the same again: nothing written, nothing moved */
    CHECK_EQ_INT((int)mod_set_generation(), (int)g1);

    set_to("b", "mark:edition=b");
    CHECK(mod_set_generation() != g1);
    CHECK_EQ_INT(mod_set_dirs(dirs, 4), 0);
    CHECK(cars_are((const int16_t[]){ 1 }, 1));
    CHECK(tracks_are((const int16_t[]){ 1 }, 1));
    CHECK(!warrior_unlocked());
    CHECK_EQ_INT(shown_best(&time), 1);   /* B's: the NSX's */
    CHECK_EQ_INT(time, 60000);

    set_to(NULL, "mark:edition=-");
    Edition buf;
    CHECK(edition_in_use(&buf) == NULL);
    CHECK(port_settings_of_profile(PROFILE_RESTOMOD).edition[0] == '\0');
    CHECK(cars_are((const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7 }, 8));
    CHECK(tracks_are((const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7 }, 8));
    CHECK_EQ_INT(shown_best(&time), 8);   /* race_records.dat again */
    CHECK_EQ_INT(time, 50000);
    CHECK_EQ_INT(edition_set(""), 0);   /* "" is the standard edition too */
    mod_set_stop();
}

/* an id no edition can have is refused, and nothing is written */
static void test_a_bad_id(void)
{
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(edition_set(NULL) >= 0);   /* from the standard edition, whatever the test before left */
    CHECK_EQ_INT(edition_set("b"), 1);
    CHECK_EQ_INT(edition_set("Not An Id"), -1);
    CHECK_EQ_INT(edition_set("original"), -1);
    CHECK(strcmp(port_settings_of_profile(PROFILE_RESTOMOD).edition, "b") == 0);
    char cfg[400];   /* restomod.cfg made a folder: the save cannot replace it, so -1 and no mark */
    snprintf(cfg, sizeof cfg, "%s/restomod.cfg", pref);
    CHECK_EQ_INT(unlink(cfg), 0);
    CHECK_EQ_INT(mkdir(cfg, 0777), 0);
    port_log_watch("mark:edition=");
    CHECK_EQ_INT(edition_set("a"), -1);
    CHECK(!port_log_watch_hit());
    CHECK_EQ_INT(rmdir(cfg), 0);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_live_edition_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(cd, sizeof cd, "%s/cd", base);
    snprintf(pref, sizeof pref, "%s/pref", base);
    make_dirs(cd), make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_a_then_b_then_the_standard();
    test_a_bad_id();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}

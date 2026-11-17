/* dep59 step 4: the edition in use (game/sys/edition_file.h), on a scratch player folder. Restomod's edition=<id> names
 * editions/<id>.ini: no key is the standard edition; a file that is missing, damaged or names no car and no track that are there is
 * logged and the standard edition used (the key and the file left as they are); Stock and Restored have none. The editions in the
 * folder are listed by id, in order. */
#include "game/sys/edition_file.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/sys/track_registry.h"
#include "platform/platform.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256], pref[300];

static void make_dirs(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void write_file(const char *rel, const char *text)
{
    char path[1100], dir[1100];
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    snprintf(dir, sizeof dir, "%s", path);
    *strrchr(dir, '/') = '\0';
    make_dirs(dir);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

static void use(const char *id)
{
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    const PortExtras e = port_extras_of_profile(PROFILE_RESTOMOD);
    snprintf(s.edition, sizeof s.edition, "%s", id);
    CHECK(port_mode_save(&s, &e));
}

static void test_the_edition_in_use(void)
{
    Edition e;
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK_EQ_INT(edition_active(&e), 0);   /* no key: the standard edition */
    write_file("editions/mine.ini", "name=Mine\ncars=supra,rx7\ntracks=city,vegas\ntweaks=night\n");
    use("mine");
    CHECK_EQ_INT(edition_active(&e), 1);
    CHECK(strcmp(e.name, "Mine") == 0 && e.cars == 2 && e.tracks == 2 && e.tweaks == 1);
    /* missing: the standard edition, said in the log, the key kept */
    use("gone");
    port_log_watch("editions/gone.ini");
    CHECK_EQ_INT(edition_active(&e), -1);
    CHECK(port_log_watch_hit());
    CHECK(strcmp(port_settings_of_profile(PROFILE_RESTOMOD).edition, "gone") == 0);
    /* damaged: the same, its reason said; the file left as it was */
    write_file("editions/bad.ini", "name=Thirteen chrs\n");
    use("bad");
    port_log_watch("1 to 12 characters");
    CHECK_EQ_INT(edition_active(&e), -1);
    CHECK(port_log_watch_hit());
    /* in Stock and Restored, no edition whatever Restomod's file says */
    use("mine");
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK_EQ_INT(edition_active(&e), 0);
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK_EQ_INT(edition_active(&e), 0);
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK_EQ_INT(edition_active(&e), 1);
}

/* an edition can be used when, its missing pieces left out, it keeps a car and a track */
static void test_what_can_be_used(void)
{
    Edition e;
    char why[160] = "";
    write_file("mods/f40/mod.ini", "id=f40\nname=F40\nversion=1\ntype=car\nstem=F40\nsound_from=viper\n");
    write_file("mods/night/mod.ini", "id=night\nname=Night\nversion=1\n");
    write_file("editions/added.ini", "name=Added\ncars=ghost,f40\ntracks=hidden,city\n");
    use("added");
    CHECK_EQ_INT(edition_active(&e), 1);
    CHECK_EQ_INT(edition_usable(&e, why, sizeof why), 1);
    CHECK_EQ_INT(edition_car_present("f40"), 1);
    CHECK_EQ_INT(edition_car_present("viper"), 1);
    CHECK_EQ_INT(edition_car_present("night"), 0);   /* a tweak is no car */
    CHECK_EQ_INT(edition_car_present("ghost"), 0);
    CHECK_EQ_INT(edition_track_present("city"), 1);
    CHECK_EQ_INT(edition_track_present("hidden"), 0);   /* no geometry on the CD: never listed */
    CHECK_EQ_INT(edition_track_present("ghost"), 0);
    write_file("mods/dune/mod.ini", "id=dune\nname=Dune\nversion=1\ntype=track\nstem=ZZB\n");   /* an installed track piece (T7g), registered or not */
    write_file("mods/not-a-track/mod.ini", "id=not-a-track\nname=Car\nversion=1\ntype=car\nstem=F40\nsound_from=viper\n");
    CHECK_EQ_INT(edition_track_present("dune"), 1);
    CHECK_EQ_INT(edition_track_present("not-a-track"), 0);   /* a car is no track */
    track_registry_clear();   /* dep59 step 5, T7b: an added track is present while it is registered, with no mod of that id installed */
    TrackPiece piece;
    memset(&piece, 0, sizeof piece);
    snprintf(piece.id, sizeof piece.id, "reg-only"), snprintf(piece.name, sizeof piece.name, "reg-only"), snprintf(piece.stem, sizeof piece.stem, "ZZB");
    piece.spec.kind = TRACK_KIND_STAGE, piece.spec.segments = 1, piece.spec.times = TRACK_TIME_DAY, piece.fingerprint = 7;
    CHECK_EQ_INT(edition_track_present("reg-only"), 0);
    CHECK(track_registry_add(&piece, NULL, 0) >= TRACK_ADDED_FIRST);
    CHECK_EQ_INT(edition_track_present("reg-only"), 1);
    track_registry_clear();
    CHECK_EQ_INT(edition_track_present("reg-only"), 0);
    write_file("editions/no-car.ini", "name=No car\ncars=ghost,night\ntracks=city\n");
    use("no-car");
    port_log_watch("no car");
    CHECK_EQ_INT(edition_active(&e), -1);
    CHECK(port_log_watch_hit());
    write_file("editions/no-track.ini", "name=No track\ncars=rx7\ntracks=hidden,ghost\n");
    use("no-track");
    CHECK_EQ_INT(edition_active(&e), -1);
    Edition no_track = { .cars = 1, .tracks = 1 };
    strcpy(no_track.car[0], "rx7"), strcpy(no_track.track[0], "hidden");
    CHECK_EQ_INT(edition_usable(&no_track, why, sizeof why), 0);
    CHECK(strstr(why, "no track") != NULL);
    use("");
}

static void test_the_list(void)
{
    char ids[8][EDITION_ID_MAX + 1];
    write_file("editions/Capital.ini", "name=X\n");    /* not an id */
    write_file("editions/original.ini", "name=X\n");   /* kept for the game's own */
    write_file("editions/notes.txt", "x");
    write_file("editions/folder.ini/x", "x");          /* a folder */
    const int n = edition_list(ids, 8);
    const char *const want[] = { "added", "bad", "mine", "no-car", "no-track" };
    CHECK_EQ_INT(n, 5);
    for (int i = 0; i < n && i < 5; i++) CHECK(strcmp(ids[i], want[i]) == 0);
    CHECK_EQ_INT(edition_list(ids, 2), 2);
    CHECK(strcmp(ids[1], "bad") == 0);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_edition_file_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_edition_in_use();
    test_what_can_be_used();
    test_the_list();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}

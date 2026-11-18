/* dep59 n4 phase 2: the mods in effect (game/sys/mod_set.h), on a scratch player folder, `--mods` above the rest (Restomod's only: Stock
 * and Restored load no mods at all); a change of edition or of mode is in effect at once (the settings' write), the generation moving on
 * only when the folders do. The overlay (platform_read_file) and the port's data (data_path) follow the same folders.
 * dep59 step 4: the rest is the edition in use's (edition_file.h): its tweaks in their order, then its added cars' folders; a piece with
 * no folder left out; the standard edition (no edition=) none, whatever is installed; an edition whose file is missing or that cannot be
 * used, the standard one (its key kept). */
#include "game/sys/data_path.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
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

/* the game's NOTE.TXT as the overlay gives it, compared with `text` */
static int note_is(const char *text)
{
    char asked[400];
    size_t size = 0;
    snprintf(asked, sizeof asked, "%s/FRONTEND/ART/IART/NOTE.TXT", cd);
    uint8_t *d = platform_read_file(asked, &size);
    const int same = d != NULL && size == strlen(text) && memcmp(d, text, size) == 0;
    platform_free_file(d);
    return same;
}

static void mod_note(const char *id, const char *text)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/mods/%s/FRONTEND/ART/IART", pref, id);
    write_file(dir, "NOTE.TXT", text);
}

/* editions/<id>.ini with these tweaks and cars (and a stock track), then restomod.cfg's edition=<id> ("" the standard one, no file) */
static const char *edition_tracks = "city";   /* the tracks= of use_edition's file */
static void use_edition(const char *id, const char *tweaks, const char *cars)
{
    if (id[0] != '\0') {
        char dir[400], text[600], name[80];
        snprintf(dir, sizeof dir, "%s/editions", pref);
        snprintf(text, sizeof text, "name=Test\ncars=%s\ntracks=%s\ntweaks=%s\n", cars, edition_tracks, tweaks);
        snprintf(name, sizeof name, "%s.ini", id);
        write_file(dir, name, text);
    }
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    const PortExtras e = port_extras_of_profile(PROFILE_RESTOMOD);
    snprintf(s.edition, sizeof s.edition, "%s", id);
    CHECK(port_mode_save(&s, &e));
}

/* an added car's folder in mods/, with its card */
static void mod_car(const char *id)
{
    char dir[400], text[200];
    snprintf(dir, sizeof dir, "%s/mods/%s", pref, id);
    snprintf(text, sizeof text, "id=%s\nname=Car\nversion=1\ntype=car\nstem=CARX\nsound_from=viper\n", id);
    write_file(dir, "mod.ini", text);
}

/* an added track's folder in mods/, with its card */
static void mod_track(const char *id)
{
    char dir[400], text[200];
    snprintf(dir, sizeof dir, "%s/mods/%s", pref, id);
    snprintf(text, sizeof text, "id=%s\nname=Track\nversion=1\ntype=track\nstem=TRKX\n", id);
    write_file(dir, "mod.ini", text);
}

static void test_the_editions_tweaks_in_their_order(void)
{
    mod_note("a", "from a");
    mod_note("b", "from b");
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(mod_set_start(cd, NULL));
    CHECK(note_is("from the cd"));   /* the standard edition: nothing installed is in effect */
    const uint32_t g0 = mod_set_generation();
    use_edition("one", "b,a", "rx7");
    CHECK(note_is("from b"));
    const uint32_t g1 = mod_set_generation();
    CHECK(g1 != g0);
    use_edition("two", "a,b", "rx7");
    CHECK(note_is("from a"));
    const char *dirs[4];
    CHECK_EQ_INT(mod_set_dirs(dirs, 4), 2);   /* the folders in effect, the first wins */
    CHECK(strstr(dirs[0], "/mods/a") != NULL && strstr(dirs[1], "/mods/b") != NULL);
    CHECK_EQ_INT(mod_set_dirs(dirs, 1), 1);   /* no more than asked for */
    CHECK(strstr(dirs[0], "/mods/a") != NULL);
    CHECK(mod_set_generation() != g1);
    const uint32_t g2 = mod_set_generation();
    PortSettings s = port_settings_load();   /* another row saved: the same folders, the same generation */
    s.fov_adjust = 2;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT((int)mod_set_generation(), (int)g2);
    char dir[400], found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];   /* the port's data follows the same folders */
    snprintf(dir, sizeof dir, "%s/mods/b", pref);
    write_file(dir, "car_envmap.png", "png");
    data_path_candidates("car_envmap.png", found);
    CHECK(strstr(found[0], "/mods/b/car_envmap.png") != NULL);
    use_edition("three", "ghost,b", "rx7");   /* a piece with no folder is left out */
    CHECK(note_is("from b"));
    /* its added cars' folders after its tweaks; a stock car has none, a missing car is left out */
    mod_car("f40");
    use_edition("four", "a", "f40,rx7,ghost-car");
    CHECK_EQ_INT(mod_set_dirs(dirs, 4), 2);
    CHECK(strstr(dirs[0], "/mods/a") != NULL && strstr(dirs[1], "/mods/f40") != NULL);
    /* a tweak named in cars= is not a car's folder */
    use_edition("five", "", "rx7,b");
    CHECK_EQ_INT(mod_set_dirs(dirs, 4), 0);
    /* back to the standard edition; then an edition whose file is missing: the standard's, its key kept */
    use_edition("", "", "");
    CHECK(note_is("from the cd"));
    use_edition("two", "a,b", "rx7");
    CHECK(note_is("from a"));
    PortSettings missing = port_settings_of_profile(PROFILE_RESTOMOD);
    const PortExtras extras = port_extras_of_profile(PROFILE_RESTOMOD);
    snprintf(missing.edition, sizeof missing.edition, "gone");
    CHECK(port_mode_save(&missing, &extras));
    CHECK(note_is("from the cd"));
    CHECK(strcmp(port_settings_of_profile(PROFILE_RESTOMOD).edition, "gone") == 0);
    /* an edition with no car that is there cannot be used: the standard's */
    use_edition("six", "a", "ghost-car");
    CHECK(note_is("from the cd"));
    /* dep59 step 5 (T8): its added tracks' folders last, after its tweaks and cars; a stock track has none, a missing one is left out, a car's
     * folder named in tracks= is no track's */
    mod_track("dune");
    mod_track("hill");
    edition_tracks = "city,dune,ghost-track,hill";
    use_edition("seven", "a", "f40,rx7");
    const char *more[6];
    CHECK_EQ_INT(mod_set_dirs(more, 6), 4);
    CHECK(strstr(more[0], "/mods/a") != NULL && strstr(more[1], "/mods/f40") != NULL);
    CHECK(strstr(more[2], "/mods/dune") != NULL && strstr(more[3], "/mods/hill") != NULL);
    edition_tracks = "city,f40,b";   /* a car's and a tweak's id in tracks=: not folders of tracks */
    use_edition("eight", "", "rx7");
    CHECK_EQ_INT(mod_set_dirs(more, 6), 0);
    edition_tracks = "city";
}

static void test_stock_and_restored_read_no_edition(void)
{
    use_edition("one-a", "a", "rx7");
    CHECK(note_is("from a"));
    const uint32_t g = mod_set_generation();
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(note_is("from the cd"));
    CHECK(mod_set_generation() != g);
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK(note_is("from the cd"));
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(note_is("from a"));   /* the edition was kept: back in Restomod, in effect again */
}

static void test_the_command_line_folder_comes_first(void)
{
    char cli[400];
    snprintf(cli, sizeof cli, "%s/cli/FRONTEND/ART/IART", base);
    write_file(cli, "NOTE.TXT", "from the command line");
    snprintf(cli, sizeof cli, "%s/cli", base);
    char missing[400];
    snprintf(missing, sizeof missing, "%s/nope", base);
    CHECK(!mod_set_start(cd, missing));   /* a --mods folder that is not there stops the start, in Restomod */
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(mod_set_start(cd, missing));   /* elsewhere it is ignored */
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(mod_set_start(cd, cli));
    CHECK(note_is("from the command line"));
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(note_is("from the cd"));   /* Stock and Restored load no mods, --mods neither */
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK(note_is("from the cd"));
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(note_is("from the command line"));
    CHECK(mod_set_start(cd, NULL));
    CHECK(note_is("from a"));
}

/* the game's file `rel` as the overlay gives it, compared with `text` */
static int game_file_is(const char *rel, const char *text)
{
    char asked[400];
    size_t size = 0;
    snprintf(asked, sizeof asked, "%s/%s", cd, rel);
    uint8_t *d = platform_read_file(asked, &size);
    const int same = d != NULL && size == strlen(text) && memcmp(d, text, size) == 0;
    platform_free_file(d);
    return same;
}

/* the port's data file `rel` as data_path finds it: 1 when the first candidate is in `dir` */
static int data_from(const char *rel, const char *dir)
{
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    data_path_candidates(rel, found);
    return strstr(found[0], dir) != NULL;
}

/* dep59 step 2: an installed folder serves only what its kind of piece may carry (mod_file_rule), saying once per generation what it
 * leaves out; --mods serves everything, and the first file that changes the race marks the run (no records, no replays) */
static void test_what_a_folder_may_serve(void)
{
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    char dir[400];
    snprintf(dir, sizeof dir, "%s/mods/twk/SIMDATA/MISC", pref);
    write_file(dir, "NOTE.B21", "from twk");
    snprintf(dir, sizeof dir, "%s/mods/twk/track_light", pref);
    write_file(dir, "TR1B.txt", "track TR1 B\n");
    snprintf(dir, sizeof dir, "%s/mods/car1", pref);
    write_file(dir, "mod.ini", "id=car1\nname=Car One\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\n");
    snprintf(dir, sizeof dir, "%s/mods/car1/track_light", pref);
    write_file(dir, "TR2B.txt", "track TR2 B\n");
    CHECK(mod_set_start(cd, NULL));
    use_edition("serve", "twk", "car1");
    port_log_watch("twk: SIMDATA/MISC/NOTE.B21 is left out");
    CHECK(game_file_is("SIMDATA/MISC/NOTE.B21", "from the cd"));   /* an installed tweak never serves what changes the race */
    CHECK(port_log_watch_hit());
    port_log_watch("is left out");
    CHECK(game_file_is("SIMDATA/MISC/NOTE.B21", "from the cd"));
    CHECK(game_file_is("simdata/misc/note.b21", "from the cd"));
    CHECK(!port_log_watch_hit());   /* said once, not at every read */
    CHECK(data_from("track_light/TR1B.txt", "/mods/twk/"));   /* its look is served */
    CHECK(!data_from("track_light/TR2B.txt", "/mods/car1/"));   /* a car's folder serves its own files only */

    char cli[400];
    snprintf(cli, sizeof cli, "%s/mods/twk", pref);
    port_log_watch("a changed track is a track piece now: tnfs-studio track new --piece");
    CHECK(mod_set_start(cd, cli));   /* the same folder as --mods */
    CHECK(game_file_is("SIMDATA/MISC/NOTE.B21", "from the cd"));   /* --mods no longer serves a file that changes the race, and says why */
    CHECK(port_log_watch_hit());
    port_log_watch("a changed track is a track piece now");
    CHECK(game_file_is("SIMDATA/MISC/NOTE.B21", "from the cd"));
    CHECK(!port_log_watch_hit());   /* once */
    CHECK(data_from("track_light/TR1B.txt", "/mods/twk/"));   /* what does not change the race is still served */
    mod_set_stop();

    snprintf(dir, sizeof dir, "%s/light_only/track_light", base);
    write_file(dir, "TR1B.txt", "track TR1 B\n");
    snprintf(cli, sizeof cli, "%s/light_only", base);
    CHECK(mod_set_start(cd, cli));
    CHECK(data_from("track_light/TR1B.txt", "/light_only/"));   /* a --mods folder with a light only changes nothing that races */

    snprintf(dir, sizeof dir, "%s/car_cli/SIMDATA/CARSPECS", base);   /* a car piece given as --mods: its own files are no change */
    write_file(dir, "TESTCAR.PBS", "specs");
    snprintf(dir, sizeof dir, "%s/car_cli", base);
    write_file(dir, "mod.ini", "id=car-cli\nname=Car Cli\nversion=1\ntype=car\nstem=TESTCAR\nsound_from=viper\n");
    snprintf(cli, sizeof cli, "%s/car_cli", base);
    CHECK(mod_set_start(cd, cli));
    CHECK(data_from("track_light/TR1B.txt", "/mods/twk/"));   /* the edition still in effect below it */
    char asked[400];
    size_t size = 0;
    snprintf(asked, sizeof asked, "%s/SIMDATA/CARSPECS/TESTCAR.PBS", cd);
    platform_free_file(platform_read_file(asked, &size));   /* served (then refused by the original's limits: it is no PBS) */

    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(mod_set_start(cd, cli));
    CHECK(game_file_is("SIMDATA/MISC/NOTE.B21", "from the cd"));   /* Stock serves nothing */
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    use_edition("", "", "");
    mod_set_stop();
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_mod_set_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(cd, sizeof cd, "%s/cd", base);
    snprintf(pref, sizeof pref, "%s/pref", base);
    char cd_dir[400];
    snprintf(cd_dir, sizeof cd_dir, "%s/FRONTEND/ART/IART", cd);   /* a file a tweak may carry (look) */
    write_file(cd_dir, "NOTE.TXT", "from the cd");
    snprintf(cd_dir, sizeof cd_dir, "%s/SIMDATA/MISC", cd);   /* one it may not: in MISC with no reader, so race (mod_file_rule) */
    write_file(cd_dir, "NOTE.B21", "from the cd");
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_editions_tweaks_in_their_order();
    test_stock_and_restored_read_no_edition();
    test_the_command_line_folder_comes_first();
    test_what_a_folder_may_serve();
    mod_set_stop();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}

/* dep59 step 2: the installed mods (game/sys/mod_store.h), each case on a scratch player folder: an install lands in mods/<id>/ with the
 * zip's bytes and nothing in mods/.staging, and only made available (dep59 step 4: no edition changes); an update keeps the old one as a backup, three at most; a staging folder
 * left by a stopped run is cleaned; a folder put there by hand, a zip the check refuses, mods/ as a file or as a link: nothing written;
 * uninstall moves to a backup (the editions that name it left as they are), restore brings it back; --mods-list's "on" is a mod the
 * edition in use names; dropping the backups drops only that id's;
 * a folder whose card names another id is left alone; a card spelt MOD.INI lands as mod.ini; the tree changes nothing outside mods/
 * and mods_backup/. */
#include "game/sys/mod_set.h"
#include "game/sys/mod_store.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "platform/platform.h"
#include "test.h"
#include "zip_build.h"

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256], pref[300], cd[300], zips[300];
static char why[256], old[32];
static ModInfo got;   /* the card mod_store_install hands back */
static uint8_t buf[1 << 16];

static void make_dirs(const char *path)
{
    char b[1024];
    snprintf(b, sizeof b, "%s", path);
    for (char *p = b + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(b, 0777); *p = '/'; }
    mkdir(b, 0777);
}

static int exists(const char *rel)
{
    char path[700];
    struct stat st;
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    return lstat(path, &st) == 0;
}

static int file_is(const char *rel, const char *text)
{
    char path[700], got[256] = "";
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(got, 1, sizeof got - 1, f);
    fclose(f);
    got[n] = '\0';
    return strcmp(got, text) == 0;
}

static void write_at(const char *rel, const char *text)
{
    char path[700], dir[700];
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    snprintf(dir, sizeof dir, "%s", path);
    *strrchr(dir, '/') = '\0';
    make_dirs(dir);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

/* a tweak's zip, `id` at `version`, with a track light saying `light`; its path */
static const char *tweak_zip(const char *name, const char *mod_id, const char *version, const char *light)
{
    static char path[400], card[200];
    snprintf(card, sizeof card, "id=%s\nname=Test Tweak\nversion=%s\n", mod_id, version);
    const ZbEntry e[] = { zb_text("mod.ini", card), zb_text("track_light/TR1B.txt", light), zb_text("README.md", "# test\n") };
    const size_t size = zb_build(e, 3, buf, sizeof buf);
    snprintf(path, sizeof path, "%s/%s", zips, name);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL && size > 0);
    if (f != NULL) fwrite(buf, 1, size, f), fclose(f);
    return path;
}

/* Restomod's edition= */
static const char *edition_text(void)
{
    static char text[EDITION_ID_MAX + 1];
    snprintf(text, sizeof text, "%s", port_settings_of_profile(PROFILE_RESTOMOD).edition);
    return text;
}

/* editions/<id>.ini naming these tweaks, in use */
static void use_edition(const char *id, const char *tweaks)
{
    char text[200], rel[80];
    snprintf(text, sizeof text, "name=Test\ncars=rx7\ntracks=city\ntweaks=%s\n", tweaks);
    snprintf(rel, sizeof rel, "editions/%s.ini", id);
    char path[700];   /* (the tree writes only below mods/ and mods_backup/: written as the player would) */
    snprintf(path, sizeof path, "%s/editions", pref);
    make_dirs(path);
    snprintf(path, sizeof path, "%s/%s", pref, rel);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    const PortExtras e = port_extras_of_profile(PROFILE_RESTOMOD);
    snprintf(s.edition, sizeof s.edition, "%s", id);
    CHECK(port_mode_save(&s, &e));
}

static int backups_of(const char *mod_id)
{
    int count = 0, n = 0;
    char **names = platform_user_list_dirs("mods_backup", &count);
    for (int i = 0; i < count; i++)
        if (strncmp(names[i], mod_id, strlen(mod_id)) == 0 && names[i][strlen(mod_id)] == '-') n++;
    platform_free_names(names);
    return n;
}

static void fresh(void)
{
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", pref);
    CHECK(system(cmd) == 0);
    make_dirs(pref);
    port_settings_forget();
    CHECK(port_profile_select(PROFILE_RESTOMOD));
}

static void test_install_and_update(void)
{
    fresh();
    CHECK(mod_store_install(tweak_zip("a.zip", "twk", "0.1", "track TR1 B one\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(strcmp(got.id, "twk") == 0 && strcmp(got.version, "0.1") == 0 && old[0] == '\0');
    CHECK(file_is("mods/twk/track_light/TR1B.txt", "track TR1 B one\n") && file_is("mods/twk/README.md", "# test\n"));
    CHECK(!exists("mods/.staging/twk"));
    CHECK(strcmp(edition_text(), "") == 0);   /* available, in no edition */
    ModStoreEntry e[4];
    CHECK_EQ_INT(mod_store_list(e, 4), 1);
    CHECK(strcmp(e[0].info.id, "twk") == 0 && !e[0].active && e[0].backups == 0);
    use_edition("mine", "twk");   /* on: the edition in use names it */
    CHECK_EQ_INT(mod_store_list(e, 4), 1);
    CHECK(e[0].active);
    CHECK(mod_store_install(tweak_zip("b.zip", "twk", "0.2", "track TR1 B two\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(strcmp(old, "0.1") == 0);
    CHECK(file_is("mods/twk/track_light/TR1B.txt", "track TR1 B two\n"));
    CHECK_EQ_INT(backups_of("twk"), 1);
    int count = 0;
    char **names = platform_user_list_dirs("mods_backup", &count);
    CHECK(count == 1 && strncmp(names[0], "twk-0.1-", 8) == 0);
    char rel[400];
    snprintf(rel, sizeof rel, "mods_backup/%s/track_light/TR1B.txt", count == 1 ? names[0] : "x");
    CHECK(file_is(rel, "track TR1 B one\n"));   /* the old files, in the backup */
    platform_free_names(names);
    CHECK(strcmp(edition_text(), "mine") == 0);   /* an update changes no edition */
    for (int i = 3; i <= 6; i++) {   /* four more updates, in the same second: three backups stay */
        char version[8];
        snprintf(version, sizeof version, "0.%d", i);
        CHECK(mod_store_install(tweak_zip("c.zip", "twk", version, "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    }
    CHECK_EQ_INT(backups_of("twk"), 3);
    CHECK(mod_store_install(tweak_zip("same.zip", "twk", "0.6", "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(strcmp(old, "0.6") == 0);   /* the same version again: reinstalled, with its backup */
    CHECK_EQ_INT(backups_of("twk"), 3);
}

static void test_the_same_version_again_says_so(void)
{
    ModStoreOutcome how = MOD_STORE_NEW;
    fresh();
    CHECK(mod_store_install_outcome(tweak_zip("a.zip", "twk", "0.1", "track TR1 B one\n"), &got, old, sizeof old, &how, why, sizeof why));
    CHECK(how == MOD_STORE_NEW && old[0] == '\0');
    CHECK(mod_store_install_outcome(tweak_zip("a2.zip", "twk", "0.1", "track TR1 B one\n"), &got, old, sizeof old, &how, why, sizeof why));
    CHECK(how == MOD_STORE_SAME && strcmp(old, "0.1") == 0);   /* the same files, the same version: reinstalled, not updated */
    CHECK_EQ_INT(backups_of("twk"), 1);   /* (still with its backup) */
    CHECK(mod_store_install_outcome(tweak_zip("a3.zip", "twk", "0.1", "track TR1 B changed\n"), &got, old, sizeof old, &how, why, sizeof why));
    CHECK(how == MOD_STORE_SAME_CHANGED && strcmp(old, "0.1") == 0);   /* other files, the same version: the author forgot to raise it */
    CHECK(file_is("mods/twk/track_light/TR1B.txt", "track TR1 B changed\n"));   /* it is replaced all the same */
    CHECK(mod_store_install_outcome(tweak_zip("b.zip", "twk", "0.2", "track TR1 B changed\n"), &got, old, sizeof old, &how, why, sizeof why));
    CHECK(how == MOD_STORE_UPDATED && strcmp(old, "0.1") == 0);
    CHECK(mod_store_install_outcome(tweak_zip("bad.zip", "twk", "0.2", "x\n"), NULL, NULL, 0, NULL, why, sizeof why));   /* every out is optional */
}

static void test_a_stopped_install_is_cleaned(void)
{
    fresh();
    write_at("mods/.staging/twk/junk", "left by a run that stopped");
    CHECK(mod_store_install(tweak_zip("a.zip", "twk", "0.1", "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(!exists("mods/.staging/twk/junk") && !exists("mods/twk/junk"));
}

static void test_what_is_never_replaced(void)
{
    fresh();
    write_at("mods/twk/mine.txt", "put here by hand");   /* no mod.ini */
    CHECK(!mod_store_install(tweak_zip("a.zip", "twk", "0.1", "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(strstr(why, "not installed by the game") != NULL);
    CHECK(file_is("mods/twk/mine.txt", "put here by hand") && !exists("mods/twk/mod.ini") && !exists("mods_backup"));
    CHECK(!mod_store_uninstall("twk", why, sizeof why));
    CHECK(file_is("mods/twk/mine.txt", "put here by hand"));
    fresh();
    static const char *const BAD[][2] = { { "../x", "x" }, { "run.exe", "MZ" }, { "SIMDATA/MISC/TR1.TRI", "x" } };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        const ZbEntry e[] = { zb_text("mod.ini", "id=bad\nname=Bad\nversion=1\n"), zb_text(BAD[i][0], BAD[i][1]) };
        const size_t size = zb_build(e, 2, buf, sizeof buf);
        char path[400];
        snprintf(path, sizeof path, "%s/bad.zip", zips);
        FILE *f = fopen(path, "wb");
        if (f != NULL) fwrite(buf, 1, size, f), fclose(f);
        CHECK(!mod_store_install(path, &got, old, sizeof old, why, sizeof why));
        CHECK(!exists("mods") && !exists("mods_backup"));   /* refused before anything is written */
    }
    char missing[400];
    snprintf(missing, sizeof missing, "%s/none.zip", zips);
    CHECK(!mod_store_install(missing, &got, old, sizeof old, why, sizeof why));
}

static void test_mods_not_a_folder(void)
{
    fresh();
    write_at("mods", "a file");
    CHECK(!mod_store_install(tweak_zip("a.zip", "twk", "0.1", "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(file_is("mods", "a file"));
    fresh();
    char elsewhere[400], link_path[400];
    snprintf(elsewhere, sizeof elsewhere, "%s/elsewhere", base);
    make_dirs(elsewhere);
    snprintf(link_path, sizeof link_path, "%s/mods", pref);
    CHECK(symlink(elsewhere, link_path) == 0);
    CHECK(!mod_store_install(tweak_zip("a.zip", "twk", "0.1", "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    char inside[500];
    snprintf(inside, sizeof inside, "%s/twk", elsewhere);
    struct stat st;
    CHECK(stat(inside, &st) != 0);   /* nothing written through the link */
    snprintf(inside, sizeof inside, "%s/.staging", elsewhere);
    CHECK(stat(inside, &st) != 0);
}

static void test_uninstall_and_restore(void)
{
    fresh();
    CHECK(mod_store_install(tweak_zip("a.zip", "twk", "0.1", "track TR1 B one\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(mod_store_install(tweak_zip("o.zip", "other", "1", "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    use_edition("both", "twk,other");
    CHECK(mod_store_uninstall("twk", why, sizeof why));
    CHECK(!exists("mods/twk") && backups_of("twk") == 1);
    CHECK(strcmp(edition_text(), "both") == 0);   /* the edition, and its file, as they were: it skips the missing piece */
    CHECK(file_is("editions/both.ini", "name=Test\ncars=rx7\ntracks=city\ntweaks=twk,other\n"));
    ModStoreEntry e[4];
    CHECK_EQ_INT(mod_store_list(e, 4), 1);
    /* dep59 step 4b: an uninstalled mod with a backup is listed as removed, from its newest backup's card; the editions naming it counted */
    CHECK_EQ_INT(mod_store_removed(e, 4), 1);
    CHECK(strcmp(e[0].info.id, "twk") == 0 && strcmp(e[0].info.version, "0.1") == 0 && e[0].backups == 1 && !e[0].active);
    CHECK_EQ_INT(mod_store_used_in("twk"), 1);
    CHECK_EQ_INT(mod_store_used_in("nothing"), 0);
    CHECK(!mod_store_uninstall("twk", why, sizeof why));   /* not installed */
    CHECK(strstr(why, "not installed") != NULL);
    CHECK(mod_store_restore("twk", why, sizeof why));
    CHECK(file_is("mods/twk/track_light/TR1B.txt", "track TR1 B one\n") && backups_of("twk") == 0);
    CHECK_EQ_INT(mod_store_removed(e, 4), 0);   /* installed again: not removed */
    CHECK(strcmp(edition_text(), "both") == 0);
    CHECK(!mod_store_restore("twk", why, sizeof why));   /* no backup left */
    CHECK(mod_store_uninstall("other", why, sizeof why) && mod_store_uninstall("twk", why, sizeof why));
    CHECK(mod_store_install(tweak_zip("a.zip", "twk", "0.2", "track TR1 B\n"), &got, old, sizeof old, why, sizeof why));
    CHECK(mod_store_uninstall("twk", why, sizeof why));
    CHECK(backups_of("twk") == 2 && backups_of("other") == 1);
    CHECK(mod_store_drop_backups("twk", why, sizeof why));
    CHECK(backups_of("twk") == 0 && backups_of("other") == 1);   /* only that id's */
    CHECK(!mod_store_uninstall("../x", why, sizeof why));
    use_edition("solo", "twk");   /* a second edition naming it: two, whichever is in use */
    CHECK_EQ_INT(mod_store_used_in("twk"), 2);
    CHECK_EQ_INT(mod_store_used_in("other"), 1);
    CHECK_EQ_INT(mod_store_used_in("../x"), 0);
}

/* the card under the name the game reads, whatever the zip spelt: listed by its exact name (a case-blind folder would find either) */
static void test_the_card_lands_as_mod_ini(void)
{
    fresh();
    const ZbEntry e[] = { zb_text("MOD.INI", "id=twk\nname=Test Tweak\nversion=1\n"), zb_text("track_light/TR1B.txt", "track TR1 B\n") };
    const size_t size = zb_build(e, 2, buf, sizeof buf);
    char path[400];
    snprintf(path, sizeof path, "%s/upper.zip", zips);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(buf, 1, size, f), fclose(f);
    CHECK(mod_store_install(path, &got, old, sizeof old, why, sizeof why));
    char dir_path[700];
    snprintf(dir_path, sizeof dir_path, "%s/mods/twk", pref);
    int lower = 0, upper = 0;
    DIR *d = opendir(dir_path);
    CHECK(d != NULL);
    for (struct dirent *de; d != NULL && (de = readdir(d)) != NULL;)
        lower += strcmp(de->d_name, "mod.ini") == 0, upper += strcmp(de->d_name, "MOD.INI") == 0;
    if (d != NULL) closedir(d);
    CHECK(lower == 1 && upper == 0);
    ModStoreEntry got[2];
    CHECK_EQ_INT(mod_store_list(got, 2), 1);
}

static void test_a_card_of_another_id_is_left_alone(void)
{
    fresh();
    write_at("mods/twk/mod.ini", "id=other\nname=Other\nversion=1\n");
    CHECK(!mod_store_uninstall("twk", why, sizeof why));
    CHECK(strstr(why, "not installed by the game") != NULL);
    CHECK(file_is("mods/twk/mod.ini", "id=other\nname=Other\nversion=1\n") && !exists("mods_backup"));
    write_at("mods_backup/twk-1-20260101-000000/mod.ini", "id=twk\nname=Test Tweak\nversion=1\n");
    CHECK(!mod_store_restore("twk", why, sizeof why));   /* nor replaced by a restore */
    CHECK(file_is("mods/twk/mod.ini", "id=other\nname=Other\nversion=1\n"));
}

/* --mods-list's "on": a mod an edition names in its tracks= is on too (it was "off" when only tweaks and cars were looked at) */
static void test_a_track_named_in_tracks_is_on(void)
{
    fresh();
    CHECK(mod_store_install(tweak_zip("t.zip", "trkmod", "0.1", "track TR1 B one\n"), &got, old, sizeof old, why, sizeof why));
    char path[700];
    snprintf(path, sizeof path, "%s/editions", pref);
    make_dirs(path);
    snprintf(path, sizeof path, "%s/editions/trk.ini", pref);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs("name=Test\ncars=rx7\ntracks=city,trkmod\n", f), fclose(f);
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    const PortExtras x = port_extras_of_profile(PROFILE_RESTOMOD);
    snprintf(s.edition, sizeof s.edition, "%s", "trk");
    CHECK(port_mode_save(&s, &x));
    ModStoreEntry e[4];
    CHECK_EQ_INT(mod_store_list(e, 4), 1);
    CHECK(strcmp(e[0].info.id, "trkmod") == 0 && e[0].active);
}

/* what changes the player's folder changes only mods/, mods_backup/ and editions/, whatever a caller asks */
static void test_the_tree_changes_only_mods(void)
{
    fresh();
    write_at("settings.ini", "kept\n");
    write_at("mods/a/x.txt", "x");
    const uint8_t data[] = "y";
    CHECK(!platform_user_mkdirs("elsewhere") && !exists("elsewhere"));
    CHECK(!platform_user_mkdirs("modsx/a") && !exists("modsx"));
    CHECK(!platform_user_write_file_at("settings.ini", data, 1) && file_is("settings.ini", "kept\n"));
    CHECK(!platform_user_remove_tree("settings.ini") && exists("settings.ini"));
    CHECK(!platform_user_rename("settings.ini", "mods/settings.ini") && exists("settings.ini"));
    CHECK(!platform_user_rename("mods/a", "a") && exists("mods/a/x.txt"));
    CHECK(platform_user_mkdirs("mods_backup/b") && platform_user_rename("mods/a", "mods_backup/b/a"));
    CHECK(platform_user_write_file_at("mods/y.txt", data, 1) && file_is("mods/y.txt", "y"));
    CHECK(platform_user_remove_tree("mods_backup/b") && !exists("mods_backup/b"));
    CHECK(platform_user_is_dir("mods") && !platform_user_is_dir("settings.ini"));   /* reading is not limited */
    /* dep59 step 4b: editions/ too (the editions the CONTENT screen writes), and nothing beside it */
    CHECK(platform_user_mkdirs("editions") && platform_user_write_file_at("editions/a.ini", data, 1) && file_is("editions/a.ini", "y"));
    CHECK(platform_user_rename("editions/a.ini", "editions/b.ini") && file_is("editions/b.ini", "y") && !exists("editions/a.ini"));
    CHECK(!platform_user_rename("editions/b.ini", "b.ini") && exists("editions/b.ini"));
    CHECK(!platform_user_mkdirs("editionsx") && !exists("editionsx"));
    CHECK(platform_user_remove_tree("editions/b.ini") && !exists("editions/b.ini"));
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_mod_store_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    snprintf(cd, sizeof cd, "%s/cd", base);
    snprintf(zips, sizeof zips, "%s/zips", base);
    make_dirs(pref), make_dirs(cd), make_dirs(zips);
    setenv("TNFS_PREF_DIR", pref, 1);
    CHECK(mod_set_start(cd, NULL));
    test_install_and_update();
    test_the_same_version_again_says_so();
    test_a_stopped_install_is_cleaned();
    test_what_is_never_replaced();
    test_mods_not_a_folder();
    test_uninstall_and_restore();
    test_the_card_lands_as_mod_ini();
    test_a_card_of_another_id_is_left_alone();
    test_a_track_named_in_tracks_is_on();
    test_the_tree_changes_only_mods();
    mod_set_stop();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}

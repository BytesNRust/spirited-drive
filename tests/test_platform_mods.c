/* The mods overlay of the platform layer (BYTES-0009 AC8), on a scratch directory: the mod's copy replaces the
 * data root's, everything else comes from the data root, an invalid mod file is refused with the file and the
 * reason, nothing can leave the mods directory, and the player's files go to the player's folder (never beside the mod, never into the data
 * root), which --user moves and the older folders feed once. No window is opened. */
#include "platform/platform.h"
#include "test.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void make_dirs(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void write_file(const char *dir, const char *name, const void *data, size_t size)
{
    char path[1100];
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f == NULL) return;
    fwrite(data, 1, size, f);
    fclose(f);
}

/* A checksum over every file below a directory, to show the data root was not written. */
static uint32_t tree_sum(const char *dir)
{
    uint32_t sum = 0;
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { sum = sum * 31 + tree_sum(path); continue; }
        FILE *f = fopen(path, "rb");
        int c;
        while (f && (c = fgetc(f)) != EOF) sum = sum * 33 + (uint32_t)c;
        sum = sum * 31 + (uint32_t)st.st_size;
        if (f) fclose(f);
    }
    closedir(d);
    return sum;
}

static int same(const uint8_t *data, size_t size, const char *text)
{
    return data != NULL && size == strlen(text) && memcmp(data, text, size) == 0;
}

static int file_is(const char *dir, const char *name, const char *text)
{
    char path[1100], got[64] = "";
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(got, 1, sizeof got - 1, f);
    fclose(f);
    return n == strlen(text) && memcmp(got, text, n) == 0;
}

/* the player's folder's older names, beside it: "TNFS Restomod" first, then tnfs/tnfs. What the folder has stays; what both old ones have comes
 * from the newer; everything else comes from wherever it is; the old copies of what stayed are left alone; a second start moves nothing */
static void test_older_folders(const char *base)
{
    char data[400], pref[420], restomod[420], tnfs[420];
    snprintf(data, sizeof data, "%s/osdata", base);
    snprintf(pref, sizeof pref, "%s/Spirited Drive/", data);
    snprintf(restomod, sizeof restomod, "%s/TNFS Restomod", data);
    snprintf(tnfs, sizeof tnfs, "%s/tnfs/tnfs", data);
    write_file(pref, "port.cfg", "new port", 8);
    write_file(restomod, "port.cfg", "restomod port", 13);
    write_file(restomod, "records.dat", "restomod records", 16);
    write_file(restomod, "controls.cfg", "restomod controls", 17);
    write_file(tnfs, "records.dat", "tnfs records", 12);
    write_file(tnfs, "Replay.rpl", "tnfs replay", 11);
    CHECK_EQ_INT(platform_migrate_user_folders(pref), 3);   /* records.dat and controls.cfg from TNFS Restomod, Replay.rpl from tnfs/tnfs */
    CHECK(file_is(pref, "port.cfg", "new port"));               /* the folder's own wins */
    CHECK(file_is(pref, "records.dat", "restomod records"));    /* the newer old name wins over the older */
    CHECK(file_is(pref, "controls.cfg", "restomod controls"));
    CHECK(file_is(pref, "Replay.rpl", "tnfs replay"));
    CHECK(file_is(restomod, "port.cfg", "restomod port"));      /* what stayed behind is intact */
    CHECK(file_is(tnfs, "records.dat", "tnfs records"));
    CHECK(!file_is(restomod, "records.dat", "restomod records"));   /* moved, not copied */
    CHECK_EQ_INT(platform_migrate_user_folders(pref), 0);   /* once */
    char lone[420];
    snprintf(lone, sizeof lone, "%s/elsewhere/Spirited Drive/", base);
    CHECK_EQ_INT(platform_migrate_user_folders(lone), 0);   /* no old folders: nothing, and nothing made */
}

int main(void)
{
    char base[256];
    snprintf(base, sizeof base, "%s/tnfs_mods_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    char cd[300], mods[300], outside[300], path[400];
    snprintf(cd, sizeof cd, "%s/cd", base);
    snprintf(mods, sizeof mods, "%s/mod", base);
    snprintf(outside, sizeof outside, "%s/outside", base);
    char cd_misc[400], mod_misc[400];
    snprintf(cd_misc, sizeof cd_misc, "%s/SIMDATA/MISC", cd);
    snprintf(mod_misc, sizeof mod_misc, "%s/SIMDATA/MISC", mods);

    write_file(cd_misc, "NOTE.TXT", "from the cd", 11);
    write_file(cd_misc, "ONLYCD.TXT", "cd only", 7);
    write_file(cd_misc, "TR2.TRI", "cd tri", 6);
    write_file(mod_misc, "NOTE.TXT", "from the mod", 12);
    write_file(mod_misc, "TR2.TRI", "short", 5);                       /* not a TRI: invalid */
    write_file(cd_misc, "ESCAPE.TXT", "cd copy", 7);
    write_file(outside, "secret.txt", "secret", 6);
    char secret[400];
    snprintf(secret, sizeof secret, "%s/secret.txt", outside);
    snprintf(path, sizeof path, "%s/ESCAPE.TXT", mod_misc);
    CHECK(symlink(secret, path) == 0);                              /* the mod's file is a link out of the mods directory */
    const uint32_t cd_before = tree_sum(cd);
    char pref[400];
    snprintf(pref, sizeof pref, "%s/pref", base);
    setenv("TNFS_PREF_DIR", pref, 1);                                /* the player's folder: scratch */

    size_t size = 0;
    uint8_t *d;
    char asked[400];

    /* without a mods directory: the data root as always */
    snprintf(asked, sizeof asked, "%s/SIMDATA/MISC/NOTE.TXT", cd);
    d = platform_read_file(asked, &size);
    CHECK(same(d, size, "from the cd"));
    platform_free_file(d);

    CHECK(!platform_set_mods(cd, (snprintf(path, sizeof path, "%s/nope", base), path)));        /* a missing directory */
    CHECK(strstr(platform_mods_error(), "does not exist") != NULL);
    CHECK(platform_set_mods(cd, mods));
    CHECK(strcmp(platform_mods_error(), "") == 0);

    /* the mod's copy wins, the rest comes from the data root */
    d = platform_read_file(asked, &size);
    CHECK(same(d, size, "from the mod"));
    platform_free_file(d);
    snprintf(asked, sizeof asked, "%s/SIMDATA/MISC/ONLYCD.TXT", cd);
    d = platform_read_file(asked, &size);
    CHECK(same(d, size, "cd only"));
    platform_free_file(d);

    /* an invalid mod file is not given to the game, and the error says which and why */
    snprintf(asked, sizeof asked, "%s/SIMDATA/MISC/TR2.TRI", cd);
    d = platform_read_file(asked, &size);
    CHECK(d == NULL);
    CHECK(strstr(platform_mods_error(), "SIMDATA/MISC/TR2.TRI") != NULL);
    CHECK(strstr(platform_mods_error(), "FILE.READABLE") != NULL);
    CHECK(strstr(platform_mods_error(), "0043e4e0") != NULL);                                  /* with the EXE address */

    /* a symbolic link out of the mods directory is refused */
    snprintf(asked, sizeof asked, "%s/SIMDATA/MISC/ESCAPE.TXT", cd);
    d = platform_read_file(asked, &size);
    CHECK(d == NULL);
    CHECK(strstr(platform_mods_error(), "symbolic link") != NULL);

    /* a path that is not below the data root is not an overlay candidate: read as is */
    snprintf(asked, sizeof asked, "%s/outside/secret.txt", base);
    d = platform_read_file(asked, &size);
    CHECK(same(d, size, "secret"));
    platform_free_file(d);

    /* the player's files go to the player's folder, not beside the mod */
    const uint8_t cfg[3] = { 1, 2, 3 };
    CHECK(platform_write_user_file("config.dat", cfg, sizeof cfg));
    snprintf(path, sizeof path, "%s/config.dat", pref);
    struct stat st;
    CHECK(stat(path, &st) == 0 && st.st_size == 3);
    snprintf(path, sizeof path, "%s/user", mods);
    CHECK(stat(path, &st) != 0);                                                               /* the mods directory got no user/ */
    d = platform_read_user_file("config.dat", &size);
    CHECK(d != NULL && size == 3 && memcmp(d, cfg, 3) == 0);
    platform_free_file(d);
    CHECK(!platform_write_user_file("../evil", cfg, sizeof cfg));                              /* plain names only */
    CHECK(!platform_write_user_file("sub/dir", cfg, sizeof cfg));
    CHECK(!platform_write_user_file("..", cfg, sizeof cfg));

    /* saved games: listed by pattern (case ignored, sorted), tested and removed by plain names */
    CHECK(platform_write_user_file("ZED.SAV", cfg, sizeof cfg));
    CHECK(platform_write_user_file("amy.sav", cfg, sizeof cfg));
    CHECK(platform_user_file_exists("ZED.SAV") && !platform_user_file_exists("NONE.SAV"));
    int count = 0;
    char **names = platform_list_user_files("*.SAV", &count);
    CHECK(names != NULL && count == 2 && strcmp(names[0], "amy.sav") == 0 && strcmp(names[1], "ZED.SAV") == 0);
    platform_free_names(names);
    CHECK(platform_list_user_files("*.RPL", &count) == NULL && count == 0);
    CHECK(platform_remove_user_file("ZED.SAV") && !platform_user_file_exists("ZED.SAV"));
    CHECK(!platform_remove_user_file("../config.dat"));
    CHECK(platform_remove_user_file("amy.sav"));

    /* --user DIR: the folder is that one */
    char portable[400], got[1300];
    snprintf(portable, sizeof portable, "%s/portable", base);
    platform_set_user_dir(portable);
    CHECK(platform_user_dir(got, sizeof got) && strcmp(got, (snprintf(path, sizeof path, "%s/", portable), path)) == 0);
    CHECK(platform_write_user_file("port.cfg", cfg, sizeof cfg));
    snprintf(path, sizeof path, "%s/port.cfg", portable);
    CHECK(stat(path, &st) == 0);
    platform_set_user_dir(NULL);
    CHECK(platform_user_dir(got, sizeof got) && strncmp(got, pref, strlen(pref)) == 0);

    /* the folders move once: what the destination lacks comes, what it has stays and the old copy is left alone */
    char old_dir[400], new_dir[400];
    snprintf(old_dir, sizeof old_dir, "%s/old", base);
    snprintf(new_dir, sizeof new_dir, "%s/new", base);
    char old_sub[420];
    snprintf(old_sub, sizeof old_sub, "%s/sub", old_dir);
    write_file(old_dir, "a.cfg", "old a", 5);
    write_file(old_dir, "c.cfg", "old c", 5);
    write_file(old_sub, "b.dat", "old b", 5);
    write_file(new_dir, "c.cfg", "new c", 5);
    CHECK_EQ_INT(platform_move_user_files(old_dir, new_dir), 2);                               /* a.cfg and sub/ */
    snprintf(path, sizeof path, "%s/a.cfg", new_dir);
    CHECK(stat(path, &st) == 0);
    snprintf(path, sizeof path, "%s/sub/b.dat", new_dir);
    CHECK(stat(path, &st) == 0);
    snprintf(path, sizeof path, "%s/a.cfg", old_dir);
    CHECK(stat(path, &st) != 0);                                                               /* moved, not copied */
    snprintf(path, sizeof path, "%s/c.cfg", new_dir);
    FILE *kept = fopen(path, "rb");
    char text[8] = "";
    CHECK(kept != NULL && fread(text, 1, 5, kept) == 5 && memcmp(text, "new c", 5) == 0);      /* the destination's */
    if (kept) fclose(kept);
    snprintf(path, sizeof path, "%s/c.cfg", old_dir);
    CHECK(stat(path, &st) == 0);                                                               /* and the old one intact */
    CHECK_EQ_INT(platform_move_user_files(old_dir, new_dir), 0);                               /* once: nothing more to move */
    CHECK_EQ_INT(platform_move_user_files("/nonexistent_tnfs_dir", new_dir), 0);
    test_older_folders(base);

    /* the default mods directory: <user folder>/mods/ when it is there */
    char default_misc[420];
    snprintf(default_misc, sizeof default_misc, "%s/mods/SIMDATA/MISC", pref);
    CHECK(platform_set_mods(cd, NULL) && platform_mods_dir() == NULL);                        /* no folder, no mods */
    write_file(default_misc, "NOTE.TXT", "from the default mods", 21);
    CHECK(platform_set_mods(cd, NULL) && platform_mods_dir() != NULL);
    snprintf(asked, sizeof asked, "%s/SIMDATA/MISC/NOTE.TXT", cd);
    d = platform_read_file(asked, &size);
    CHECK(same(d, size, "from the default mods"));
    platform_free_file(d);
    snprintf(path, sizeof path, "%s/NOTE.TXT", default_misc);
    unlink(path);                                                                              /* the folder stays, empty: the CD's again */

    /* the data root was not written */
    CHECK(tree_sum(cd) == cd_before);
    CHECK(platform_set_mods(cd, NULL));
    snprintf(asked, sizeof asked, "%s/SIMDATA/MISC/NOTE.TXT", cd);
    d = platform_read_file(asked, &size);
    CHECK(same(d, size, "from the cd"));                                                       /* overlay off again */
    platform_free_file(d);

    char cmd[700];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}

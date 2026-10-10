/* The mods overlay of the platform layer (BYTES-0009 AC8), on a scratch directory: the mod's copy replaces the
 * data root's, everything else comes from the data root, an invalid mod file is refused with the file and the
 * reason, nothing can leave the mods directory, and the player's files go beside the mod, never into the data
 * root. No window is opened. */
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

    /* the player's files go beside the mod */
    const uint8_t cfg[3] = { 1, 2, 3 };
    CHECK(platform_write_user_file("config.dat", cfg, sizeof cfg));
    snprintf(path, sizeof path, "%s/user/config.dat", mods);
    struct stat st;
    CHECK(stat(path, &st) == 0 && st.st_size == 3);
    d = platform_read_user_file("config.dat", &size);
    CHECK(d != NULL && size == 3 && memcmp(d, cfg, 3) == 0);
    platform_free_file(d);
    CHECK(!platform_write_user_file("../evil", cfg, sizeof cfg));                              /* plain names only */
    CHECK(!platform_write_user_file("sub/dir", cfg, sizeof cfg));
    CHECK(!platform_write_user_file("..", cfg, sizeof cfg));

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

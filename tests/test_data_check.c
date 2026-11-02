/* The first-run wizard's checks (sha256.h, data_check.h, data_copy.h): the digest, a folder against a manifest (also one level down, any case of name), the copy. */
#include "game/sys/data_check.h"
#include "game/sys/data_copy.h"
#include "game/sys/data_root.h"
#include "game/sys/sha256.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void hex_of(const char *text, char out[65])
{
    Sha256 s;
    sha256_init(&s);
    sha256_update(&s, text, strlen(text));
    sha256_hex(&s, out);
}

static void test_digests(void)
{
    char hex[65];
    hex_of("", hex);
    CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
    hex_of("abc", hex);
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    hex_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", hex);   /* two blocks, the padding spilling into the second */
    CHECK(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);
    Sha256 s;
    sha256_init(&s);
    for (int i = 0; i < 1000000; i++) sha256_update(&s, "a", 1);   /* one byte at a time: the buffer's edges */
    sha256_hex(&s, hex);
    CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
}

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

static int cancel(void *user, int64_t bytes)
{
    (void)user, (void)bytes;
    return 0;
}

static void test_check_and_copy(void)
{
    char base[] = "/tmp/tnfs_data_check_XXXXXX";
    CHECK(mkdtemp(base) != NULL);
    char path[600], sub[600];
    snprintf(sub, sizeof sub, "%s/NFS", base);
    mkdir(sub, 0755);
    snprintf(path, sizeof path, "%s/simdata", sub);   /* the CD's folder one level down, names in another case */
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/NFS/GAME.EXE", base);
    put(path, "abc");
    snprintf(path, sizeof path, "%s/NFS/simdata/Track.tri", base);
    put(path, "");
    static const DataFile files[2] = {
        { "game.exe", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "SIMDATA/TRACK.TRI", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
    };
    DataCheck c;
    CHECK_EQ_INT(data_check_against(base, files, 2, &c), 1);   /* found one folder down, case ignored */
    CHECK_EQ_INT(c.matched, 2);
    char under[700];
    snprintf(under, sizeof under, "%s/NFS", base);
    CHECK_EQ_INT(data_check_against(under, files, 2, &c), 1);   /* and given directly */
    static const DataFile wrong[2] = { { "game.exe", 3, "00" }, { "nothere.bin", 1, "00" } };
    CHECK_EQ_INT(data_check_against(base, wrong, 2, &c), 0);
    CHECK_EQ_INT(c.different, 1);
    CHECK_EQ_INT(c.missing, 1);
    CHECK_EQ_INT(data_check_against("/no/such/dir", files, 2, &c), 0);
    CHECK_EQ_INT(c.missing, 2);
    CHECK_EQ_INT(data_check_against(NULL, files, 2, &c), 0);
    /* the copy: all of it, the progress told, never over what is there, a cancel leaves nothing */
    CHECK_EQ_INT((int)data_copy_size(under), 3);
    char dest[700], part[720];
    snprintf(dest, sizeof dest, "%s/copy", base);
    snprintf(part, sizeof part, "%s.part", dest);
    CHECK_EQ_INT(data_copy_tree(under, dest, NULL, NULL), 1);
    CHECK_EQ_INT(data_check_against(dest, files, 2, &c), 1);
    CHECK_EQ_INT(data_copy_tree(under, dest, NULL, NULL), 1);   /* it exists: replaced as a whole */
    CHECK_EQ_INT(data_check_against(dest, files, 2, &c), 1);
    char aside[720];
    snprintf(aside, sizeof aside, "%s.old", dest);
    CHECK(access(aside, F_OK) != 0);
    CHECK_EQ_INT(data_copy_tree(under, dest, cancel, NULL), 0);   /* a cancelled replacement leaves the old copy */
    CHECK_EQ_INT(data_check_against(dest, files, 2, &c), 1);
    char again[700];
    snprintf(again, sizeof again, "%s/again", base);
    CHECK_EQ_INT(data_copy_tree(under, again, (DataCopyProgress)NULL, NULL), 1);
    snprintf(dest, sizeof dest, "%s/cancelled", base);
    CHECK_EQ_INT(data_copy_tree(under, dest, cancel, NULL), 0);
    CHECK(access(dest, F_OK) != 0);
    snprintf(part, sizeof part, "%s.part", dest);
    CHECK(access(part, F_OK) != 0);
    snprintf(path, sizeof path, "find %s -delete", base);   /* the scratch tree */
    CHECK_EQ_INT(system(path), 0);
}

/* the copy never follows a link to a folder (the owner's source had original/original -> original, a loop), copies a link to a file as the file, copes with read-only
 * files (the CD's are r--------) and says what failed: the path and the reason, not a guess about the disk */
static void test_copy_links_and_read_only(void)
{
    char base[] = "/tmp/tnfs_data_copy_XXXXXX";
    CHECK(mkdtemp(base) != NULL);
    char src[600], path[1300], dest[600];
    snprintf(src, sizeof src, "%s/src", base);
    mkdir(src, 0755);
    snprintf(path, sizeof path, "%s/sub", src);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/sub/A.BIN", src);
    put(path, "abc");
    chmod(path, 0400);   /* as on the CD */
    snprintf(path, sizeof path, "%s/loop", src);
    CHECK_EQ_INT(symlink(src, path), 0);   /* a folder that links to its own parent */
    snprintf(path, sizeof path, "%s/sub/up", src);
    CHECK_EQ_INT(symlink("..", path), 0);
    snprintf(path, sizeof path, "%s/alias.bin", src);
    CHECK_EQ_INT(symlink("sub/A.BIN", path), 0);   /* a link to a file */
    snprintf(path, sizeof path, "%s/dangling", src);
    CHECK_EQ_INT(symlink("/no/such/file", path), 0);
    CHECK_EQ_INT((int)data_copy_size(src), 6);   /* A.BIN and the alias; the links to folders are not entered */
    snprintf(dest, sizeof dest, "%s/dest", base);
    CHECK_EQ_INT(data_copy_tree(src, dest, NULL, NULL), 1);
    CHECK(strcmp(data_copy_error(), "") == 0);
    snprintf(path, sizeof path, "%s/sub/A.BIN", dest);
    CHECK(access(path, R_OK) == 0);
    snprintf(path, sizeof path, "%s/alias.bin", dest);
    struct stat st;
    CHECK(lstat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 3);   /* the file's contents, not a link */
    snprintf(path, sizeof path, "%s/loop", dest);
    CHECK(lstat(path, &st) != 0);   /* nothing made of the link to a folder */
    /* a failure: an unreadable file says which, and leaves nothing behind (a read-only .part from before is cleared) */
    snprintf(path, sizeof path, "%s/sub/A.BIN", src);
    chmod(path, 0000);
    snprintf(dest, sizeof dest, "%s/dest2", base);
    char part[700];
    snprintf(part, sizeof part, "%s.part", dest);
    mkdir(part, 0755);
    snprintf(path, sizeof path, "%s/stale", part);
    put(path, "x");
    chmod(path, 0400);
    chmod(part, 0500);
    if (geteuid() != 0) {   /* root reads anything */
        CHECK_EQ_INT(data_copy_tree(src, dest, NULL, NULL), 0);
        CHECK(strstr(data_copy_error(), "A.BIN") != NULL);
        CHECK(strstr(data_copy_error(), "Permission denied") != NULL);
    }
    CHECK(access(dest, F_OK) != 0);
    CHECK(access(part, F_OK) != 0);
    snprintf(path, sizeof path, "%s/sub/A.BIN", src);
    chmod(path, 0400);
    CHECK_EQ_INT(data_copy_tree(src, dest, NULL, NULL), 1);   /* and the retry works */
    snprintf(path, sizeof path, "find %s -delete", base);
    CHECK_EQ_INT(system(path), 0);
}

/* where the data is: --data as given, the player's copy, a development folder, else the wizard */
/* "Use it anyway" (a folder whose NFS_WIN.EXE is another size): the copy is marked, the next start takes it with no wizard, and a new copy replaces it */
static void test_use_it_anyway_then_restart(void)
{
    char base[] = "/tmp/tnfs_data_anyway_XXXXXX";
    CHECK(mkdtemp(base) != NULL);
    char src[600], dest[600], path[1300], out[1024];
    snprintf(src, sizeof src, "%s/src", base);
    mkdir(src, 0755);
    snprintf(path, sizeof path, "%s/%s", src, DATA_MANIFEST[0].path);
    put(path, "another edition");   /* not the manifest's size */
    snprintf(dest, sizeof dest, "%s/gamedata", base);
    DataCheck c;
    CHECK_EQ_INT(data_check(src, &c), 0);
    CHECK(c.root[0] != '\0' && c.different >= 1);
    CHECK_EQ_INT(data_copy_tree(src, dest, NULL, NULL), 1);
    CHECK_EQ_INT(data_looks_installed(dest), 0);   /* the start check alone fails: this is the wizard again... */
    CHECK_EQ_INT((int)data_root_resolve(NULL, dest, NULL, out, sizeof out), (int)DATA_ROOT_NEEDS_SETUP);
    CHECK_EQ_INT(data_mark_accepted(dest, &c), 1);   /* ...unless the accepted copy was marked */
    CHECK_EQ_INT(data_looks_installed(dest), 1);
    CHECK_EQ_INT((int)data_root_resolve(NULL, dest, NULL, out, sizeof out), (int)DATA_ROOT_USER_COPY);
    CHECK_EQ_INT(data_copy_tree(src, dest, NULL, NULL), 1);   /* a new copy over the old one, no "File exists" */
    CHECK_EQ_INT(data_looks_installed(dest), 0);   /* the replacement is whole and unmarked until the wizard marks it */
    snprintf(path, sizeof path, "find %s -delete", base);
    CHECK_EQ_INT(system(path), 0);
}

static void test_the_root(void)
{
    char out[1024];
    CHECK_EQ_INT((int)data_root_resolve("/x/y", "/no/copy", "/no/dev", out, sizeof out), (int)DATA_ROOT_EXPLICIT);   /* given: taken as it is, even if it is nothing */
    CHECK(strcmp(out, "/x/y") == 0);
    CHECK_EQ_INT((int)data_root_resolve(NULL, "/no/copy", NULL, out, sizeof out), (int)DATA_ROOT_NEEDS_SETUP);
    CHECK(out[0] == '\0');
    CHECK_EQ_INT((int)data_root_resolve(NULL, NULL, NULL, out, sizeof out), (int)DATA_ROOT_NEEDS_SETUP);
    const char *dir = getenv("TNFS_DATA");
#ifdef TNFS_SOURCE_DIR
    if (dir == NULL) dir = TNFS_SOURCE_DIR "/original";
#endif
    if (dir != NULL && data_looks_installed(dir)) {
        CHECK_EQ_INT((int)data_root_resolve(NULL, dir, "/no/dev", out, sizeof out), (int)DATA_ROOT_USER_COPY);   /* the copy first */
        CHECK_EQ_INT((int)data_root_resolve(NULL, "/no/copy", dir, out, sizeof out), (int)DATA_ROOT_DEV);
        CHECK(strcmp(out, dir) == 0);
    }
}

/* the real CD's files (the manifest was made from them): $TNFS_DATA or original/ of the source tree; skipped when it is not there */
static void test_the_real_cd(void)
{
    const char *dir = getenv("TNFS_DATA");
#ifdef TNFS_SOURCE_DIR
    if (dir == NULL) dir = TNFS_SOURCE_DIR "/original";
#endif
    DataCheck c;
    if (dir == NULL || !data_looks_installed(dir)) return;
    CHECK_EQ_INT(data_check(dir, &c), 1);
    CHECK_EQ_INT(c.matched, DATA_MANIFEST_COUNT);
    CHECK(c.total == DATA_MANIFEST_COUNT && DATA_MANIFEST_COUNT >= 10);
}

int main(void)
{
    test_digests();
    test_check_and_copy();
    test_copy_links_and_read_only();
    test_use_it_anyway_then_restart();
    test_the_root();
    test_the_real_cd();
    return TEST_REPORT();
}

/* exe_tables_load / exe_tables_load_dir (game/exe/exe_tables.c): the six families together, all or nothing, and the messages
 * for a folder with no exe, an exe that is not the PC SE's, and a family that cannot be read. With original/ there, the real
 * exe is loaded and compared with the tables the port still holds. */
#include "exe_pe.h"
#include "game/exe/exe_tables.h"
#include "tests/exe_tables_family.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_missing_and_foreign(void)
{
    ExeTables *t = malloc(sizeof *t);
    memset(t, 0xAB, sizeof *t);
    ExeTables *keep = malloc(sizeof *keep);
    memset(keep, 0xAB, sizeof *keep);
    char err[1024] = "";
    CHECK(exe_tables_load_dir("/nonexistent/folder", t, err, sizeof err) != 0);
    CHECK(strstr(err, "NFS_WIN.EXE not found") != NULL);
    CHECK(strstr(err, "/nonexistent/folder") != NULL);
    CHECK(exe_tables_load_dir(NULL, t, err, sizeof err) != 0);

    char dir[256];
    const char *tmp = getenv("TMPDIR");
    snprintf(dir, sizeof dir, "%s/tnfs_exe_tables_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    CHECK(mkdtemp(dir) != NULL);
    char file[300];
    snprintf(file, sizeof file, "%s/nfs_win.exe", dir);   /* the name in lower case: found all the same */
    FILE *f = fopen(file, "wb");
    CHECK(f != NULL);
    if (f) { fputs("not the exe", f); fclose(f); }
    err[0] = 0;
    CHECK(exe_tables_load_dir(dir, t, err, sizeof err) != 0);
    CHECK(strstr(err, "not the PC Special Edition build") != NULL);
    CHECK(strstr(err, dir) != NULL);
    CHECK(strstr(err, "1069056") != NULL);
    CHECK_MEM(t, keep, sizeof *t);                          /* untouched on failure */
    unlink(file);
    rmdir(dir);
    free(t);
    free(keep);
}

static void test_all_or_nothing(void)
{
    ExeTables *t = malloc(sizeof *t), *keep = malloc(sizeof *keep);
    memset(t, 0xAB, sizeof *t);
    memset(keep, 0xAB, sizeof *keep);
    TestExe x = exe_pe_make(0x4B0000, 0x100);   /* a PE with none of the tables */
    ExeImage img;
    char err[512] = "";
    CHECK_EQ_INT(exe_image_open(&img, x.data, x.len, err, sizeof err), 0);
    CHECK(exe_tables_extract(&img, t, err, sizeof err) != 0);
    CHECK(strstr(err, "NFS_WIN.EXE") != NULL);
    CHECK(strstr(err, "tables") != NULL);
    CHECK_MEM(t, keep, sizeof *t);
    free(x.data);
    free(t);
    free(keep);
}

#ifdef TNFS_SOURCE_DIR
static void test_real_exe(void)
{
    char dir[1024];
    snprintf(dir, sizeof dir, "%s/original", TNFS_SOURCE_DIR);
    char path[1100];
    snprintf(path, sizeof path, "%s/NFS_WIN.EXE", dir);
    if (access(path, R_OK) != 0) { fprintf(stderr, "no original/NFS_WIN.EXE: the real exe's load skipped\n"); return; }
    ExeTables *t = calloc(1, sizeof *t);
    char err[1024] = "";
    CHECK_EQ_INT(exe_tables_load_dir(dir, t, err, sizeof err), 0);
    CHECK(family_ok(t, "ai") && family_ok(t, "split") && family_ok(t, "view") && family_ok(t, "track") && family_ok(t, "menu") && family_ok(t, "config"));
    ExeTables *again = calloc(1, sizeof *again);
    CHECK_EQ_INT(exe_tables_load_dir(dir, again, err, sizeof err), 0);   /* idempotent: the same result */
    CHECK_MEM(t, again, sizeof *t);
    free(again);
    free(t);
}
#endif

int main(void)
{
    test_missing_and_foreign();
    test_all_or_nothing();
#ifdef TNFS_SOURCE_DIR
    test_real_exe();
#endif
    return TEST_REPORT();
}

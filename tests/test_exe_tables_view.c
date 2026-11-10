/* The viewport tables read from the exe (game/exe/exe_tables_view.c) are the tables the fixture holds (tests/exe_tables_fixture.h). */
#include "tests/exe_tables_support.h"
#include "tests/exe_tables_family.h"
#include "tests/test.h"

static void test_real(void)
{
    size_t n = 0;
    uint8_t *b = real_exe(&n);
    if (b == NULL) { fprintf(stderr, "no original/NFS_WIN.EXE: the real exe's part skipped\n"); return; }
    ExeImage img;
    char err[256] = "";
    CHECK_EQ_INT(exe_image_open(&img, b, n, err, sizeof err), 0);
    ExeTables *t = malloc(sizeof *t);
    memset(t, 0xAA, sizeof *t);
    CHECK_EQ_INT(exe_tables_view(&img, t, err, sizeof err), 0);
    CHECK(family_ok(t, "view"));
    free(t), free(b);
}

static void test_missing_data(void)
{
    size_t n = 0;
    uint8_t *b = tiny_exe(&n);
    ExeImage img;
    char err[256] = "";
    CHECK_EQ_INT(exe_image_open(&img, b, n, err, sizeof err), 0);
    ExeTables *t = calloc(1, sizeof *t);
    CHECK_EQ_INT(exe_tables_view(&img, t, err, sizeof err), -1);
    CHECK(strstr(err, "004b7080") != NULL);
    free(t), free(b);
}

int main(void)
{
    test_missing_data();
    test_real();
    return TEST_REPORT();
}

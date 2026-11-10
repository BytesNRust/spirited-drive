/* The default records read from the exe (exe_tables_config.c): the numbers are text in the exe, parsed the way 00481f93 parses
 * them; the rows and names must be what config_records.c holds today. The parsing rules and the errors run on an invented
 * image; with original/ there, the real exe's rows are compared with the generated table. */
#include "exe_pe.h"
#include "game/exe/exe_tables.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SEC_VA = 0x4AE000, SEC_SIZE = 0x14000, STR_VA = 0x4C0000, NAMES_VA = 0x4AEC24 };

/* one row of the invented table: a name and five number strings at 0x4C0000 + 0x100 * row + 0x20 * n */
static void put_row(TestExe *x, int row, const char *name, const char *const num[5])
{
    uint32_t at = CONFIG_RECORD_FIRST + (uint32_t)row * CONFIG_RECORD_ROW_SIZE, s = STR_VA + 0x100u * (uint32_t)row;
    exe_pe_pokes(x, s, name);
    exe_pe_poke32(x, at, s);
    for (int n = 0; n < 5; n++) {
        exe_pe_pokes(x, s + 0x20u * (uint32_t)(n + 1), num[n]);
        exe_pe_poke32(x, at + 4u * (uint32_t)(n + 1), s + 0x20u * (uint32_t)(n + 1));
    }
}

static TestExe invented(void)
{
    /* the rows are 244 * 0x100 bytes of strings: past 0x4C0000 the section must reach 0x4D0000 */
    TestExe x = exe_pe_make(SEC_VA, 0x30000);
    const char *const plain[5] = { "1", "2", "3", "4", "5" };
    for (int r = 0; r < CONFIG_RECORD_ROW_COUNT; r++) put_row(&x, r, "Row", plain);
    exe_pe_pokes(&x, NAMES_VA, "One");
    exe_pe_pokes(&x, NAMES_VA + 8, "Two");
    exe_pe_pokes(&x, NAMES_VA + 12, "Three");
    return x;
}

static int run(TestExe *x, ExeTables *t, char *err, size_t errsz)
{
    ExeImage img;
    char e[128];
    if (exe_image_open(&img, x->data, x->len, e, sizeof e) != 0) { snprintf(err, errsz, "open: %s", e); return -1; }
    return exe_tables_config(&img, t, err, errsz);
}

static void test_atoi_rules(void)
{
    TestExe x = invented();
    const char *const num[5] = { "  12", "-7", "+3", "4294967295", "2147483648" };
    put_row(&x, 0, "Greenbeard", num);
    const char *const odd[5] = { "abc", "12abc", "\t \n-5", "", "-2147483648" };
    put_row(&x, 1, "Odd", odd);
    ExeTables *t = calloc(1, sizeof *t);
    char err[256] = "";
    CHECK_EQ_INT(run(&x, t, err, sizeof err), 0);
    CHECK(strcmp(t->config_row_name[0], "Greenbeard") == 0);
    CHECK_EQ_INT(t->config_row_number[0][0], 12);
    CHECK_EQ_INT(t->config_row_number[0][1], -7);
    CHECK_EQ_INT(t->config_row_number[0][2], 3);
    CHECK_EQ_INT(t->config_row_number[0][3], -1);                    /* wraps at 32 bits, as the exe's */
    CHECK_EQ_INT(t->config_row_number[0][4], (int32_t)0x80000000u);
    CHECK_EQ_INT(t->config_row_number[1][0], 0);                     /* no digits */
    CHECK_EQ_INT(t->config_row_number[1][1], 12);                    /* stops at the first non-digit */
    CHECK_EQ_INT(t->config_row_number[1][2], -5);                    /* blanks, then a sign */
    CHECK_EQ_INT(t->config_row_number[1][3], 0);                     /* an empty string */
    CHECK_EQ_INT(t->config_row_number[1][4], (int32_t)0x80000000u);
    CHECK(strcmp(t->config_name[0], "One") == 0);
    CHECK(strcmp(t->config_name[1], "Two") == 0);
    CHECK(strcmp(t->config_name[2], "Three") == 0);
    free(t);
    free(x.data);
}

static void test_errors(void)
{
    ExeTables *t = calloc(1, sizeof *t);
    char err[256];
    TestExe x = invented();
    exe_pe_poke32(&x, CONFIG_RECORD_FIRST + 24u * 5 + 8, 0x10);   /* a number's pointer in no section */
    err[0] = 0;
    CHECK(run(&x, t, err, sizeof err) != 0);
    CHECK(strstr(err, "row 5") != NULL);
    CHECK(strstr(err, "00000010") != NULL);
    free(x.data);
    x = invented();
    char longname[64];
    memset(longname, 'x', sizeof longname - 1);
    longname[sizeof longname - 1] = 0;
    exe_pe_pokes(&x, STR_VA + 0x100u * 7, longname);   /* row 7's name does not fit */
    err[0] = 0;
    CHECK(run(&x, t, err, sizeof err) != 0);
    CHECK(strstr(err, "row 7") != NULL);
    free(x.data);
    x = invented();
    exe_pe_poke32(&x, CONFIG_RECORD_FIRST, 0);   /* row 0's name at address 0 */
    err[0] = 0;
    CHECK(run(&x, t, err, sizeof err) != 0);
    free(x.data);
    TestExe small = exe_pe_make(0x4B0000, 0x100);   /* the table is not in the image at all */
    err[0] = 0;
    CHECK(run(&small, t, err, sizeof err) != 0);
    CHECK(strstr(err, "config") != NULL);
    free(small.data);
    free(t);
}

#ifdef TNFS_SOURCE_DIR
static void test_real_exe(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/original/NFS_WIN.EXE", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "no original/NFS_WIN.EXE: the real exe's records skipped\n"); return; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    CHECK(fread(b, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    ExeImage img;
    char err[256] = "";
    CHECK_EQ_INT(exe_image_open(&img, b, (size_t)n, err, sizeof err), 0);
    ExeTables *t = calloc(1, sizeof *t);
    CHECK_EQ_INT(exe_tables_config(&img, t, err, sizeof err), 0);
    for (int r = 0; r < CONFIG_RECORD_ROW_COUNT; r++) {
        CHECK(strcmp(t->config_row_name[r], CONFIG_RECORD_ROWS[r].name) == 0);
        CHECK_MEM(t->config_row_number[r], CONFIG_RECORD_ROWS[r].number, sizeof t->config_row_number[r]);
    }
    for (int i = 0; i < 3; i++) CHECK(strcmp(t->config_name[i], CONFIG_RECORD_NAMES[i]) == 0);
    free(t);
    free(b);
}
#endif

int main(void)
{
    test_atoi_rules();
    test_errors();
#ifdef TNFS_SOURCE_DIR
    test_real_exe();
#endif
    return TEST_REPORT();
}

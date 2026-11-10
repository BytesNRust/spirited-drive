/* The in-race menu's constants and map points read from the exe (exe_tables_menu.c): the items' places, the map's offsets, the
 * shape names and every map's points in one pool. The reading and the errors run on an invented image; with original/ there,
 * everything is checked against the fixture (tests/exe_tables_fixture.h). */
#include "exe_pe.h"
#include "game/exe/exe_tables.h"
#include "tests/exe_tables_family.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SEC_VA = 0x447000, SEC_SIZE = 0x7C000, ITEMS = 0x447410, XOFF = 0x447428, YOFF = 0x44745E, TRACKS = 0x447494,
       MARKERS = 0x4474B8, COUNTS0 = 0x4474D8, COUNTS1 = 0x4474EA, TEXT = 0x447800,
       MAP00 = 0x4BFB56, MAP01 = 0x4C02D6, MAP30 = 0x4C124E };

static TestExe invented(void)
{
    TestExe x = exe_pe_make(SEC_VA, SEC_SIZE);
    for (int i = 0; i < 12; i++) exe_pe_poke16(&x, ITEMS + 2u * (uint32_t)i, (uint16_t)(100 + i));
    for (int i = 0; i < 27; i++) {
        exe_pe_poke16(&x, XOFF + 2u * (uint32_t)i, (uint16_t)(-i));
        exe_pe_poke16(&x, YOFF + 2u * (uint32_t)i, (uint16_t)(7 * i));
    }
    for (int i = 0; i < 9; i++) {
        char name[16];
        snprintf(name, sizeof name, "trk%d", i);
        exe_pe_pokes(&x, TEXT + 16u * (uint32_t)i, name);
        exe_pe_poke32(&x, TRACKS + 4u * (uint32_t)i, TEXT + 16u * (uint32_t)i);
    }
    for (int i = 0; i < 8; i++) {
        char name[8];
        snprintf(name, sizeof name, "m%d", i);
        exe_pe_pokes(&x, TEXT + 0x100u + 8u * (uint32_t)i, name);
        exe_pe_poke32(&x, MARKERS + 4u * (uint32_t)i, TEXT + 0x100u + 8u * (uint32_t)i);
    }
    return x;
}

static int run(TestExe *x, ExeTables *t, char *err, size_t errsz)
{
    ExeImage img;
    char e[128];
    if (exe_image_open(&img, x->data, x->len, e, sizeof e) != 0) return -1;
    return exe_tables_menu(&img, t, err, errsz);
}

static void test_reading(void)
{
    TestExe x = invented();
    exe_pe_poke16(&x, COUNTS0 + 0, 3);    /* track 0, variant 0: 3 points */
    exe_pe_poke16(&x, COUNTS0 + 2, 2);    /* track 0, variant 1 */
    exe_pe_poke16(&x, COUNTS1 + 0, 1);    /* track 3 */
    for (int i = 0; i < 6; i++) exe_pe_poke16(&x, MAP00 + 2u + 2u * (uint32_t)i, (uint16_t)(10 + i));
    for (int i = 0; i < 4; i++) exe_pe_poke16(&x, MAP01 + 2u + 2u * (uint32_t)i, (uint16_t)(50 + i));
    exe_pe_poke16(&x, MAP30 + 2u, 900);
    exe_pe_poke16(&x, MAP30 + 4u, (uint16_t)-900);
    ExeTables *t = calloc(1, sizeof *t);
    char err[256] = "";
    CHECK_EQ_INT(run(&x, t, err, sizeof err), 0);
    CHECK_EQ_INT(t->menu_item_x[0], 100); CHECK_EQ_INT(t->menu_item_x[5], 105);
    CHECK_EQ_INT(t->menu_item_y[0], 106); CHECK_EQ_INT(t->menu_item_y[5], 111);
    CHECK_EQ_INT(t->menu_x_offset[26], -26);
    CHECK_EQ_INT(t->menu_y_offset[26], 7 * 26);
    CHECK(strcmp(t->menu_track_name[8], "trk8") == 0);
    CHECK(strcmp(t->menu_marker_name[7], "m7") == 0);
    CHECK_EQ_INT(t->menu_map_count[0][0], 3);
    CHECK_EQ_INT(t->menu_map_count[0][1], 2);
    CHECK_EQ_INT(t->menu_map_count[0][2], 0);
    CHECK_EQ_INT(t->menu_map_count[3][0], 1);
    CHECK_EQ_INT(t->menu_map_count[3][1], 0);                        /* tracks 3..8 have one map */
    CHECK_EQ_INT(t->menu_map_first[0][0], 0);                        /* the pool is in track, variant order */
    CHECK_EQ_INT(t->menu_map_first[0][1], 3);
    CHECK_EQ_INT(t->menu_map_first[3][0], 5);
    CHECK_EQ_INT(t->menu_pool[0][0], 10); CHECK_EQ_INT(t->menu_pool[2][1], 15);
    CHECK_EQ_INT(t->menu_pool[3][0], 50); CHECK_EQ_INT(t->menu_pool[4][1], 53);
    CHECK_EQ_INT(t->menu_pool[5][0], 900); CHECK_EQ_INT(t->menu_pool[5][1], -900);
    free(t);
    free(x.data);
}

static void test_errors(void)
{
    ExeTables *t = calloc(1, sizeof *t);
    char err[256];
    TestExe x = invented();
    for (int i = 0; i < 9; i++) exe_pe_poke16(&x, COUNTS0 + 2u * (uint32_t)i, 500);   /* 4500 points */
    for (int i = 0; i < 7; i++) exe_pe_poke16(&x, COUNTS1 + 2u * (uint32_t)i, 500);   /* + 3500: past the pool */
    err[0] = 0;
    CHECK(run(&x, t, err, sizeof err) != 0);
    CHECK(strstr(err, "pool") != NULL);
    free(x.data);
    x = invented();
    exe_pe_poke16(&x, COUNTS0 + 4, (uint16_t)-1);                                     /* a negative count */
    err[0] = 0;
    CHECK(run(&x, t, err, sizeof err) != 0);
    CHECK(strstr(err, "count") != NULL);
    free(x.data);
    x = invented();
    exe_pe_poke32(&x, TRACKS + 12, 0x10);                                             /* a name in no section */
    err[0] = 0;
    CHECK(run(&x, t, err, sizeof err) != 0);
    CHECK(strstr(err, "00000010") != NULL);
    free(x.data);
    x = invented();
    exe_pe_pokes(&x, TEXT + 0x100u, "toolong");                                       /* a marker of 7 letters: room for 4 */
    err[0] = 0;
    CHECK(run(&x, t, err, sizeof err) != 0);
    CHECK(strstr(err, "marker") != NULL);
    free(x.data);
    TestExe small = exe_pe_make(0x4B0000, 0x100);                                     /* the tables are not in the image */
    err[0] = 0;
    CHECK(run(&small, t, err, sizeof err) != 0);
    CHECK(strstr(err, "menu") != NULL);
    free(small.data);
    free(t);
}

#ifdef TNFS_SOURCE_DIR
static void test_real_exe(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/original/NFS_WIN.EXE", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "no original/NFS_WIN.EXE: the real exe's menu tables skipped\n"); return; }
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
    CHECK_EQ_INT(exe_tables_menu(&img, t, err, sizeof err), 0);
    CHECK(family_ok(t, "menu"));
    free(t);
    free(b);
}
#endif

int main(void)
{
    test_reading();
    test_errors();
#ifdef TNFS_SOURCE_DIR
    test_real_exe();
#endif
    return TEST_REPORT();
}

/* The viewport tables read from the exe (game/exe/exe_tables_view.c) are the tables the port holds now. */
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
    for (int m = 0; m < VIEW_SCREEN_MODES; m++) {
        for (int r = 0; r < VIEW_LAYOUTS; r++) {   /* by field: no padding to trust */
            const ViewLayout *g = &t->race_view_layouts[m][r], *w = &race_view_layouts[m][r];
            CHECK(g->unknown_00 == w->unknown_00 && g->scale == w->scale && g->x == w->x && g->y == w->y && g->width == w->width);
            CHECK(g->height == w->height && g->left == w->left && g->top == w->top && g->unknown_20 == w->unknown_20 && g->unknown_24 == w->unknown_24);
        }
        for (int r = 0; r < VIEW_ROWS; r++) {
            const ViewRow *g = &t->race_view_rows[m][r], *w = &race_view_rows[m][r];
            CHECK_EQ_INT(g->next, w->next);
            CHECK_MEM(g->layouts, w->layouts, sizeof w->layouts);
            CHECK_MEM(g->unknown_14, w->unknown_14, sizeof w->unknown_14);
        }
    }
    CHECK_MEM(t->race_view_model_y, race_view_model_y, sizeof race_view_model_y);
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

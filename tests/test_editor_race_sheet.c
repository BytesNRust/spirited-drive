/* Sheets (editor/race_sheet.h): the cells, the photos they take (halved from render scale 2), the grid and its labels, a failed or pending cell, the PNG. */
#include "race_sheet.h"
#include "formats/png.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint8_t *solid(int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t *p = malloc((size_t)w * (size_t)h * 4);
    for (int i = 0; p != NULL && i < w * h; ++i) p[i * 4] = r, p[i * 4 + 1] = g, p[i * 4 + 2] = b, p[i * 4 + 3] = 255;
    return p;
}

static const uint8_t *at(const uint8_t *img, int w, int x, int y) { return img + ((size_t)y * (size_t)w + (size_t)x) * 4; }

static int is(const uint8_t *px, uint8_t r, uint8_t g, uint8_t b) { return px[0] == r && px[1] == g && px[2] == b; }

/* the label band of a cell holds some pixel that is not the band's background */
static int label_has_ink(const uint8_t *img, int w, int cx, int cy)
{
    const uint8_t *bg = at(img, w, cx + SHEET_CELL_W - 1, cy + SHEET_CELL_H + SHEET_LABEL_H - 1);   /* the band's far corner: background */
    for (int y = cy + SHEET_CELL_H; y < cy + SHEET_CELL_H + SHEET_LABEL_H; ++y)
        for (int x = cx; x < cx + SHEET_CELL_W; ++x)
            if (memcmp(at(img, w, x, y), bg, 3) != 0) return 1;
    return 0;
}

static void test_grid(void)
{
    RaceSheet s;
    sheet_init(&s, 2);
    CHECK(sheet_add(&s, "Day", car_export_car(0), "/p/a", "rusty", 0) == 0);
    CHECK(sheet_add(&s, "Night", car_export_car(0), "/p/a", "rusty", 1) == 1);
    CHECK(sheet_add(&s, "Third", car_export_car(0), "/p/a", "rusty", 1) == 2);
    CHECK(sheet_next(&s) == 0 && !sheet_complete(&s));
    uint8_t *big = solid(1280, 960, 200, 10, 10), *small = solid(640, 480, 10, 200, 10), *odd = solid(300, 200, 0, 0, 0);
    CHECK(sheet_set_photo(&s, 0, big, 1280, 960) && sheet_next(&s) == 1);
    CHECK(sheet_set_photo(&s, 1, small, 640, 480) && sheet_next(&s) == 2);
    CHECK(!sheet_set_photo(&s, 2, odd, 300, 200) && s.cell[2].state == SHEET_FAILED && s.cell[2].note[0] != '\0');   /* another size: failed, not silently skipped */
    CHECK(sheet_next(&s) == -1 && sheet_complete(&s) && sheet_finished_count(&s) == 3);
    uint8_t *img = NULL;
    int w = 0, h = 0;
    char why[160] = "";
    CHECK(sheet_compose(&s, &img, &w, &h, why, sizeof why));
    CHECK(w == 2 * SHEET_CELL_W && h == 2 * (SHEET_CELL_H + SHEET_LABEL_H));
    if (img != NULL && w == 2 * SHEET_CELL_W) {
        CHECK(is(at(img, w, 100, 100), 200, 10, 10));                                    /* cell 0: the halved big photo */
        CHECK(is(at(img, w, SHEET_CELL_W + 100, 100), 10, 200, 10));                     /* cell 1, in the second column */
        CHECK(!is(at(img, w, 100, SHEET_CELL_H + SHEET_LABEL_H + 100), 200, 10, 10));     /* cell 2 (failed) is not a photo's colour */
        CHECK(label_has_ink(img, w, 0, 0) && label_has_ink(img, w, SHEET_CELL_W, 0) && label_has_ink(img, w, 0, SHEET_CELL_H + SHEET_LABEL_H));
    }
    free(img), free(big), free(small), free(odd);
    sheet_free(&s);
}

static void test_halving(void)
{
    /* a 2 x 2 block of 1280 x 960 averages into one pixel */
    uint8_t *big = solid(1280, 960, 0, 0, 0);
    uint8_t *p = big;
    p[0] = 100, p[4] = 200;   /* the block's first row: 100 and 200 in red */
    RaceSheet s;
    sheet_init(&s, 1);
    sheet_add(&s, "X", car_export_car(0), "/p", "rusty", 0);
    CHECK(sheet_set_photo(&s, 0, big, 1280, 960));
    CHECK(s.cell[0].rgba != NULL && s.cell[0].rgba[0] == 75);   /* (100 + 200 + 0 + 0) / 4 */
    free(big);
    sheet_free(&s);
}

static void test_empty_pending_failed(void)
{
    RaceSheet s;
    sheet_init(&s, 3);
    uint8_t *img = NULL;
    int w, h;
    char why[160] = "";
    CHECK(!sheet_compose(&s, &img, &w, &h, why, sizeof why) && why[0] != '\0' && img == NULL);   /* nothing to lay out */
    CHECK(!sheet_complete(&s) && sheet_next(&s) == -1);
    sheet_add(&s, "Pending", NULL, "", "", 0);
    sheet_add(&s, "Failed", NULL, "", "", 0);
    sheet_set_failed(&s, 1, "the game ended with an error");
    CHECK(sheet_next(&s) == 0 && !sheet_complete(&s) && s.cell[1].state == SHEET_FAILED);
    CHECK(sheet_compose(&s, &img, &w, &h, why, sizeof why) && w == 2 * SHEET_CELL_W && h == SHEET_CELL_H + SHEET_LABEL_H);   /* a partial sheet still draws, as wide as its cells (3 columns, 2 cells) */
    free(img);
    for (int i = 0; i < SHEET_CELLS; ++i) sheet_add(&s, "x", NULL, "", "", 0);
    CHECK(sheet_add(&s, "one too many", NULL, "", "", 0) == -1 && s.count == SHEET_CELLS);
    sheet_free(&s);
}

static void test_png(void)
{
    RaceSheet s;
    sheet_init(&s, 2);
    sheet_add(&s, "Day", car_export_car(0), "/p", "rusty", 0);
    uint8_t *small = solid(640, 480, 10, 200, 10);
    sheet_set_photo(&s, 0, small, 640, 480);
    char path[256], why[160] = "";
    snprintf(path, sizeof path, "%s/race-sheet-test-%d.png", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp", (int)getpid());
    CHECK(sheet_write_png(&s, path, why, sizeof why));
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        uint8_t head[24] = { 0 };
        const size_t got = fread(head, 1, sizeof head, f);
        fclose(f);
        CHECK(got == sizeof head && memcmp(head + 1, "PNG", 3) == 0);
        remove(path);
    }
    CHECK(!sheet_write_png(&s, "/nonexistent-folder/x.png", why, sizeof why) && why[0] != '\0');
    free(small);
    sheet_free(&s);
}

int main(void)
{
    test_grid();
    test_halving();
    test_empty_pending_failed();
    test_png();
    return TEST_REPORT();
}

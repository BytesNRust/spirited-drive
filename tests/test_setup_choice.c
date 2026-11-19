/* owner 2026-11-19, 2026-11-19 (the second answer): the SETUP panel in two columns. The labels in the first, as wide as the widest label; in the
 * second the selectors and values: on a Color or Skin row the box next to the left arrow, the name centred in a span as wide as the longest colour
 * name of the CD's cars, then the right arrow. The panel's width is their sum and it is centred with air at the sides. The geometry needs no data; the
 * widths (label column, name span, every colour name fitting at the row's own size) need the CD's font and are skipped without original/. */
#include "game/frontend/added_gallery_page.h"
#include "game/frontend/gallery_layout.h"
#include "game/race/car_paint.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "test.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* a click on the middle of each row's capitals is that row, and a switch's step is 1 (Transmission, ABS, Traction control) */
static void rows(void)
{
    int step;
    for (int row = 0; row < GALLERY_SETUP_ROWS; row++) {
        const int mid = GL_SETUP_Y + GL_SETUP_ROW0 + row * GL_SETUP_STEP + GL_SETUP_INK_MID;
        CHECK_EQ_INT(gallery_setup_row_at(mid), row);
        CHECK_EQ_INT(gallery_setup_row_at(mid - GL_SETUP_STEP / 2), row);                  /* the band's top row */
        CHECK_EQ_INT(gallery_setup_row_at(mid + GL_SETUP_STEP / 2 - 1), row);              /* and its last */
        CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + GL_SETUP_LABEL_X + 10, mid, &step), row);
        CHECK_EQ_INT(step, row >= 2 ? 1 : 0);   /* the label's side of Color and Skin: the row alone */
    }
    CHECK_EQ_INT(gallery_setup_row_at(GL_SETUP_Y + 5), -1);
}

static void geometry(void)
{
    SetupChoice c;
    gallery_setup_choice(&c);
    CHECK_EQ_INT(c.left, GL_SETUP_X + GL_SETUP_LABEL_X + GL_SETUP_LABEL_COL_W + GL_SETUP_COL_GAP);   /* the second column after the labels' and the air */
    CHECK_EQ_INT(c.preview, c.left + GL_ARROW_W + GL_SETUP_CHOICE_GAP);                              /* the box next to the left arrow */
    CHECK_EQ_INT(c.name_x0, c.preview + GL_SETUP_PREVIEW_W + GL_SETUP_CHOICE_GAP);                   /* the name starts past the box */
    CHECK_EQ_INT(c.name_x1 - c.name_x0, GL_SETUP_NAME_W);
    CHECK_EQ_INT(c.right, c.name_x1 + GL_SETUP_CHOICE_GAP);                                          /* and ends before the right arrow */
    CHECK_EQ_INT(c.right + GL_ARROW_W + GL_SETUP_LABEL_X, GL_SETUP_X + GL_SETUP_W);                  /* the right margin is the left one */
    CHECK(GL_SETUP_X >= 24 && 2 * GL_SETUP_X + GL_SETUP_W <= GL_W + 1 && 2 * GL_SETUP_X + GL_SETUP_W >= GL_W - 1);   /* centred, air at both sides */
    CHECK_EQ_INT(gallery_setup_choice_step(c.left - 17), 0);   /* the label's side: the row alone */
    CHECK_EQ_INT(gallery_setup_choice_step(c.left), -1);
    CHECK_EQ_INT(gallery_setup_choice_step(c.right + GL_ARROW_W - 1), 1);
    int hit_step;
    CHECK_EQ_INT(gallery_setup_hit(c.left + 2, GL_SETUP_Y + GL_SETUP_ROW0 + GL_SETUP_INK_MID, &hit_step), 0);    /* Color's row, left half */
    CHECK_EQ_INT(hit_step, -1);
    CHECK_EQ_INT(gallery_setup_hit(c.right + 2, GL_SETUP_Y + GL_SETUP_ROW0 + GL_SETUP_INK_MID, &hit_step), 0);
    CHECK_EQ_INT(hit_step, 1);
}

/* the widths are the texts' own, measured in the CD's font at the row's size (the constants are what these measure) */
static void measured(void)
{
    char cd[512], probe[600];
    snprintf(cd, sizeof cd, "%s/original", TNFS_SOURCE_DIR);
    snprintf(probe, sizeof probe, "%s/FRONTEND/ART/FONTS/GRAVER18.FFN", cd);
    if (access(probe, R_OK) != 0) {
        printf("skip the widths (no original/)\n");
        return;
    }
    resource_init(cd, 0);
    CHECK(text_init());
    int top, bottom;
    text_ink_scaled("H", GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN, &top, &bottom);
    CHECK_EQ_INT((top + bottom) / 2, GL_SETUP_INK_MID);   /* the constant is the font's */
    static const char *const LABEL[5] = { "Color", "Skin", "Transmission", "ABS", "Traction control" };
    int widest_label = 0;
    for (int i = 0; i < 5; i++) {
        const int w = text_width_scaled(LABEL[i], GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
        if (w > widest_label) widest_label = w;
    }
    CHECK_EQ_INT(widest_label, GL_SETUP_LABEL_COL_W);   /* the first column is exactly the widest label */
    int widest = text_width_scaled("Console version", GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
    for (int model = 0; model < CAR_PAINT_MODELS; model++)
        for (int k = 0; k < car_paint_count(model); k++) {
            const int w = text_width_scaled(car_paint(model, k)->name, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
            if (w > widest) widest = w;
            if (w > GL_SETUP_NAME_W) printf("too wide: %s %d > %d\n", car_paint(model, k)->name, w, GL_SETUP_NAME_W);
            CHECK(w <= GL_SETUP_NAME_W);   /* every name of the CD's cars fits, at the row's size, between the box and the arrow */
        }
    CHECK(GL_SETUP_NAME_W - widest >= 0 && GL_SETUP_NAME_W - widest <= 8);   /* the span is the widest name's with a little air, not more */
    /* General's and Mechanical's values (owner 2026-11-19): one that fits is whole, a long one is cut with "...", none is a dash */
    char fit[64];
    const int room = 80;
    added_gallery_figure_fit("Germany", room, fit, sizeof fit);
    CHECK(strcmp(fit, "Germany") == 0 || text_width_scaled(fit, 3, 4) <= room);
    added_gallery_figure_fit("Mercedes-Benz AMG Performance", room, fit, sizeof fit);
    CHECK(strlen(fit) > 3 && strcmp(fit + strlen(fit) - 3, "...") == 0 && strncmp(fit, "Mercedes", 3) == 0);
    CHECK(text_width_scaled(fit, 3, 4) <= room);
    added_gallery_figure_fit("", room, fit, sizeof fit);
    CHECK(strcmp(fit, "-") == 0);
    added_gallery_figure_fit("Mercedes-Benz AMG Performance", 6, fit, sizeof fit);   /* no room even for the dots */
    CHECK(fit[0] == 0);
    printf("widest colour name %d px of %d, label column %d px, panel %d px wide at x %d\n", widest, GL_SETUP_NAME_W, widest_label, GL_SETUP_W, GL_SETUP_X);
}

int main(void)
{
    geometry();
    rows();
    measured();
    return TEST_REPORT();
}

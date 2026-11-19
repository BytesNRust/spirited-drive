/* owner 2026-11-19: the SETUP panel's Color and Skin row: the box next to the left arrow, the name centred in the gap between the box and the right
 * arrow, and the longest colour name of the CD's cars (and "Console version") inside that gap in the panel's font. The geometry needs no data; the
 * widths need the CD's font and are skipped without original/. */
#include "game/frontend/gallery_layout.h"
#include "game/frontend/setup_name.h"
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
    CHECK_EQ_INT(gallery_setup_hit(GL_SETUP_X + 150, GL_SETUP_Y + GL_SETUP_ROW0 + GALLERY_SETUP_ROWS * GL_SETUP_STEP + GL_SETUP_INK_MID, &step), -1);
}

static void geometry(void)
{
    SetupChoice c;
    gallery_setup_choice(&c);
    CHECK_EQ_INT(c.preview, c.left + GL_ARROW_W + GL_SETUP_CHOICE_GAP);                       /* the box next to the left arrow */
    CHECK_EQ_INT(c.name_x0, c.preview + GL_SETUP_PREVIEW_W + GL_SETUP_CHOICE_GAP);            /* the name starts past the box */
    CHECK_EQ_INT(c.name_x1, c.right - GL_SETUP_CHOICE_GAP);                                   /* and ends before the right arrow */
    CHECK_EQ_INT(c.right + GL_ARROW_W, GL_SETUP_X + GL_SETUP_W - 16);                         /* the right arrow ends where a switch's value does */
    CHECK(c.left > GL_SETUP_X + GL_SETUP_LABEL_X + 60 && c.name_x1 > c.name_x0);
    CHECK_EQ_INT(gallery_setup_choice_step(c.left - 17), 0);   /* the label's side: the row alone */
    CHECK_EQ_INT(gallery_setup_choice_step(c.left), -1);
    CHECK_EQ_INT(gallery_setup_choice_step(c.right + GL_ARROW_W - 1), 1);
    int hit_step;
    CHECK_EQ_INT(gallery_setup_hit(c.left + 2, GL_SETUP_Y + GL_SETUP_ROW0 + GL_SETUP_INK_MID, &hit_step), 0);    /* Color's row, left half */
    CHECK_EQ_INT(hit_step, -1);
    CHECK_EQ_INT(gallery_setup_hit(c.right + 2, GL_SETUP_Y + GL_SETUP_ROW0 + GL_SETUP_INK_MID, &hit_step), 0);
    CHECK_EQ_INT(hit_step, 1);
}

static void names(void)
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
    SetupChoice c;
    gallery_setup_choice(&c);
    const int room = c.name_x1 - c.name_x0;
    char out[64];
    int num, den, smallest = GL_SETUP_FONT_NUM * 100 / GL_SETUP_FONT_DEN;
    CHECK(setup_name_fit("Console version", room, out, sizeof out, &num, &den) <= room && strcmp(out, "Console version") == 0);
    for (int model = 0; model < CAR_PAINT_MODELS; model++)
        for (int k = 0; k < car_paint_count(model); k++) {
            const char *name = car_paint(model, k)->name;
            const int w = setup_name_fit(name, room, out, sizeof out, &num, &den);
            CHECK(w <= room);
            CHECK(strcmp(out, name) == 0);   /* whole, not cut */
            if (num * 100 / den < smallest) smallest = num * 100 / den;
            if (num * 100 / den < 75) printf("%s: %d/%d\n", name, num, den);
        }
    /* an added car's 24-character name of wide letters: cut with a "." as a last resort, within the room */
    CHECK(setup_name_fit("WWWWWWWWWWWWWWWWWWWWWWWW", room, out, sizeof out, &num, &den) <= room);
    CHECK(strlen(out) < 24 && out[strlen(out) - 1] == '.');
    CHECK_EQ_INT(num * 8, 5 * den);   /* at the smallest size */
    printf("smallest size used %d%% (room %d px)\n", smallest, room);
}

int main(void)
{
    geometry();
    rows();
    names();
    return TEST_REPORT();
}

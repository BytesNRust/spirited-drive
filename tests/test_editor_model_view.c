/* The Model window's View choice (editor/model_view.h): Model / Card / As the game draws it are the old Card view + As on the card boxes, one control; and the
 * caption under the picture names the colour in use. */
#include "model_view.h"
#include "test.h"

#include <string.h>

static void test_view_flags(void)
{
    int card = -1, as = -1;
    CHECK_EQ_INT(model_view_of(0, 0), MODEL_VIEW_MODEL);
    CHECK_EQ_INT(model_view_of(1, 0), MODEL_VIEW_CARD);
    CHECK_EQ_INT(model_view_of(1, 1), MODEL_VIEW_GAME);
    CHECK_EQ_INT(model_view_of(0, 1), MODEL_VIEW_MODEL);   /* As on the card without Card view shows nothing of it: the Model */

    model_view_flags(MODEL_VIEW_MODEL, &card, &as);
    CHECK(card == 0 && as == 0);
    model_view_flags(MODEL_VIEW_CARD, &card, &as);
    CHECK(card == 1 && as == 0);
    model_view_flags(MODEL_VIEW_GAME, &card, &as);
    CHECK(card == 1 && as == 1);

    for (int v = 0; v < MODEL_VIEWS; ++v) {   /* a round trip */
        model_view_flags((ModelView)v, &card, &as);
        CHECK_EQ_INT(model_view_of(card, as), v);
    }
    CHECK(strcmp(model_view_name(MODEL_VIEW_MODEL), "Model") == 0);
    CHECK(strcmp(model_view_name(MODEL_VIEW_CARD), "Card") == 0);
    CHECK(strcmp(model_view_name(MODEL_VIEW_GAME), "As the game draws it") == 0);
    CHECK(strcmp(model_view_name((ModelView)99), "") == 0);   /* a value outside the choice names nothing */
}

static void test_paint_caption(void)
{
    char text[64];
    CHECK(model_paint_caption("Formula Red", 1, text, sizeof text) && strcmp(text, "Paint: Formula Red") == 0);
    CHECK(model_paint_caption("", 1, text, sizeof text) && strcmp(text, "Paint: unnamed") == 0);   /* a colour with no name is still a colour */
    CHECK(model_paint_caption(NULL, 1, text, sizeof text) && strcmp(text, "Paint: none (base texture)") == 0);
    CHECK(model_paint_caption("Formula Red", 0, text, sizeof text) && strcmp(text, "Paint: none (base texture)") == 0);   /* a car the paint cannot be made for */
    char small[10];
    CHECK(model_paint_caption("Formula Red", 1, small, sizeof small) == 0 && small[sizeof small - 1] == '\0');   /* cut: said, never overflowed */
    CHECK(model_paint_caption("x", 1, text, 0) == 0);
}

static void test_render_size(void)
{
    int w = 0, h = 0;
    CHECK_EQ_INT(model_view_render_size(1400, 850, 1, &w, &h), 1);   /* the game's 480 lines at scale 1, in the area's shape */
    CHECK_EQ_INT(h, 480);
    CHECK_EQ_INT(w, 791);   /* 480 * 1400 / 850, rounded */
    CHECK_EQ_INT(model_view_render_size(1400, 850, 2, &w, &h), 2);   /* 960 lines would be finer than the area's 850: the area's own */
    CHECK(h == 850 && w == 1400);
    CHECK_EQ_INT(model_view_render_size(2000, 1200, 2, &w, &h), 2);
    CHECK(h == 960 && w == 1600);
    CHECK_EQ_INT(model_view_render_size(2000, 1200, 3, &w, &h), 3);
    CHECK(h == 1200 && w == 2000);   /* 1440 lines against the area's 1200 */
    CHECK_EQ_INT(model_view_render_size(180, 100, 1, &w, &h), 1);   /* a small panel is drawn at its own size */
    CHECK(h == 100 && w == 180);
    CHECK_EQ_INT(model_view_render_size(800, 600, 9, &w, &h), MODEL_RENDER_SCALE_MAX);   /* more than the most: the most */
    CHECK_EQ_INT(model_view_render_size(800, 600, 0, &w, &h), 1);
    CHECK_EQ_INT(model_view_render_size(800, 600, -3, &w, &h), 1);
    CHECK_EQ_INT(model_view_render_size(0, 0, 2, &w, &h), 2);
    CHECK(w == 0 && h == 0);
}

int main(void)
{
    test_view_flags();
    test_paint_caption();
    test_render_size();
    return TEST_REPORT();
}

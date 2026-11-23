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
    CHECK_EQ_INT(model_view_render_size(1, &w, &h), 1);   /* the game's 4:3 frame at the scale: 640 x 480 times it */
    CHECK(w == 640 && h == 480);
    CHECK_EQ_INT(model_view_render_size(2, &w, &h), 2);
    CHECK(w == 1280 && h == 960);
    CHECK_EQ_INT(model_view_render_size(3, &w, &h), 3);
    CHECK(w == 1920 && h == 1440);
    CHECK_EQ_INT(model_view_render_size(9, &w, &h), MODEL_RENDER_SCALE_MAX);   /* more than the most: the most */
    CHECK_EQ_INT(model_view_render_size(0, &w, &h), 1);
    CHECK_EQ_INT(model_view_render_size(-3, &w, &h), 1);
}

static void test_fit(void)
{
    int x, y, w, h;
    model_view_fit(1400, 850, 1280, 960, &x, &y, &w, &h);   /* a wide panel: as tall as it, in the middle */
    CHECK(h == 850 && w == 1133 && y == 0 && x == 133);
    model_view_fit(740, 240, 1280, 960, &x, &y, &w, &h);   /* the docked strip */
    CHECK(h == 240 && w == 320 && x == 210 && y == 0);
    model_view_fit(400, 600, 1280, 960, &x, &y, &w, &h);   /* a tall panel: as wide as it */
    CHECK(w == 400 && h == 300 && x == 0 && y == 150);
    model_view_fit(0, 100, 1280, 960, &x, &y, &w, &h);
    CHECK(w == 0 && h == 0 && x == 0 && y == 0);
}

/* a drag of the Color box: a change every 16 ms for a second, then it stops; each frame asks whether the scene is made again, as the Model does (and a pending change is taken as soon as it is due) */
static void test_colour_drag(void)
{
    uint32_t last = 0;
    int rebuilds = 0, low_frames = 0, frames = 0;
    for (uint32_t now = 1000; now < 2000; now += 16) {   /* the drag: a stale colour every frame */
        if (model_colour_rebuild_due(now, last)) last = (now != 0 ? now : 1u), ++rebuilds;
        low_frames += model_colour_low(now, last), ++frames;
    }
    CHECK(rebuilds >= 9 && rebuilds <= 10);   /* about 10 a second, not 62 */
    CHECK_EQ_INT(low_frames, frames);         /* every frame of the drag is the scale 1 one */
    const uint32_t stop = 2000 + 16;   /* the drag stops with one change not yet taken: the next due frame takes it */
    uint32_t now = stop;
    while (!model_colour_rebuild_due(now, last)) now += 16;
    CHECK(now - stop <= MODEL_COLOUR_REBUILD_MS);   /* taken within a tenth of a second of stopping */
    last = (now != 0 ? now : 1u);
    CHECK(model_colour_low(last + 10, last));
    CHECK(!model_colour_low(last + MODEL_COLOUR_LOW_MS, last));   /* then the full frame */
    CHECK(!model_colour_low(5000, 0));   /* no colour changed yet: never low */
    CHECK(model_colour_rebuild_due(5000, 0));
}

int main(void)
{
    test_colour_drag();
    test_view_flags();
    test_paint_caption();
    test_render_size();
    test_fit();
    return TEST_REPORT();
}

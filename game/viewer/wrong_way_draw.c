/* port (wrong_way): see wrong_way_draw.h. */
#include "game/viewer/wrong_way_draw.h"

#include "game/sys/text.h"
#include "game/sys/video.h"

#include <string.h>

enum {
    MAX_WIDTH = 3360, MAX_HEIGHT = 1440,   /* the widest frame drawn on (21:9 at 1440 rows), as race_stats_draw.c */
    BASE_ROWS = 480,                       /* the HUD's own frame: everything below is in its rows and grows with the frame's height */
    SCALE_NUM = 1, SCALE_DEN = BASE_ROWS,  /* graver18 at its own size on a 480-row frame (capitals 15 rows), not enlarged (the owner, i196) */
    TOP_ROWS = 32 + 4,                     /* the HUD's top bar (times, speed) ends at row 32 of 480: the words 4 rows under it */
    WIDEST_NUM = 4, WIDEST_DEN = 5         /* the words at most four fifths of the frame's width */
};

static uint8_t scratch[MAX_WIDTH * MAX_HEIGHT];

const uint8_t *wrong_way_compose(const WrongWay *w, const uint8_t *frame, int width, int height, const uint8_t palette[768], int german)
{
    if (!wrong_way_visible(w) || width <= 0 || width > MAX_WIDTH || height <= 0 || height > MAX_HEIGHT) return frame;
    if (!text_loaded() && !text_init()) return frame;
    text_build_styles_from(palette);   /* the palette on the display; each frame drawn, as the stats overlay may have built them from another */
    memcpy(scratch, frame, (size_t)width * (size_t)height);
    const char *const text = wrong_way_text(german);
    int num = SCALE_NUM * height, den = SCALE_DEN;   /* the frame's height over 480: 2 at render_scale 2, 3 at 3 */
    const int plain = text_width(text);
    if (plain > 0 && (long long)plain * num > (long long)width * WIDEST_NUM / WIDEST_DEN * den)   /* the German words at 4:3: smaller to fit */
        num = width * WIDEST_NUM, den = plain * WIDEST_DEN;
    video_select_target(scratch, width, height);
    text_select(TEXT_RED);
    text_draw_smooth(text, (width - text_width_scaled(text, num, den)) / 2, height * TOP_ROWS / BASE_ROWS, num, den);
    video_select_target(NULL, 0, 0);
    return scratch;
}

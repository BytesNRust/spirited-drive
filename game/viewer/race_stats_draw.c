/* port: the race's stats overlay drawn on a copy of the frame (race_stats.h). The figures and the words are race_stats.c's. */
#include "viewer/race_stats.h"

#include "game/sys/text.h"
#include "game/sys/video.h"

#include <string.h>

enum {
    MARGIN_X = 4, MARGIN_Y = 26,     /* the corner under the HUD's top row (checked in the captures) */
    PAD = 4,                         /* the backing's edge round the text */
    MAX_WIDTH = 1600,                /* the widest frame the overlay draws on (21:9 at 480 is about 1493) */
    MAX_HEIGHT = 480
};

/* the copy the overlay is drawn on: the module's own, so the race's buffers stay as they were */
static uint8_t scratch[MAX_WIDTH * MAX_HEIGHT];

/* the palette's black: the entry with the least light */
static uint8_t nearest_black(const uint8_t palette[768])
{
    int best = 0, best_sum = 1 << 30;
    for (int i = 0; i < 256; i++) {
        const int sum = palette[3 * i] + palette[3 * i + 1] + palette[3 * i + 2];
        if (sum < best_sum) best = i, best_sum = sum;
    }
    return (uint8_t)best;
}

/* the text styles are built for the palette on the display, once per palette */
static void match_palette(RaceStats *s, const uint8_t palette[768])
{
    if (s->palette_known && memcmp(s->palette, palette, 768) == 0) return;
    memcpy(s->palette, palette, 768);
    text_build_styles_from(palette);
    s->dark = nearest_black(palette);
    s->palette_known = 1;
}

static void backing(uint8_t *to, int width, int height, int x0, int y0, int w, int h, uint8_t dark)
{
    for (int y = y0 < 0 ? 0 : y0; y < y0 + h && y < height; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x0 + w && x < width; x++) to[y * width + x] = dark;
}

const uint8_t *race_stats_compose(RaceStats *s, const uint8_t *frame, int width, int height,
                                  const uint8_t palette[768], const RaceStatsView *v)
{
    if (!s->on || width <= 0 || width > MAX_WIDTH || height <= 0 || height > MAX_HEIGHT) return frame;
    if (!s->text_ready) s->text_ready = text_init();
    if (!s->text_ready) return frame;
    match_palette(s, palette);
    memcpy(scratch, frame, (size_t)width * (size_t)height);
    RaceStatsText t;
    race_stats_format(v, &t);
    const int line_h = text_line_height();
    int widest = 0;
    for (int i = 0; i < RACE_STATS_LINES; i++) {
        const int w = text_width(t.line[i]);
        if (w > widest) widest = w;
    }
    backing(scratch, width, height, MARGIN_X, MARGIN_Y, widest + 2 * PAD, RACE_STATS_LINES * line_h + 2 * PAD, s->dark);
    video_select_target(scratch, width, height);
    text_select(TEXT_CREAM);
    text_ramp(0);
    for (int i = 0; i < RACE_STATS_LINES; i++)
        text_draw(t.line[i], MARGIN_X + PAD, MARGIN_Y + PAD + i * line_h);
    video_select_target(NULL, 0, 0);
    return scratch;
}

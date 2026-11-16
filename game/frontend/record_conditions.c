/* port: see record_conditions.h. */
#include "game/frontend/record_conditions.h"

#include "game/sys/extras_marks.h"

#include <stdio.h>

void record_conditions_lines(RecordMarks m, char *first, char *second, int size)
{
    static const char *const WEATHER[4] = { "CLR", "FOG", "RAIN", "STRM" };   /* four letters at most */
    static const char *const TIME[4] = { "DAY", "DAWN", "DUSK", "NGT" };
    static const char *const LEVEL[3] = { "ORIGINAL", "EASY", "HARD" };
    if (!(m.weather & RECORD_MARK_KNOWN)) {
        snprintf(first, (size_t)size, "-");
        snprintf(second, (size_t)size, "%s", "");
        return;
    }
    const int kind = m.weather & RECORD_WEATHER_KIND;
    const char *weather = m.weather & RECORD_WEATHER_SNOW ? "SNOW" : m.weather & RECORD_WEATHER_WET ? "WET" : kind < 4 ? WEATHER[kind] : "?";
    const int t = m.time & 0x0f;
    const int rivals = m.difficulty & 3, police = m.difficulty >> 2 & 3, traffic = m.difficulty >> 4 & 3;
    snprintf(first, (size_t)size, "%s %s", weather, t < 4 ? TIME[t] : "?");
    snprintf(second, (size_t)size, "%s", rivals == police && police == traffic && rivals < 3 ? LEVEL[rivals] : "MIXED");
}

/* the two lines (the second "" for one) at num/den: their block `wide`, the lines centred on each other from `left`, the block's letters
 * centred on those of the row of full-size text drawn at `row_y` */
static int32_t block_width(const char *first, const char *second, int32_t num, int32_t den, const ConditionsPen *pen)
{
    const int32_t w1 = pen->width(pen->ctx, first, num, den), w2 = second[0] != '\0' ? pen->width(pen->ctx, second, num, den) : 0;
    return w1 > w2 ? w1 : w2;
}

static void block_draw(const char *first, const char *second, int32_t left, int32_t row_y, int32_t num, int32_t den, const ConditionsPen *pen)
{
    const int two = second[0] != '\0';
    const int32_t w1 = pen->width(pen->ctx, first, num, den), w2 = two ? pen->width(pen->ctx, second, num, den) : 0;
    const int32_t wide = w1 > w2 ? w1 : w2;
    int32_t row_top, row_bottom, t1, b1, t2 = 0, b2 = 0;
    pen->ink(pen->ctx, "X", 1, 1, &row_top, &row_bottom);   /* the row's capitals */
    pen->ink(pen->ctx, first, num, den, &t1, &b1);
    if (two) pen->ink(pen->ctx, second, num, den, &t2, &b2);
    const int32_t height = (b1 - t1) + (two ? RECORD_CONDITIONS_GAP + (b2 - t2) : 0);
    const int32_t y1 = row_y + (row_top + row_bottom - height) / 2 - t1;   /* the block's letters on the middle of the row's */
    pen->text(pen->ctx, first, left + (wide - w1) / 2, y1, num, den);
    if (two) pen->text(pen->ctx, second, left + (wide - w2) / 2, y1 + b1 + RECORD_CONDITIONS_GAP - t2, num, den);
}

void record_conditions_draw(RecordMarks m, int32_t right, int32_t row_y, int32_t num, int32_t den, const ConditionsPen *pen)
{
    char first[16], second[16];
    record_conditions_lines(m, first, second, sizeof first);
    block_draw(first, second, right - block_width(first, second, num, den, pen), row_y, num, den, pen);
}

int record_cheat_lines(int cheats, char *first, char *second, int size)
{
    static const struct {
        int mark;
        const char *word;
    } WORDS[] = { { EXTRAS_MARK_NITRO, "NITRO" }, { EXTRAS_MARK_EXPLODE, "EXPLODE" }, { EXTRAS_MARK_TURBO, "TURBO" }, { EXTRAS_MARK_LUNAR, "LUNAR" } };
    const char *on[4];
    int n = 0;
    for (size_t i = 0; i < sizeof WORDS / sizeof WORDS[0]; i++)
        if (cheats & WORDS[i].mark) on[n++] = WORDS[i].word;
    snprintf(first, (size_t)size, "%s%s%s", n > 0 ? on[0] : "", n > 1 ? " " : "", n > 1 ? on[1] : "");
    snprintf(second, (size_t)size, "%s%s%s", n > 2 ? on[2] : "", n > 3 ? " " : "", n > 3 ? on[3] : "");
    return n;
}

void record_cheat_draw(int cheats, int32_t centre, int32_t room, int32_t row_y, const ConditionsPen *pen)
{
    static const int32_t SIZES[][2] = { { 1, 2 }, { 2, 5 }, { 1, 3 } };
    enum { SIZE_COUNT = sizeof SIZES / sizeof SIZES[0] };
    char first[16], second[16];
    if (record_cheat_lines(cheats, first, second, sizeof first) == 0) return;
    int s = 0;
    while (s + 1 < SIZE_COUNT && block_width(first, second, SIZES[s][0], SIZES[s][1], pen) > room) s++;
    const int32_t wide = block_width(first, second, SIZES[s][0], SIZES[s][1], pen);
    block_draw(first, second, centre - wide / 2, row_y, SIZES[s][0], SIZES[s][1], pen);
}

int32_t record_cheat_draw_before(int cheats, int32_t right, int32_t row_y, int32_t num, int32_t den, const ConditionsPen *pen)
{
    char first[16], second[16];
    if (record_cheat_lines(cheats, first, second, sizeof first) == 0) return right;
    const int32_t left = right - block_width(first, second, num, den, pen);
    block_draw(first, second, left, row_y, num, den, pen);
    return left;
}

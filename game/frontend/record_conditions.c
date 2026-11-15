/* port: see record_conditions.h. */
#include "game/frontend/record_conditions.h"

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

void record_conditions_draw(RecordMarks m, int32_t right, int32_t row_y, int32_t num, int32_t den, const ConditionsPen *pen)
{
    char first[16], second[16];
    record_conditions_lines(m, first, second, sizeof first);
    const int32_t w1 = pen->width(pen->ctx, first, num, den), w2 = second[0] != '\0' ? pen->width(pen->ctx, second, num, den) : 0;
    const int32_t wide = w1 > w2 ? w1 : w2, left = right - wide;
    const int32_t full = pen->line_height(pen->ctx), line = full * num / den;
    const int32_t top = row_y + (full - (second[0] != '\0' ? 2 : 1) * line) / 2;
    pen->text(pen->ctx, first, left + (wide - w1) / 2, top, num, den);
    if (second[0] != '\0') pen->text(pen->ctx, second, left + (wide - w2) / 2, top + line, num, den);
}

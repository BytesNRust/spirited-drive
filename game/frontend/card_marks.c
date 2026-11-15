/* port (dep56): see card_marks.h. */
#include "game/frontend/card_marks.h"

#include <stdio.h>
#include <string.h>

void card_marks_text(int mirror, int inverse, int rally, int weather, int light_time, int laps, char *out, size_t size)
{
    static const char *const WEATHER[4] = { "CLR", "FOG", "RAIN", "STRM" };   /* WEATHER_* (weather.h), Best Times' words */
    static const char *const TIME[4] = { "DAY", "DAWN", "DUSK", "NGT" };      /* LIGHT_TIME_* (port_settings.h) */
    if (size == 0) return;
    char layout[4] = "";
    size_t n = 0;
    if (mirror) layout[n++] = 'M';
    if (inverse) layout[n++] = 'I';
    if (rally) layout[n++] = 'R';
    layout[n] = '\0';
    char time[8] = "", lap_words[16] = "";
    if (light_time >= 0 && light_time < 4) snprintf(time, sizeof time, " %s", TIME[light_time]);
    if (laps > 0) snprintf(lap_words, sizeof lap_words, " %d LAP%s", laps, laps == 1 ? "" : "S");
    snprintf(out, size, "%s%s%s%s%s", layout, n > 0 ? "  " : "", weather >= 0 && weather < 4 ? WEATHER[weather] : "CLR", time, lap_words);
}

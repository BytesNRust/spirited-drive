#include "game/race/session_rules.h"

#include "game/race/weather.h"

#include <string.h>

static const uint8_t MAGIC[8] = { 'T', 'N', 'F', 'S', 'S', 'Y', 'N', '1' };

int session_rules_put(const SessionRules *r, uint8_t out[SESSION_RULES_SIZE])
{
    memcpy(out, MAGIC, sizeof MAGIC);
    out[SESSION_VERSION_AT] = SESSION_RULES_VERSION;
    out[SESSION_MODE_AT] = r->mode, out[SESSION_DIFFICULTY_AT] = r->difficulty, out[SESSION_WEATHER_AT] = r->weather;
    out[12] = r->reverse, out[13] = r->lunar_springs, out[14] = r->lunar_gravity, out[15] = r->rally_gravel, out[16] = r->cheats;
    out[17] = r->rain_grip, out[18] = r->liveries, out[19] = r->time_of_day, out[20] = r->psx_time;
    memcpy(out + SESSION_PAINT_AT, r->paint, SESSION_SLOTS);
    out[SESSION_SNOW_AT] = r->snow_weather;
    return SESSION_RULES_SIZE;
}

int session_rules_get(const uint8_t *d, size_t size, SessionRules *out)
{
    if (d == NULL || size < SESSION_RULES_SIZE_V1 || memcmp(d, MAGIC, sizeof MAGIC) != 0 || d[SESSION_VERSION_AT] < 1) return 0;
    const int has_snow = d[SESSION_VERSION_AT] >= 2;   /* a version 1 record is 29 bytes: no snow */
    if (has_snow && (size < SESSION_RULES_SIZE || d[SESSION_SNOW_AT] > 1)) return 0;
    if (d[SESSION_MODE_AT] > SESSION_RESTOMOD || d[SESSION_DIFFICULTY_AT] > 2 || d[SESSION_WEATHER_AT] >= WEATHER_COUNT) return 0;
    if (d[12] > 1 || d[13] > 1 || d[14] > 1 || d[15] > 1 || d[16] > 7 || d[17] > 1 || d[18] > 1 || d[19] > 2 || d[20] > 2) return 0;
    SessionRules r;
    r.mode = d[SESSION_MODE_AT], r.difficulty = d[SESSION_DIFFICULTY_AT], r.weather = d[SESSION_WEATHER_AT];
    r.reverse = d[12], r.lunar_springs = d[13], r.lunar_gravity = d[14], r.rally_gravel = d[15], r.cheats = d[16];
    r.rain_grip = d[17], r.liveries = d[18], r.time_of_day = d[19], r.psx_time = d[20];
    memcpy(r.paint, d + SESSION_PAINT_AT, SESSION_SLOTS);
    r.snow_weather = has_snow ? d[SESSION_SNOW_AT] : 0;
    *out = r;
    return 1;
}

static SessionRules active;
static int has_active;

void session_rules_set(const SessionRules *rules)
{
    if (rules != NULL) active = *rules;
    has_active = rules != NULL;
}

const SessionRules *session_rules_active(void) { return has_active ? &active : NULL; }

/* port (online): the rules of a link session that only the host chooses, sent to every guest after the setup. The setup (004fb7bc) already holds the track, race type, laps,
 * cars, seed and track flags; these are the port's own settings that change the race or the cars' look and that each machine used to read from its own files. A guest
 * that gets none (an older host) keeps what it did before: Original difficulty, no reverse, its own settings. */
#ifndef TNFS_SESSION_RULES_H
#define TNFS_SESSION_RULES_H

#include <stddef.h>
#include <stdint.h>

enum { SESSION_STOCK = 0, SESSION_RESTORED = 1, SESSION_RESTOMOD = 2, SESSION_SLOTS = 8, SESSION_RULES_VERSION = 2 };
enum { SESSION_VERSION_AT = 8, SESSION_MODE_AT = 9, SESSION_DIFFICULTY_AT = 10, SESSION_WEATHER_AT = 11, SESSION_PAINT_AT = 21, SESSION_SNOW_AT = 29, SESSION_RULES_SIZE_V1 = 29, SESSION_RULES_SIZE = 30 };   /* version 2 added the snow at the end */

typedef struct {
    uint8_t mode;            /* SESSION_*: the host's mode, which governs the session's shared rules */
    uint8_t difficulty;      /* 0 Easy, 1 Original, 2 Hard (difficulty.h) */
    uint8_t weather;         /* WEATHER_* (weather.h) */
    uint8_t reverse;         /* the circuit driven the other way round */
    uint8_t lunar_springs;   /* the moon's textures on Rusty Springs' rally */
    uint8_t lunar_gravity;   /* the moon's pull and grip */
    uint8_t rally_gravel;    /* gravel noise and dust */
    uint8_t cheats;          /* EXTRAS_MARK_* (extras_marks.h): NITRO 1, EXPLODE 2, TURBO 4 */
    uint8_t rain_grip;       /* with wet weather the road's grip is reduced */
    uint8_t liveries;        /* the car colours (CUSTOMIZE) are on: every machine draws the paints below; off, the exe's colours */
    uint8_t time_of_day;     /* the exe's: 0 morning, 1 midday, 2 evening */
    uint8_t psx_time;        /* the PS1 light's: PSX_TIME_* (port_settings.h) */
    uint8_t snow_weather;    /* (version 2) the snow of a snowy track (weather_snow.h) and its rally on snow (rally_gravel.h): the host's, since it changes the grip; 0 from a version 1 host */
    uint8_t paint[SESSION_SLOTS];   /* each slot's paint (PAINT_OWN 0, PAINT_PS1 1, PAINT_CATALOGUE + k; 0xff: the race's own draw) */
} SessionRules;

/* The record as it goes on the wire: "TNFSSYN1", the version, then the fields in the order above; SESSION_RULES_SIZE */
int session_rules_put(const SessionRules *rules, uint8_t out[SESSION_RULES_SIZE]);
/* 1 and `*out` when `data` (size bytes) is a record of this version or a later one (its extra bytes are skipped) with every field in range; else 0 and `*out` untouched */
int session_rules_get(const uint8_t *data, size_t size, SessionRules *out);

/* The session the race in progress runs under: set from a guest's trailer or the host's own rules, NULL when the session has none (a solo race, an older host) */
void session_rules_set(const SessionRules *rules);
const SessionRules *session_rules_active(void);

#endif

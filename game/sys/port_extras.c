/* port: the port's own extras file (extras.cfg). */
#include "game/sys/port_extras.h"
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_EXTRAS_NAME "extras.cfg"

static const char *const START_NAMES[] = { "game_mode", "network", "central" };
static const char *const UNIT_NAMES[] = { "language", "mph", "kmh" };

/* The switches, in the order they are written. */
static const char *const SWITCH_KEYS[] = { "mirror", "rally", "warrior", "vegas", "explode", "nitro",
                                            "turbo", "replay_cameras", "warrior_rival", "no_movies", "no_assists",
                                            "modern_3d", "texture_filter", "fog", "bumper_mirror",
                                            "bumper_radar", "variant_time", "stats_overlay", "car_markers",
                                            "lunar_springs" };
enum { SWITCH_COUNT = (int)(sizeof SWITCH_KEYS / sizeof SWITCH_KEYS[0]) };

/* Each switch as one row, so the parse, the format and the checks agree on the order. */
static void switch_read(const PortExtras *e, int rows[SWITCH_COUNT])
{
    const int all[] = { e->mirror, e->rally,  e->warrior, e->vegas,  e->explode, e->nitro,
                        e->turbo, e->replay_cameras, e->warrior_rival, e->no_movies, e->no_assists, e->modern_3d,
                        e->texture_filter, e->fog, e->bumper_mirror, e->bumper_radar, e->variant_time,
                        e->stats_overlay, e->car_markers, e->lunar_springs };
    memcpy(rows, all, sizeof all);
}

static void switch_write(PortExtras *e, const int rows[SWITCH_COUNT])
{
    int *const all[] = { &e->mirror, &e->rally,  &e->warrior, &e->vegas,  &e->explode, &e->nitro,
                         &e->turbo, &e->replay_cameras, &e->warrior_rival, &e->no_movies, &e->no_assists,
                         &e->modern_3d, &e->texture_filter, &e->fog, &e->bumper_mirror, &e->bumper_radar,
                         &e->variant_time, &e->stats_overlay, &e->car_markers, &e->lunar_springs };
    for (int i = 0; i < SWITCH_COUNT; i++)
        *all[i] = rows[i];
}

PortExtras port_extras_defaults(void)
{
    const PortExtras d = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, START_GAME_MODE, UNITS_LANGUAGE, 0 };
    return d;
}

static bool parse_int(const char *value, long *out)
{
    char *end;
    *out = strtol(value, &end, 10);
    return end != value && *end == '\0';
}

static void apply(PortExtras *e, const char *key, const char *value)
{
    long n;
    if (strcmp(key, "start") == 0) {
        for (int i = 0; i < 3; i++)
            if (strcmp(value, START_NAMES[i]) == 0)
                e->start = (StartScreen)i;
        return;
    }
    if (strcmp(key, "units") == 0) {
        for (int i = 0; i < 3; i++)
            if (strcmp(value, UNIT_NAMES[i]) == 0)
                e->units = (SpeedUnits)i;
        return;
    }
    for (int i = 0; i < SWITCH_COUNT; i++)
        if (strcmp(key, SWITCH_KEYS[i]) == 0) {
            if (parse_int(value, &n) && (n == 0 || n == 1)) {
                int rows[SWITCH_COUNT];
                switch_read(e, rows);
                rows[i] = (int)n;
                switch_write(e, rows);
            }
            return;
        }
}

PortExtras port_extras_parse(const char *text, size_t size)
{
    PortExtras e = port_extras_defaults();
    size_t pos = 0;
    while (pos < size) {
        char line[64];
        size_t len = 0;
        while (pos < size && text[pos] != '\n') {
            if (len < sizeof line - 1)
                line[len++] = text[pos];
            pos++;
        }
        pos++;
        if (len > 0 && line[len - 1] == '\r')
            len--;
        line[len] = '\0';
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            apply(&e, line, eq + 1);
        }
    }
    return e;
}

size_t port_extras_format(const PortExtras *e, char *out, size_t size)
{
    int rows[SWITCH_COUNT];
    switch_read(e, rows);
    int n = snprintf(out, size, "start=%s\nunits=%s\n", START_NAMES[e->start], UNIT_NAMES[e->units]);
    if (n < 0 || (size_t)n >= size)
        return 0;
    for (int i = 0; i < SWITCH_COUNT; i++) {
        const int w = snprintf(out + n, size - (size_t)n, "%s=%d\n", SWITCH_KEYS[i], rows[i]);
        if (w < 0 || (size_t)w >= size - (size_t)n)
            return 0;
        n += w;
    }
    return (size_t)n;
}

PortExtras port_extras_load(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_EXTRAS_NAME, &size);
    if (!data)
        return port_extras_defaults();
    const PortExtras e = port_extras_parse((const char *)data, size);
    platform_free_file(data);
    return e;
}

bool port_extras_save(const PortExtras *e)
{
    char text[512];
    const size_t n = port_extras_format(e, text, sizeof text);
    return n > 0 && platform_write_user_file(PORT_EXTRAS_NAME, (const uint8_t *)text, n);
}

int port_extras_race_on(RaceSwitch which)
{
    const PortExtras e = port_extras_load();
    return which == RACE_SWITCH_STATS ? e.stats_overlay : e.car_markers;
}

bool port_extras_set_race_switch(RaceSwitch which, int on)
{
    PortExtras e = port_extras_load();
    if (which == RACE_SWITCH_STATS)
        e.stats_overlay = on != 0;
    else
        e.car_markers = on != 0;
    return port_extras_save(&e);
}

int speed_units_kmh(SpeedUnits units, int german)
{
    if (units == UNITS_LANGUAGE) return german != 0;
    return units == UNITS_KMH;
}

uint32_t port_extras_cheat_marks(const PortExtras *e)
{
    return (e->nitro ? EXTRAS_MARK_NITRO : 0) | (e->explode ? EXTRAS_MARK_EXPLODE : 0) | (e->turbo ? EXTRAS_MARK_TURBO : 0);
}

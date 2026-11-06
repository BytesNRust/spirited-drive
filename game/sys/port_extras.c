/* port: the port's own extras file (the mode's file). */
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char *const START_NAMES[] = { "game_mode", "network", "central" };
static const char *const UNIT_NAMES[] = { "mph", "kmh" };
static const char *const DASH_BOX_NAMES[] = { "map", "lever", "hidden" };

/* The switches, in the order they are written. */
static const char *const SWITCH_KEYS[] = { "mirror", "rally", "warrior", "vegas", "explode", "nitro",
                                            "turbo", "replay_cameras", "warrior_rival", "no_movies", "no_assists",
                                            "bumper_mirror",
                                            "bumper_radar", "variant_time", "stats_overlay", "car_markers",
                                            "lunar_springs", "movies_wide", "follow_camera", "psx_liveries", "reverse", "lunar_gravity", "rally_gravel", "update_check" };
enum { SWITCH_COUNT = (int)(sizeof SWITCH_KEYS / sizeof SWITCH_KEYS[0]) };

/* Each switch as one row, so the parse, the format and the checks agree on the order. */
static void switch_read(const PortExtras *e, int rows[SWITCH_COUNT])
{
    const int all[] = { e->mirror, e->rally,  e->warrior, e->vegas,  e->explode, e->nitro,
                        e->turbo, e->replay_cameras, e->warrior_rival, e->no_movies, e->no_assists,
                        e->bumper_mirror, e->bumper_radar, e->variant_time,
                        e->stats_overlay, e->car_markers, e->lunar_springs, e->movies_wide, e->follow_camera, e->psx_liveries, e->reverse, e->lunar_gravity, e->rally_gravel, e->update_check };
    memcpy(rows, all, sizeof all);
}

static void switch_write(PortExtras *e, const int rows[SWITCH_COUNT])
{
    int *const all[] = { &e->mirror, &e->rally,  &e->warrior, &e->vegas,  &e->explode, &e->nitro,
                         &e->turbo, &e->replay_cameras, &e->warrior_rival, &e->no_movies, &e->no_assists,
                         &e->bumper_mirror, &e->bumper_radar,
                         &e->variant_time, &e->stats_overlay, &e->car_markers, &e->lunar_springs, &e->movies_wide,
                         &e->follow_camera, &e->psx_liveries, &e->reverse, &e->lunar_gravity, &e->rally_gravel, &e->update_check };
    for (int i = 0; i < SWITCH_COUNT; i++)
        *all[i] = rows[i];
}

PortExtras port_extras_defaults(void)
{
    const PortExtras d = { .car_markers = 1, .start = START_GAME_MODE, .units = UNITS_MPH, .dash_box = DASH_BOX_MAP, .movies_wide = 1, .difficulty = 1, .update_check = 1 };
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
    if (strcmp(key, "dash_box") == 0) {
        for (int i = 0; i < 3; i++)
            if (strcmp(value, DASH_BOX_NAMES[i]) == 0)
                e->dash_box = (DashBox)i;
        return;
    }
    if (strcmp(key, "car_paint") == 0) {
        for (int c = 0; c < CAR_PAINT_SLOTS && value[c] != '\0'; c++) {
            const char d = value[c];
            const int v = d >= '0' && d <= '9' ? d - '0' : d >= 'a' && d <= 'f' ? d - 'a' + 10 : -1;
            if (v < 0) break;
            e->car_paint[c] = (uint8_t)v;
        }
        return;
    }
    if (strcmp(key, "units") == 0) {
        for (int i = 0; i < 2; i++)
            if (strcmp(value, UNIT_NAMES[i]) == 0)
                e->units = (SpeedUnits)i;
        if (strcmp(value, "language") == 0) e->units = UNITS_MPH;   /* an older file: the port has no languages */
        return;
    }
    if (strcmp(key, "difficulty") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 2)
            e->difficulty = (int)n;
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
    int n = snprintf(out, size, "start=%s\nunits=%s\ndash_box=%s\n", START_NAMES[e->start],
                       UNIT_NAMES[e->units], DASH_BOX_NAMES[e->dash_box]);
    if (n < 0 || (size_t)n >= size)
        return 0;
    for (int i = 0; i < SWITCH_COUNT; i++) {
        const int w = snprintf(out + n, size - (size_t)n, "%s=%d\n", SWITCH_KEYS[i], rows[i]);
        if (w < 0 || (size_t)w >= size - (size_t)n)
            return 0;
        n += w;
    }
    char paint[CAR_PAINT_SLOTS + 1];
    for (int c = 0; c < CAR_PAINT_SLOTS; c++) paint[c] = "0123456789abcdef"[e->car_paint[c] & 15];
    paint[CAR_PAINT_SLOTS] = '\0';
    const int w = snprintf(out + n, size - (size_t)n, "car_paint=%s\ndifficulty=%d\n", paint, e->difficulty);
    if (w < 0 || (size_t)w >= size - (size_t)n)
        return 0;
    return (size_t)(n + w);
}

void port_extras_take(void *to, const char *key, const char *value) { apply(to, key, value); }

PortExtras port_extras_load(void)
{
    const Profile p = (Profile)port_settings_profile();
    PortExtras e = port_profile_extras(p, port_extras_defaults());
    size_t size = 0;
    uint8_t *data = platform_read_user_file(port_profile_file(p), &size);
    if (data == NULL) return e;
    port_profile_read_free(p, (const char *)data, size, port_extras_take, &e);
    platform_free_file(data);
    return e;
}

bool port_extras_save(const PortExtras *e)
{
    const PortSettings s = port_settings_load();
    if (port_mode_save(&s, e)) return true;
    port_log("extras: %s was not saved\n", port_profile_file((Profile)s.profile));
    return false;
}

DashBox port_extras_dash_box(void)
{
    return port_extras_load().dash_box;
}

int port_extras_race_on(RaceSwitch which)
{
    const PortExtras e = port_extras_load();
    return which == RACE_SWITCH_STATS ? e.stats_overlay : e.car_markers;
}

bool port_extras_set_race_switch(RaceSwitch which, int on)
{
    /* where the mode holds it the key lasts the race (F6), or has no effect (F12): nothing to save */
    if (!port_profile_key_free((Profile)port_settings_profile(), which == RACE_SWITCH_STATS ? "stats_overlay" : "car_markers")) return false;
    PortExtras e = port_extras_load();
    if (which == RACE_SWITCH_STATS)
        e.stats_overlay = on != 0;
    else
        e.car_markers = on != 0;
    return port_extras_save(&e);
}

int speed_units_kmh(SpeedUnits units)
{
    return units == UNITS_KMH;
}

uint32_t port_extras_cheat_marks(const PortExtras *e)
{
    return (e->nitro ? EXTRAS_MARK_NITRO : 0) | (e->explode ? EXTRAS_MARK_EXPLODE : 0) | (e->turbo ? EXTRAS_MARK_TURBO : 0);
}

int port_extras_car_paint(int car)
{
    return car >= 0 && car < CAR_PAINT_SLOTS ? port_extras_load().car_paint[car] : PAINT_OWN;
}

bool port_extras_set_car_paint(int car, int paint)
{
    if (car < 0 || car >= CAR_PAINT_SLOTS || paint < 0 || paint > 15) return false;
    PortExtras e = port_extras_load();
    e.car_paint[car] = (uint8_t)paint;
    return port_extras_save(&e);
}

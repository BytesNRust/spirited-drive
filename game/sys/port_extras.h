/* port: the port's own extras (extras.cfg): the cheat codes as switches that last, and the handful of
 * things the original never offered as options. Kept apart from config.dat, which the game owns. */
#ifndef TNFS_PORT_EXTRAS_H
#define TNFS_PORT_EXTRAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { START_GAME_MODE = 0, START_NETWORK = 1, START_CENTRAL = 2 } StartScreen;

/* The speed's units, and the distances (miles or km) and the cockpit's dial with them. UNITS_LANGUAGE is the exe's:
 * km/h under German, mph otherwise; the other two are the player's choice, whatever the language. */
typedef enum { UNITS_LANGUAGE = 0, UNITS_MPH = 1, UNITS_KMH = 2 } SpeedUnits;

typedef struct {
    int mirror, rally, warrior, vegas, explode, nitro, turbo;   /* 0/1: SKCART, EARLYE, CONCEPTCAR, CASINOVILLE, ... */
    int replay_cameras, warrior_rival, no_movies, no_assists;   /* 0/1 */
    int modern_3d;    /* 0/1: the 3D clipped, unsnapped and perspective-correct, not the PS1-era mapper's warp */
    int texture_filter, fog;    /* 0/1, over Modern 3D (modern_road.h's ModernStyle); nothing without it */
    int bumper_mirror, bumper_radar;   /* 0/1: the rear-view mirror and the radar detector in the bumper view, under
                                        * the scoreboard (F1 / F2 turn them over in a race) */
    int variant_time; /* 0/1: the time of day offered with Mirror or Rally (fam_mirror.h), which the original hides */
    int stats_overlay;  /* 0/1: the race's stats overlay (F12 turns it in a race); off */
    int car_markers;    /* 0/1: the coloured triangles over the rival cars (F6 turns them in a race); On, the exe's */
    StartScreen start;                                          /* START_GAME_MODE */
    SpeedUnits units;                                           /* UNITS_LANGUAGE */
} PortExtras;

/* A record filed under these marks is not the normal one; the marks are what a race is tagged with. */
#include "game/sys/extras_marks.h"  /* EXTRAS_MARK_NITRO 1, _EXPLODE 2, _TURBO 4 */

PortExtras port_extras_defaults(void);
/* "key=value" per line; unknown keys and out-of-range values are ignored (the default stays). */
PortExtras port_extras_parse(const char *text, size_t size);
/* Writes the text form, NUL-terminated; returns its length, or 0 when it does not fit. */
size_t port_extras_format(const PortExtras *e, char *out, size_t size);
PortExtras port_extras_load(void);   /* user file "extras.cfg"; the defaults when it does not exist */
bool port_extras_save(const PortExtras *e);
/* The cheats that make a race's time an advantage one: NITRO 1, EXPLODE 2, TURBO 4. */
uint32_t port_extras_cheat_marks(const PortExtras *e);
/* port: the two switches a race turns with a key (F12 the stats overlay, F6 the car markers). A race starts with the
 * saved value, and a key in the race writes the new one back to extras.cfg. */
typedef enum { RACE_SWITCH_STATS = 0, RACE_SWITCH_MARKERS = 1 } RaceSwitch;
int port_extras_race_on(RaceSwitch which);                      /* 0/1 as extras.cfg has it */
bool port_extras_set_race_switch(RaceSwitch which, int on);     /* the file as it is, that switch set */
/* 1 when the speeds, distances and the cockpit's dial are in km/h: the player's choice, or with UNITS_LANGUAGE the
 * language's (german 1 is km/h, as 0052b31c gives it). */
int speed_units_kmh(SpeedUnits units, int german);

#endif
/* port: the port's own extras (the mode's file): the cheat codes as switches that last, and the handful of
 * things the original never offered as options. Kept apart from config.dat, which the game owns. */
#ifndef TNFS_PORT_EXTRAS_H
#define TNFS_PORT_EXTRAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { START_GAME_MODE = 0, START_NETWORK = 1, START_CENTRAL = 2 } StartScreen;

/* The speed's units, and the distances (miles or km) and the cockpit's dial with them: the player's choice, mph until it is made (the port has no
 * languages, so the exe's German km/h is not a default; an older file's `units=language` reads as mph). */
typedef enum { UNITS_MPH = 0, UNITS_KMH = 1 } SpeedUnits;

/* The dash's box under the right vent, where the exe puts the map (M hides it) and, with the map hidden, the gear
 * lever for a gear change. DASH_BOX_MAP is the exe's; DASH_BOX_LEVER keeps the lever in the box all the time and shows
 * the gear (no map); DASH_BOX_HIDDEN is the dash unbroken, the box only while changing gear. M shows the map from
 * either and goes back to the chosen mode. */
typedef enum { DASH_BOX_MAP = 0, DASH_BOX_LEVER = 1, DASH_BOX_HIDDEN = 2 } DashBox;

enum { CAR_PAINT_SLOTS = 12, PAINT_OWN = 0, PAINT_SECOND = 1, PAINT_CATALOGUE = 2 };   /* cars 0..11 (11 the Warrior) */

typedef struct {
    int mirror, rally, warrior, vegas, explode, nitro, turbo;   /* 0/1: SKCART, EARLYE, CONCEPTCAR, CASINOVILLE, ... */
    int replay_cameras, warrior_rival, no_movies, no_assists;   /* 0/1 */
    int bumper_mirror, bumper_radar;   /* 0/1: the rear-view mirror and the radar detector in the bumper view, under
                                        * the scoreboard (F1 / F2 turn them over in a race) */
    int variant_time; /* 0/1: the time of day offered with Mirror or Rally (fam_mirror.h), which the original hides */
    int stats_overlay;  /* 0/1: the race's stats overlay (F12 turns it in a race); off */
    int car_markers;    /* 0/1: the coloured triangles over the rival cars (F6 turns them in a race); On, the exe's */
    StartScreen start;                                          /* START_GAME_MODE */
    SpeedUnits units;                                           /* UNITS_MPH */
    int lunar_springs;  /* 0/1: the rally on Rusty Springs runs the cut moon track's textures and picture (lunar_springs.h).
                         * Last, so the positional initialisers above keep their meaning. */
    DashBox dash_box;   /* DASH_BOX_MAP */
    int movies_wide;    /* 0/1: on a wide screen a movie fills its width (the picture scaled up, its top and bottom cropped, tgv_wide.h);
                         * 0 the exe's look, the 4:3 frame in the middle. On. Last, as above. */
    int follow_camera;  /* 0/1: every camera follows the car's heading, the road behind drawn (race_follow.h); 0 the exe's.
                         * Off. Last, as above. */
    int extra_liveries;   /* 0/1: the car colours (the CUSTOMIZE switch; the key keeps its old name): every car offers its catalogue paints
                         * (car_paint.h) and, with the port.cfg psx_dir's files, the console version game's second livery (car_livery.h); the
                         * player's pick is car_paint, the rivals' are drawn from the race's seed; off, the exe's colours. Last, as dash_box. */
    uint8_t car_paint[CAR_PAINT_SLOTS];   /* the player's paint per car (the car showcase's Color box): PAINT_OWN 0 the car's own colour,
                                           * PAINT_SECOND 1 the console version livery, PAINT_CATALOGUE + k the k-th catalogue paint
                                           * (car_paint.h). Written as car_paint=<one hex digit a car>. */
    int reverse;        /* 0/1: a circuit driven the other way round (tri_reverse.h: the file's nodes run backwards at load);
                         * 0 the exe's. Off. Last, as car_paint. */
    int lunar_gravity;  /* 0/1: the moon's race (lunar_springs, track_gravity.h) pulls a car a third as hard and grips at two
                         * thirds; 0 the exe's gravity and grip. Off. Last, after reverse. */
    int difficulty;     /* 0 Easy, 1 Original, 2 Hard (difficulty.h): the rivals', the police's and the traffic's together; 1 the exe's. Last,
                         * after lunar_gravity. */
    int rally_gravel;   /* 0/1: the rally rolls on gravel (rally_gravel.h): a gravel noise all along, and dust from the desert's tyres; 0 the
                         * exe's. Off. Last, after lunar_gravity. */
    int update_check;   /* 0/1: THE switch for the update check (one request to our API at start, platform/update.h) and the three anonymous one-shot counters (port_stats.h); On; the EXTRAS row reads "Updates and anonymous stats". Last, after rally_gravel. */
} PortExtras;

/* the player's paint choice for `car` as the mode's file has it (PAINT_OWN for a car without a slot), and its write back: false
 * when the file does not take it */
int port_extras_car_paint(int car);
bool port_extras_set_car_paint(int car, int paint);

/* A record filed under these marks is not the normal one; the marks are what a race is tagged with. */
#include "game/sys/extras_marks.h"  /* EXTRAS_MARK_NITRO 1, _EXPLODE 2, _TURBO 4 */

PortExtras port_extras_defaults(void);
/* "key=value" per line; unknown keys and out-of-range values are ignored (the default stays). */
PortExtras port_extras_parse(const char *text, size_t size);
/* Writes the text form, NUL-terminated; returns its length, or 0 when it does not fit. */
size_t port_extras_format(const PortExtras *e, char *out, size_t size);
/* The extras of the mode in use: its table's values, with what its file says for what is free in it. */
/* one key=value over a PortExtras */
void port_extras_take(void *to, const char *key, const char *value);
PortExtras port_extras_load(void);
/* in the file of the mode in use, what differs from its values and is free in it */
bool port_extras_save(const PortExtras *e);
enum { PORT_EXTRAS_TEXT_MAX = 1024 };
/* The cheats that make a race's time an advantage one: NITRO 1, EXPLODE 2, TURBO 4. */
uint32_t port_extras_cheat_marks(const PortExtras *e);
/* port: the two switches a race turns with a key (F12 the stats overlay, F6 the car markers). A race starts with the
 * saved value, and a key in the race writes the new one back to the mode's file. */
typedef enum { RACE_SWITCH_STATS = 0, RACE_SWITCH_MARKERS = 1 } RaceSwitch;
/* port: the dash box's mode a race starts with, as the mode's file has it */
DashBox port_extras_dash_box(void);
int port_extras_race_on(RaceSwitch which);                      /* 0/1 as the mode's file has it */
bool port_extras_set_race_switch(RaceSwitch which, int on);     /* the file as it is, that switch set */
/* 1 when the speeds, distances and the cockpit's dial are in km/h: the player's choice. */
int speed_units_kmh(SpeedUnits units);

#endif

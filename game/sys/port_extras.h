/* port: the port's own extras (extras.cfg): the cheat codes as switches that last, and the handful of
 * things the original never offered as options. Kept apart from config.dat, which the game owns. */
#ifndef TNFS_PORT_EXTRAS_H
#define TNFS_PORT_EXTRAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { START_GAME_MODE = 0, START_NETWORK = 1, START_CENTRAL = 2 } StartScreen;

typedef struct {
    int mirror, rally, warrior, vegas, explode, nitro, turbo;   /* 0/1: SKCART, EARLYE, CONCEPTCAR, CASINOVILLE, ... */
    int replay_cameras, warrior_rival, no_movies, no_assists;   /* 0/1 */
    int modern_3d;    /* 0/1: the 3D clipped, unsnapped and perspective-correct, not the PS1-era mapper's warp */
    int texture_filter, fog;    /* 0/1, over Modern 3D (modern_road.h's ModernStyle); nothing without it */
    int bumper_mirror, bumper_radar;   /* 0/1: the rear-view mirror and the radar detector in the bumper view, under
                                        * the scoreboard (F7 / F8 turn them over in a race) */
    int variant_time; /* 0/1: the time of day offered with Mirror or Rally (fam_mirror.h), which the original hides */
    StartScreen start;                                          /* START_GAME_MODE */
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

#endif
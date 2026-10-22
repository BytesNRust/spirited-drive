/* Resource loading and the game's directory table.
 *
 * 0042a188 fills a table of directories (frontend\art\, frontend\movie\, ...)
 * from gamedata\config\paths.dat or built-in defaults; German swaps the
 * localised ones (art -> gart, speech -> gspeech, ...). The port keeps the
 * defaults, relative to the directory holding the CD data. */
#ifndef TNFS_RESOURCE_H
#define TNFS_RESOURCE_H

#include "game/sys/port_extras.h"  /* SpeedUnits */

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PATH_ART,      /* 00524328 */
    PATH_MOVIE,    /* 005243c8 */
    PATH_MUSIC,    /* 005242d8 */
    PATH_REPLAY,   /* 00524418 */
    PATH_CONFIG,   /* 00524198 */
    PATH_SLIDES,   /* 00524558: the loading pictures (004571e8) */
    PATH_COUNT
} GamePath;

void resource_init(const char *data_root, int german);
int resource_german(void);  /* 0052b31c: language, 0 English, 1 German */
/* port: the speed's units, apart from the language: UNITS_LANGUAGE (the default) takes them from resource_german, the
 * player's own choice (extras.cfg's units) overrides it. Set once from extras.cfg, and again when the screen saves. */
void resource_set_units(SpeedUnits units);
/* port: 1 when the speeds, the distances and the cockpit's dial are in km/h (0052b31c's German units, or the choice),
 * 0 for mph and miles. The words of the screens stay with resource_german. */
int resource_kmh(void);
const char *resource_dir(GamePath which);
const char *resource_root(void);  /* the directory holding the CD data, as resource_init got it */

/* 00476268: whole file, decompressed when it is a QFS. NULL on failure. */
uint8_t *resource_load(const char *path, size_t *size);
/* 00466c34: resource_load(art directory + name). */
uint8_t *resource_load_art(const char *name, size_t *size);
void resource_free(uint8_t *data);  /* 00475260 */

#endif

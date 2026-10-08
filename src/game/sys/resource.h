/* Resource loading and the game's directory table.
 *
 * 0042a188 fills a table of directories (frontend\art\, frontend\movie\, ...)
 * from gamedata\config\paths.dat or built-in defaults; German swaps the
 * localised ones (art -> gart, speech -> gspeech, ...). The port keeps the
 * defaults, relative to the directory holding the CD data. */
#ifndef TNFS_RESOURCE_H
#define TNFS_RESOURCE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PATH_ART,      /* 00524328 */
    PATH_MOVIE,    /* 005243c8 */
    PATH_MUSIC,    /* 005242d8 */
    PATH_REPLAY,   /* 00524418 */
    PATH_CONFIG,   /* 00524198 */
    PATH_COUNT
} GamePath;

void resource_init(const char *data_root, int german);
const char *resource_dir(GamePath which);

/* 00476268: whole file, decompressed when it is a QFS. NULL on failure. */
uint8_t *resource_load(const char *path, size_t *size);
/* 00466c34: resource_load(art directory + name). */
uint8_t *resource_load_art(const char *name, size_t *size);
void resource_free(uint8_t *data);  /* 00475260 */

#endif

/* config.dat in the running game: where it is read and written. */
#ifndef TNFS_CONFIG_H
#define TNFS_CONFIG_H

#include "game/frontend/config_dat.h"

/* 0042a010: reads the player's saved config.dat, else the one on the CD, else
 * the defaults. Returns 0 when a file was loaded, 1 when the defaults are in
 * use (the original returns the same, see 004373c4). Also sets
 * frontend_player_name. */
int config_load(void);

/* 0042a504: writes the config to the user's data directory, never to the CD
 * data directory. Takes the player's name from frontend_player_name. */
void config_save(void);

/* The loaded config; Control Central reads and updates it. */
ConfigDat *config_current(void);

/* port: 1 when the player's own saved config.dat was read by config_load (its name is the last one saved), 0 when the
 * name is the CD's or the defaults' (no name has been saved yet). */
int config_name_saved(void);

/* The same once config_load ran, else NULL (the viewer's paths that start
 * no game). */
const ConfigDat *config_loaded(void);

#endif

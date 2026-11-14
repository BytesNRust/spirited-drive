/* port (dep55): the first run of Restomod's records (race_records.h) with no race_records.dat: the store is made from what the player
 * has. config.dat's records seed the normal tables without cheats (each laps choice its lap count's table, the track's best lap and top
 * speed); the earlier builds' records.dat (a track's mirror and rally tables, and each one's table with cheats) and free_records.dat (a
 * free race's totals by lap count) are read into their tables, their rows' cheat marks kept. A row of those two files that is still one
 * of the CD's default records (the rows a table of theirs started with) is not the player's and is left out. The old files are only
 * read: nothing is written to them, nor removed. Their mirror-and-rally races were kept in the rally tables; they stay there. */
#ifndef TNFS_RECORDS_IMPORT_H
#define TNFS_RECORDS_IMPORT_H

#include "game/frontend/record_check.h"

/* Fills the (empty) store from config.dat's image and the old files, and saves it. The tables made. */
int records_import(const uint8_t config_image[RECORD_CHECK_IMAGE]);

#endif

/* port (dep59, 1b): the records of the cars a mod adds (car_registry.h), in a store of their own, "race_records_added.dat", beside
 * race_records.dat (Restomod's; Stock and Restored read neither). A table is keyed as race_records.h's and by the car's id: a car's
 * times are ranked in its own table, by the original's routines, and one car never pushes another's out. On disk a car is its id; a row
 * keeps its car word 0, and the car's number is put back for the screens (an id with no piece loaded has none). A record is never
 * dropped because its car is not installed any more.
 * port (dep59 step 4): the records by car, the stock cars' too (by their ids, formats/stock_content.h; a stock car's rows keep its number):
 * every Restomod race writes its car's table as well as race_records.dat. The file is race_records_cars.dat; step 3's,
 * race_records_added.dat, is read while there is no new one, and left as it was. The standard rows are copied in once
 * (race_records_cars_import), and the file says so. The race_records_added_* names stay. */
#ifndef TNFS_RACE_RECORDS_ADDED_H
#define TNFS_RACE_RECORDS_ADDED_H

#include "game/sys/race_records.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { RECORDS_ADDED_ID = 33, RECORDS_ADDED_SHORT = 9 };   /* an id and a short name with their NUL (mod_ini.h's sizes) */

/* The print the registry gives a car id (RecordsPrintOf, race_records.h; NULL, the default: none for any): a record is kept under the id and the
 * print the car has when it is found or made, as the tracks' are (race_records_tracks.h), so a car that changed has records of its own and
 * gets the old ones back when it is as it was. The records of the file before the prints wait for the print of their car, and the first look
 * under it takes them as its own, once. Set once, at start-up, before the records are loaded */
void race_records_added_prints_set(RecordsPrintOf car);

/* the table of a key (race_records_find's range) for the car `id`, or NULL */
RecordTable *race_records_added_find(int track, int layout, int cheats, int laps, const char *id);
/* the same, made empty when it is not there (nothing is saved); `short_name` is kept with it, the latest one given. NULL out of range,
 * when `id` is not a mod's id, or without memory */
RecordTable *race_records_added_make(int track, int layout, int cheats, int laps, const char *id, const char *short_name);
/* the best lap of (track, layout, cheats) for `id`; find and make as the tables' */
RecordRow *race_records_added_lap_find(int track, int layout, int cheats, const char *id);
RecordRow *race_records_added_lap_make(int track, int layout, int cheats, const char *id, const char *short_name);
/* the short name kept with a table, "" when there is none */
const char *race_records_added_short_name(int track, int layout, int cheats, int laps, const char *id);

/* The file's bytes (malloc'd): "TNFSRRA3", its flags (u32 LE: 1, the standard rows copied in), the tables' and the laps' counts (u32 LE), then each table (its key's four bytes, its id and
 * short name NUL-padded, one byte 1 for a record that waits for the print of its car (else 0), the car's print (32 bytes), the table as
 * race_records.dat writes one) and each lap (key with laps 0, id, short name, the waiting byte, the print, the row), ascending by key, then id,
 * then print, as bytes. NULL without memory */
uint8_t *race_records_added_image(size_t *size);
/* the store made the file in `file` (emptied first): 1; 0, and the store empty, for anything that is not exactly a good file. Step 3's
 * file (TNFSRRA1: no flags) and the one before the prints (TNFSRRA2) are read too: their entries wait for the print of their car */
int race_records_added_parse(const uint8_t *file, size_t size);
/* drops every table and lap, and the flag below */
void race_records_added_reset(void);
/* port (dep59 step 4): the standard store's rows (race_records.h) copied into their cars' tables: each totals row, top speed, segment row
 * and best lap whose car is a stock car's (0..7, the Warrior), after the rows there, with its marks; a table's best run's figures with its
 * first row. A row of 8 PACK, 9 CLOCK or 10 CLASS stays only in the standard store, which is not changed. 1 copied (the flag then set);
 * 0 the flag was set already. Nothing is saved */
int race_records_cars_import(void);
int race_records_cars_imported(void);
void race_records_cars_set_imported(int on);
/* <player folder>/race_records_cars2.dat (race_records_cars.dat, then race_records_added.dat, when there is no newer: read, never written) into the store, as race_records_load does
 * race_records.dat: a damaged one is kept as race_records_cars.bad (race_records_added.bad; .bad2 ..) and the store starts empty; with no copy kept, or a file there that could not be read, saving stays
 * off for the session */
RaceRecordsLoad race_records_added_load(void);
bool race_records_added_save(void);

#endif

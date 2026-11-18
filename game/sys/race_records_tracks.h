/* port (dep59 step 5, T5): the records of the tracks race_records.dat has no table for: Lunar Springs (its stock id "lunar",
 * formats/stock_content.h) and the tracks a mod adds (track_registry.h), in a store of their own, "race_records_tracks.dat"
 * (notes/dep59-step5-tracks-plan.md, decision 1). A table is keyed as race_records.h's, less the track number: the layout, the cheats and
 * the lap count (0 on a stage), then the track's id and the car's id (a stock car by its id, formats/stock_content.h; an added car by its
 * mod's). Its rows are the 0x27-byte rows of race_records.h with their marks, ranked by the original's routines in each car's own table
 * as race_records_cars.dat's. race_records.dat and race_records_cars.dat never hold these tracks, so an older version never sees a track
 * it does not know in them; it does not read this file at all. Restomod's only, as the other two. */
#ifndef TNFS_RACE_RECORDS_TRACKS_H
#define TNFS_RACE_RECORDS_TRACKS_H

#include "game/sys/race_records.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { RECORDS_TRACK_ID = 33, RECORDS_TRACK_CAR_ID = 33, RECORDS_TRACK_SHORT = 9 };   /* the ids and a short name with their NUL */

/* The current print of the piece with an id (RecordsPrintOf, race_records.h): the registries answer, the track one for a track id, the car one
 * for a car id; unset (NULL, the default) every piece has none. A record is kept under the ids
 * and the prints the pieces have when it is found or made, so a piece that changed has records of its own and gets the old ones back when it is
 * as it was; what is not found by prints is not lost. Set once, at start-up, before the records are loaded */
void race_records_tracks_prints_set(RecordsPrintOf track, RecordsPrintOf car);

/* the table of a key for the track `track` and the car `car`, or NULL (a key out of range: layout 0..7, cheats 0..1, laps 0..99) */
RecordTable *race_records_tracks_find(const char *track, int layout, int cheats, int laps, const char *car);
/* the same, made empty when it is not there (nothing is saved); `short_name` (an added car's, NULL to leave it) kept with it. NULL out of
 * range, for an id that is not one, or without memory */
RecordTable *race_records_tracks_make(const char *track, int layout, int cheats, int laps, const char *car, const char *short_name);
/* the best lap of (track, layout, cheats) for `car`; find and make as the tables' */
RecordRow *race_records_tracks_lap_find(const char *track, int layout, int cheats, const char *car);
RecordRow *race_records_tracks_lap_make(const char *track, int layout, int cheats, const char *car, const char *short_name);
/* The ids of the cars that have a table or a best lap for `track` (any key), ascending, at most `max`, in `ids`: how many. The ids point into
 * the store and are good until it is reset or loaded again; the screens join these cars' tables (race_records_merge.h) */
int race_records_tracks_cars(const char *track, const char *ids[], int max);
/* the short name kept with a table, "" when there is none */
const char *race_records_tracks_short_name(const char *track, int layout, int cheats, int laps, const char *car);

/* The file's bytes (malloc'd): "TNFSRRT2", its flags (u32 LE, 0), the tables' and the laps' counts (u32 LE), then each table (its key's
 * four bytes: layout, cheats, laps, and 1 for a record that waits for the print of its pieces, else 0; the track's id, the car's id and the
 * short name NUL-padded; the track's print and the car's, 32 bytes each; the table as race_records.dat writes one) and each lap (key with
 * laps 0, the names, the prints, the row), ascending by key (not its last byte), track, car, then the prints, as bytes. NULL without memory */
uint8_t *race_records_tracks_image(size_t *size);
/* the store made the file in `file` (emptied first): 1; 0, and the store empty, for anything that is not exactly a good file. A "TNFSRRT1"
 * file (the records before the prints: no prints in its entries) is good too: its entries wait for the pieces' prints, and the first look
 * under them (a find, a make, the cars of the track) takes each as theirs, once */
int race_records_tracks_parse(const uint8_t *file, size_t size);
/* drops every table and lap */
void race_records_tracks_reset(void);
/* <player folder>/race_records_tracks2.dat into the store (race_records_tracks.dat, the one before the prints, when there is no new one: read, never
 * written; an older version goes on with it), as race_records_load does race_records.dat: a damaged one is kept as
 * race_records_tracks.bad (.bad2 ..) and the store starts empty; with no copy kept, or a file there that could not be read, saving stays
 * off for the session */
RaceRecordsLoad race_records_tracks_load(void);
bool race_records_tracks_save(void);

/* The track the race now running keeps its records under here: "lunar" for the moon's race, an added track's id; NULL (the default) for a
 * track race_records.dat has. Set at the race's start (track_viewer.c), as race_records_conditions_set; an id that is not one is NULL */
void race_records_tracks_race_set(const char *track);
const char *race_records_tracks_race(void);

#endif

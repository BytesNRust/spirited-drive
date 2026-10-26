/* port (free records): the ten best totals of a free race (free_laps.h) for a lap count the track lacks, kept apart from the exe's tables
 * (config.dat's, records.dat's) in "free_records.dat": a sparse table per (circuit track, variant, cheats, laps), each laid out as the
 * exe lays out a laps choice's ten rows (0x186 bytes, 0x27 each), so record_check_totals runs on it unchanged (records_route.c copies it into
 * a scratch image's first choice). Behind the port flag free_records (port_settings.h). */
#ifndef TNFS_FREE_RECORDS_H
#define TNFS_FREE_RECORDS_H

#include "game/sys/port_records.h"

#include <stdbool.h>
#include <stdint.h>

enum { FREE_RECORDS_TABLE = 0x186, FREE_RECORDS_ROWS = 10, FREE_RECORDS_ROW = 0x27, FREE_RECORDS_TIME = 0x1b };

typedef struct {
    uint8_t rows[FREE_RECORDS_TABLE];   /* the ten rows of a laps choice's table */
    uint8_t marks[FREE_RECORDS_ROWS];   /* EXTRAS_MARK_* of each row, as records.dat's marks (all 0 without cheats) */
} FreeTable;

/* The table of a circuit (track 3..8) in a variant, with cheats (1) or without (0), for `laps` (1..99); NULL when there is none (or the key
 * is out of range). The pointer stays valid until free_records_reset. */
FreeTable *free_records_find(int track, RecordVariant v, int cheats, int laps);
/* The same, made from `seed` (FREE_RECORDS_TABLE bytes, no marks) when absent; NULL out of range. Making one is not a record: nothing is saved. */
FreeTable *free_records_make(int track, RecordVariant v, int cheats, int laps, const uint8_t seed[FREE_RECORDS_TABLE]);
/* The lap counts a (track, variant, cheats) has a table for, ascending, at most `max` of them; how many. */
int free_records_laps(int track, RecordVariant v, int cheats, int *out, int max);

/* The exe's table of laps choice `choice` (0..2) of a circuit as the CD's config.dat has it by default (not the player's): the seed's source.
 * 0 when out of range. */
int free_records_exe_rows(int track, int choice, uint8_t out[FREE_RECORDS_TABLE]);
/* A new table's rows for `laps` on a circuit: the exe table of the lap count nearest it (the lower on a tie), each row's time scaled by
 * laps over that count (kept to a positive int32). */
void free_records_seed(int track, int laps, uint8_t out[FREE_RECORDS_TABLE]);

/* "free_records.dat": loaded once when needed. A missing, damaged or foreign file is an empty store (false, logged when damaged). */
bool free_records_load(void);
bool free_records_save(void);
/* drops every table (the tests', and a load's start) */
void free_records_reset(void);

#endif

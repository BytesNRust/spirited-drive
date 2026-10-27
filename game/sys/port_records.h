/* port: the records the original has no place for (user file "records.dat"): a track's variants (mirror, rally) and
 * the table "with cheats". The original's records (config.dat's image, record_check.h) stay as they are for a normal
 * race without cheats; any other race writes here, in blocks laid out as a track's in config.dat (best_times.h's
 * BEST_TIMES_TRACK bytes from 0xb5 + 0xa6b x track), so the original's own routines can run on them. */
#ifndef TNFS_PORT_RECORDS_H
#define TNFS_PORT_RECORDS_H

#include "game/sys/extras_marks.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum { RECORDS_NORMAL = 0, RECORDS_MIRROR = 1, RECORDS_RALLY = 2 } RecordVariant;

enum {
    RECORDS_TRACKS = 9,
    RECORDS_BLOCK = 0xa6b,    /* a track's records, as config.dat keeps them */
    RECORDS_MARK_SLOTS = 64   /* the rows a track's block has marks for (records_route.h's slots) */
};

/* The block of a track (0..8) in a variant, in the table "with cheats" (1) or without (0). Normal without cheats is
 * config.dat's own and has no block here: NULL, as for anything out of range. A block starts as the original's
 * defaults (00429278's), and stays as written until port_records_save. */
uint8_t *port_records_block(int track, RecordVariant v, int with_cheats);
/* The marks of the table with cheats of a track in a variant: a byte per slot, EXTRAS_MARK_* bits set. NULL out of
 * range. */
uint8_t *port_records_marks(int track, RecordVariant v);

/* The block of a track as the CD's config.dat has it by default (the built-in defaults when there is no CD), not the player's records: the
 * seed of a table that starts later (free_records.h). NULL out of range. */
const uint8_t *port_records_default_block(int track);

/* The cheats a tournament in progress began with (EXTRAS_MARK_* bits), kept with the records so that a tournament
 * left half way and taken up later still ends in the table it began for. */
int port_records_tournament(void);
void port_records_set_tournament(int marks);

/* "records.dat": the blocks and marks as saved; the defaults (and no cheats for the tournament) when the file does
 * not exist or is not one of ours. 1 when a file was read. */
bool port_records_load(void);
bool port_records_save(void);

#endif

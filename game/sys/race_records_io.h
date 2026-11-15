/* port (dep55, dep59): the pieces of race_records.dat's format that another store of records shares (race_records_added.h): a row, a
 * table, their key's range, the empty table and the copy of a damaged file. Not for the screens: they use race_records.h. */
#ifndef TNFS_RACE_RECORDS_IO_H
#define TNFS_RACE_RECORDS_IO_H

#include "game/sys/race_records.h"

#include <stddef.h>
#include <stdint.h>

enum {
    RACE_RECORDS_ROW_BYTES = RACE_RECORDS_ROW + 4,   /* a row and its four marks */
    RACE_RECORDS_TABLE_ROWS = RACE_RECORDS_ROWS + 1 + 2 * RACE_RECORDS_SEGMENTS,
    RACE_RECORDS_TABLE_BYTES = RACE_RECORDS_TABLE_ROWS * RACE_RECORDS_ROW_BYTES + 2 * RACE_RECORDS_SEGMENTS * 4
};

uint8_t *race_records_put32(uint8_t *at, uint32_t v);
uint32_t race_records_get32(const uint8_t *at);
uint8_t *race_records_put_row(uint8_t *at, const RecordRow *r);
const uint8_t *race_records_get_row(const uint8_t *at, RecordRow *r);
uint8_t *race_records_put_table(uint8_t *at, const RecordTable *t);
const uint8_t *race_records_get_table(const uint8_t *at, RecordTable *t);
void race_records_table_empty(RecordTable *t);
/* (track, layout, cheats) in range; a table's key with its laps (a stage 0, a circuit 1..99) */
int race_records_base_ok(int track, int layout, int cheats);
int race_records_table_key_ok(int track, int layout, int cheats, int laps);
/* a damaged `<stem>.dat`'s copy, under the first of <stem>.bad, .bad2 .. .bad9 not taken, logged; 0 when none could be written */
int race_records_keep_bad(const char *stem, const uint8_t *data, size_t size);

#endif

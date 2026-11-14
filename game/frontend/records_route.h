/* port: where a finished race's records go. With port_records_unified off (Stock, Restored) the original's: config.dat's image, one table
 * per track, every race mixed (mirror, rally, cheats), the Warrior none, as the exe does; a free race (laps=N) only for a lap count the
 * track has. With it on (Restomod) race_records.h's table of the race's track, layout, cheats and laps, every row with its marks.
 * record_check.c's routines run unchanged either way: on the image, or on a copy of it whose track block is made from the race's table. */
#ifndef TNFS_RECORDS_ROUTE_H
#define TNFS_RECORDS_ROUTE_H

#include "game/frontend/record_check.h"
#include "game/sys/race_records.h"

typedef struct {
    int layout;   /* LAYOUT_* bits of race_records.h */
    int marks;    /* EXTRAS_MARK_* of the cheats on when the race began; 0: no cheats */
} RaceKind;

typedef enum { ROUTE_SEGMENT, ROUTE_LAP, ROUTE_TOP, ROUTE_TOTALS } RouteOp;

/* The slots of a track's marks the results screens read (ResultsMarks): a stage's three segment times (a circuit's best lap in the
 * first), the top speed, the ten best totals of each laps choice, each segment's top speed. */
enum { SLOT_SEGMENT = 0, SLOT_TOP = 3, SLOT_TOTALS = 4, SLOT_CHOICE = 10, SLOT_SPEED_SEGMENT = 34, RECORDS_MARK_SLOTS = 64 };
int records_slot_totals(int track, int choice, int row);   /* the slot of a totals row; -1 out of range */

/* The laps choice whose table of totals the race's records and results use: the image's own (the menu's), in a free race the choice its
 * lap count is when the track has it; 0 with port_records_unified (the race's table is put in the first choice's place). */
int records_choice(const uint8_t image[RECORD_CHECK_IMAGE]);

/* The race's kind from the flags the original keeps and the reversed track: every layout its own */
RaceKind records_race_kind(int variant_m, int variant_r, int reversed, int explode, int nitro, int turbo);
/* The kind a race is checked on once its player's car is known: the Warrior (car 11) is on the cheats table with its mark */
RaceKind records_kind_with_car(RaceKind kind, int car);
/* 1 when the kind takes the table with cheats */
int records_has_cheats(RaceKind kind);

/* The lap count of the race's table: 0 on a stage, the free count in a free race, else the menu's choice's count */
int records_race_laps(const uint8_t image[RECORD_CHECK_IMAGE]);

/* The marks a row the race sets takes: its cheats and the conditions it ran in (race_records_conditions) */
RecordMarks records_row_marks(RaceKind kind);

/* 0045bad8 (ROUTE_SEGMENT), 0045b908 (ROUTE_LAP), 0045c2d0 (ROUTE_TOP) or 0045be50 (ROUTE_TOTALS, with `times`) for a race of this kind:
 * the record_check.h routine's answer. A table of race_records.h's that changed is saved at once. */
int32_t records_route(RaceKind kind, RouteOp op, uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player,
                      const int32_t times[RECORD_CHECK_TIMES]);

/* The image a screen shows the race's records in: `image`, and with port_records_unified its track block made from the race's table (its
 * totals in the first choice's place, the choice word 0), so the row a check set is the row shown; and the marks of that table's rows in
 * the slots above (`slots`, RECORDS_MARK_SLOTS bytes; all 0 without cheats). */
void records_route_shown(RaceKind kind, const uint8_t image[RECORD_CHECK_IMAGE], uint8_t out[RECORD_CHECK_IMAGE], uint8_t slots[RECORDS_MARK_SLOTS]);

/* The race's table as a line of words, "MIRROR + RALLY - CHEATS - 6 LAPS" (a stage's without laps); `laps` 0 for none */
void records_table_line(int layout, int cheats, int laps, char *out, int size);
/* The layout's words: NORMAL, MIRROR, REVERSE, RALLY, or those on joined with " + " */
void records_layout_text(int layout, char *out, int size);

/* the letters of a row's cheats, "N" nitro, "E" explode, "T" turbo, "W" Warrior, "L" lunar gravity ("" for none) */
void records_mark_text(int marks, char out[6]);

#endif

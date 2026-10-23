/* port: where a finished race's records go (not in the original, which has one table per track in config.dat): a
 * normal race without cheats still takes config.dat's, a mirror or rally race and any race with a cheat that helps
 * (NITRO, EXPLODE, Turbo) take the port's (port_records.h). record_check.c's routines run unchanged, on a copy of the
 * image whose track block is the port's. */
#ifndef TNFS_RECORDS_ROUTE_H
#define TNFS_RECORDS_ROUTE_H

#include "game/frontend/record_check.h"
#include "game/sys/port_records.h"

typedef struct {
    RecordVariant variant;
    int marks;   /* EXTRAS_MARK_* of the cheats on when the race began; 0: the table without cheats */
} RaceKind;

typedef enum { ROUTE_SEGMENT, ROUTE_LAP, ROUTE_TOP, ROUTE_TOTALS } RouteOp;

/* The rows of a track's block a mark is kept for: a stage's three segments, the circuit's top speed, then the ten
 * best totals of each laps choice (a circuit's three, the first three tracks' one). */
enum { SLOT_SEGMENT = 0, SLOT_TOP = 3, SLOT_TOTALS = 4, SLOT_CHOICE = 10 };
int records_slot_totals(int track, int choice, int row);   /* the slot of a totals row; -1 out of range */

/* The race's kind from the flags the original keeps (rally before mirror when both are set). */
RaceKind records_race_kind(int variant_m, int variant_r, int explode, int nitro, int turbo);
/* The kind a race is checked on once its player's car is known: the Warrior (car 11) is on the cheats table with its
 * mark, as the original records nothing for it. */
RaceKind records_kind_with_car(RaceKind kind, int car);

/* 0045bad8 (ROUTE_SEGMENT), 0045b908 (ROUTE_LAP), 0045c2d0 (ROUTE_TOP) or 0045be50 (ROUTE_TOTALS, with `times`) for
 * a race of this kind: the record_check.h routine's answer. A table of the port's that changed is saved. */
int32_t records_route(RaceKind kind, RouteOp op, uint8_t image[RECORD_CHECK_IMAGE],
                      const uint8_t game[RECORD_CHECK_GAME], int32_t player, const int32_t times[RECORD_CHECK_TIMES]);

/* the letters of a row's marks, "N" nitro, "E" explode, "T" turbo, "W" Warrior ("" for none) */
void records_mark_text(int marks, char out[5]);

#endif

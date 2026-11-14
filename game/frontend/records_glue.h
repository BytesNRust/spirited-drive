/* port: the glue between the menus' race and records_route.h: the kind of race that is under way, kept from its start (the cheats may be
 * spent or cleared by the time its records are taken), and Restomod's store (race_records.h), loaded the first time it is wanted and
 * made from config.dat and the old files when there is none yet (records_import.h). */
#ifndef TNFS_RECORDS_GLUE_H
#define TNFS_RECORDS_GLUE_H

#include "game/frontend/best_times.h"
#include "game/frontend/records_route.h"
#include "game/frontend/results_screen.h"

enum { RECORDS_TOURNAMENT = 3 };   /* 00524e5d's race type */

/* Restomod's store loaded (once; a missing file is made from `config_image` and the old files). Nothing with port_records_unified off. */
void records_glue_load(const uint8_t config_image[RECORD_CHECK_IMAGE]);

/* A race begins: `races` is the game state's +0x592 (a tournament's races so far by track: all 0 on its first, which keeps the cheats on
 * then for the totals the tournament ends with); `turbo` is 0052b306 & 8. */
void records_glue_begin(const uint8_t config_image[RECORD_CHECK_IMAGE], int race_type, const uint8_t *races, int variant_m,
                        int variant_r, int explode, int nitro, int turbo);
/* The race's kind (its reversed track as raced; a tournament's totals take the cheats it began with) */
RaceKind records_glue_kind(RouteOp op);
int32_t records_glue_route(RouteOp op, uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player,
                           const int32_t times[RECORD_CHECK_TIMES]);

/* The image a results screen shows after `op` (records_route_shown) and its marks: `out->slots` NULL unless the table is Restomod's with
 * cheats, `out->line` the table's words under the title with port_records_unified (NULL without) */
void records_glue_shown_as(RouteOp op, uint8_t out[RECORD_CHECK_IMAGE], const uint8_t *image, ResultsMarks *marks);
/* 1 when the race a table of `op` is shown for ran with cheats and the records are Restomod's: the results screens' and the standings'
 * "(CHEATS)" */
int records_glue_cheats(RouteOp op, const uint8_t *image);
/* Best Times' pages (Restomod's), opened on the track screen's table: its layout (the reversed track it would race), cheats (the
 * Warrior's too) and laps (laps=N, else the menu's choice's); NULL with port_records_unified off: the exe's screen */
BestTimesPlus *records_glue_best_plus(const uint8_t *image, int variant_m, int variant_r, int explode, int nitro, int turbo, int car);
/* The table's words for the standings (NULL with port_records_unified off) */
const char *records_glue_line(RouteOp op, const uint8_t *image);

#endif

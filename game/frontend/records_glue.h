/* port: the glue between the menus' race and records_route.h: the kind of race that is under way, kept from its start
 * (the cheats may be spent or cleared by the time its records are taken). */
#ifndef TNFS_RECORDS_GLUE_H
#define TNFS_RECORDS_GLUE_H

#include "game/frontend/best_times.h"
#include "game/frontend/records_route.h"

enum { RECORDS_TOURNAMENT = 3 };   /* 00524e5d's race type */

/* A race begins: `races` is the game state's +0x592 (a tournament's races so far by track: all 0 on its first, which
 * keeps the cheats on then for the totals the tournament ends with); `turbo` is 0052b306 & 8. */
void records_glue_begin(int race_type, const uint8_t *races, int variant_m, int variant_r, int explode, int nitro,
                        int turbo);
/* The race's kind (a tournament's totals take the cheats it began with). */
RaceKind records_glue_kind(RouteOp op);
int32_t records_glue_route(RouteOp op, uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME],
                           int32_t player, const int32_t times[RECORD_CHECK_TIMES]);

/* The records screen's tables for the variant the flags name: the table without cheats (config.dat's `image` records
 * for a normal one) and the one with, with the marks, the one without shown first. `image` is config.dat's image. */
BestTimesPlus *records_glue_best_plus(const uint8_t *image, int variant_m, int variant_r);
/* 1 once Tab went down since it was last asked (BestTimesOps.plus_toggle) */
int32_t records_glue_toggle(void *ctx);

#endif

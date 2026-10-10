/* The two zone records (ai.zones, one per human) and which car each follows. */
#ifndef TNFS_AI_ZONES_H
#define TNFS_AI_ZONES_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0046a29c: car `slot` takes zone `zone` from the car holding it, which
 * gets `old` in exchange; the computer cars of either zone swap with them. */
void ai_swap_zones(RaceState *race, int32_t slot, int32_t zone, int32_t old);

/* 0046a344: the humans take the zones in race order, the leading one zone 0;
 * each one moved sounds note 0x50. */
void ai_zones_by_place(RaceState *race);

/* 0046a3c0: with 2 humans, the zones are handed out by place and each set
 * around its human's progress: the seen span from 12 behind to 43 ahead, the
 * keep span 8 inside it at the back and 11 past it ahead; seen spans that
 * cross are split between the two. Spans clamped at 0 and masked to the ring. */
void ai_zone_spans(RaceState *race);

#endif

/* A car's cursor over the track's scenery list (00509788, the TRI records
 * after the descriptors, ordered by node and ended by a node of -1). */
#ifndef TNFS_CAR_SCENERY_H
#define TNFS_CAR_SCENERY_H

#include "formats/tri.h"
#include "game/race/car.h"
#include "game/race/race_state.h"

#include <stdint.h>

/* 0040fbc4: whether a scenery record stands at `node`, walking the car's
 * cursor (scenery_node, scenery_index) forward or back to it. As the
 * original, the cursor never steps onto the last record going forward nor
 * past record 0 going back, so a record found only there reads as none. */
int32_t car_scenery_at(Car *car, const Tri *tri, int32_t node);

/* 0043cc10: the force-feedback jolt of a car hitting scenery, by its speed
 * (local z velocity while a player watches their own car, else ai_speed)
 * and which way the hit came from (`across` below 1 or not). */
void car_scenery_jolt(const Car *car, const RaceState *race, int32_t across);

/* 004501fc (): with 004fbb0c set and playback not 1, each scenery record's
 * state byte copied to the byte after it (header +0x16218 records). */
void race_scenery_keep(RaceState *race);

/* 00450250 (): with 004fbb0c set and playback not 1, each record's second
 * state byte copied back to its first (004501fc's undone). */
void race_scenery_restore(RaceState *race);

/* 0043f51c: the track's scenery streamed in again (a knocked object goes). */
void race_scenery_restream(RaceState *race);

/* 0040fef0: whether the scenery point `point` (x, y, z; y unused) lies in
 * the car's box (half its width +0x64e sideways, half its length +0x64a
 * ahead, turned by its yaw); a hit jolts (car_scenery_jolt, when effects are on) and sounds when
 * the car is within 9 nodes of the camera. */
int32_t car_scenery_hit(const Car *car, RaceState *race, const int32_t point[3]);

/* 0040fc64: whether the car knocks over a standing scenery object at its
 * node: the cursor steps back off the node, then walks to every record on
 * it (forward, or back when the node lies behind the cursor), each tried
 * at its node's position plus its offset x 256 (0040fef0). The first hit is
 * marked knocked over at this minute and the scenery streamed again. */
int32_t car_knocks_scenery(Car *car, RaceState *race);

#endif

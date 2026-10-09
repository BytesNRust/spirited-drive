/* Where the computer cars (traffic, police and opponents) are placed and
 * moved between lanes and zones (0046a238 and what it calls). A zone is one
 * of the two records at ai.zones, one per human player. */
#ifndef TNFS_AI_LANES_H
#define TNFS_AI_LANES_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* The four flag bytes at car +0x174 read as one word. Bits the lane code
 * reads or writes; the rest are not understood yet. */
enum {
    AI_RACER = 0x4,             /* +0x174: an opponent that starts on the grid, not traffic */
    AI_COP = 0x8,               /* +0x174: the police car */
    AI_SETTLED = 0x10,          /* +0x174: cleared when the speeds are set (0046e924) */
    AI_PLACE_BEHIND = 0x400,    /* +0x175: put behind the first car, not ahead (0046ea04) */
    AI_ONCOMING = 0x1000,       /* +0x175: drives against the first car's direction */
    AI_PLACE_REQUEST = 0x2000,  /* +0x175: asks for AI_PLACE_BEHIND (0046e848) */
    AI_LOST_8000 = 0x8000,      /* +0x175: cleared on every placement (0046e848) */
    AI_COP_STAGE_MASK = 0x700000 /* +0x176 bits 4..6: how often the police car was placed (0046a18c) */
};

/* 0046a070: the node lies in the span zone `zone` lets cars stay in. */
int ai_in_keep_span(const RaceState *race, int32_t node, int32_t zone);
/* 0046a0f4: the node lies in the span the player sees. */
int ai_in_view_span(const RaceState *race, int32_t node, int32_t zone);
/* 0046a0c8: 1 + the zone whose keep span holds the node, else 0. */
int ai_keep_zone(const RaceState *race, int32_t node);
/* 0046a14c: the same for the view spans; always 1 with three racers or more. */
int ai_view_zone(const RaceState *race, int32_t node);

/* 0046a5fc: ends the police chase; `clear_flag` also clears the police
 * car's flag in +0x176 bit 3. */
void ai_release_cops(RaceState *race, int32_t clear_flag);

/* 0046a238: keeps a computer car in a zone, or places it again. Returns 1
 * when it was left where it is. */
int ai_lane_update(Car *car, RaceState *race);
/* 0046a18c: tries the zones in turn to place the car. */
void ai_find_spawn(Car *car, RaceState *race);

/* 0046ef18: picks a node and lane for the car and puts it there. */
void ai_place_car(Car *car, RaceState *race, int32_t activate);
/* 0046ea04: the lane (0 oncoming) and node to place the car at. */
void ai_choose_node(const Car *car, RaceState *race, int32_t *lane, int32_t *node);
/* 0046ec38: opponents on the grid: the lateral place and the node. */
void ai_grid_slot(Car *car, const RaceState *race, int32_t *node);
/* 0046f110: puts the car at the node with the lateral offset. */
void ai_respawn(Car *car, RaceState *race, int32_t node, int32_t lane, int32_t offset, int32_t activate);

/* 0046e848 */
void ai_respawn_flags(Car *car, RaceState *race);
/* 0046e924 */
void ai_set_speeds(Car *car, RaceState *race, int32_t lane);
/* 0046e6e4 */
void ai_snap_to_road(Car *car, RaceState *race);
/* 00470028 */
void ai_road_frame(Car *car, RaceState *race);
/* 00470a34 */
void ai_new_bias(Car *car, RaceState *race);
/* 00470998 */
int32_t ai_scale_speed(const Car *car, int32_t speed);
/* 0046aab0 */
int32_t ai_leader_speed(const Car *car, const RaceState *race);
/* The driver profile slot `slot`'s ai.profiles word points at, or NULL. */
const DriverProfile *ai_driver_profile(const RaceState *race, int32_t slot);

#endif

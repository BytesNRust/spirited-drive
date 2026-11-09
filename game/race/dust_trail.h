/* port (rally_gravel.h): the dust a rolling car leaves on the desert and the moon's rally, as a trail: puffs born at its rear wheels that stay where
 * they were dropped, low, rise a little, spread and fade as the car draws away. The ring is the followed car's alone, filled one tick at a time
 * from the race's tick (the draw may run many times a tick); the screen's part, the projection and the sprites, is ported_view.c's. */
#ifndef TNFS_DUST_TRAIL_H
#define TNFS_DUST_TRAIL_H

#include <stdint.h>

enum { DUST_TRAIL_PUFFS = 192 };

typedef struct {
    int32_t pos[3];    /* the world place it was dropped at (x, y, z as the cars have them) */
    int32_t born;      /* the race tick */
    int32_t drift[2];  /* a tick's drift outward on x and z: across the car's heading, away from its wheel's side */
    int32_t seed;      /* its own random, for the size and the sprite's offset */
} DustPuff;

typedef struct {
    DustPuff puff[DUST_TRAIL_PUFFS];
    int32_t next;      /* the ring slot dropped next */
    int32_t last_tick; /* the last tick stepped, -1 none yet */
} DustTrail;

void dust_trail_reset(DustTrail *trail);

/* The race is at `tick`: for each tick since the last call (at most 12: a long pause leaves a gap, not a burst) drop a puff at each rear wheel of a car
 * at `car` (world place) heading `yaw` (24-bit angle) when `drop`; a tick before the last one is placed back along the way the car came, `speed` (16.16 metres
 * a second) a tick; a tick that went back clears the ring (a new race). `kind` is rally_gravel_kind's. */
void dust_trail_step(DustTrail *trail, int32_t tick, int kind, int drop, const int32_t car[3], int32_t yaw, int32_t speed);

/* A puff at `tick`: its world place (the drop's plus the rise and drift of its age) and its size, 0x100 a standard puff. 0 when it is gone (older
 * than the kind's life, or not yet born). `lunar` is the moon's pull: the regolith then hangs longer and falls slower. */
int dust_puff_at(const DustPuff *puff, int32_t tick, int kind, int lunar, int32_t place[3], int32_t *size);

/* How many ticks a puff of this kind lives */
int32_t dust_life(int kind, int lunar);

#endif

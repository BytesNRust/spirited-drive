/* Steering by the mouse (004380e8, from the keyboard poll 0043915c): the
 * pointer's x over a 0x100 wide range read as a steering value. */
#ifndef TNFS_MOUSE_STEER_H
#define TNFS_MOUSE_STEER_H

#include <stdint.h>

typedef struct {
    void *ctx;
    /* 0047723c (cdecl): the pointer's range */
    void (*range)(void *ctx, int32_t left, int32_t top, int32_t right, int32_t bottom);
    int32_t (*buttons)(void *ctx);  /* [004c6194] */
    int32_t (*x)(void *ctx);        /* 004c6110, read after the range is set */
} MouseSteerOps;

typedef struct {
    int16_t replay;           /* 0052b316 */
    int32_t viewed;           /* 004fbb04 */
    int32_t player;           /* 004fbb08 */
    int32_t unknown_513e6c;   /* 00513e6c */
    int32_t mirrored;         /* 004fbad8 */
    int32_t buttons;          /* 004fb794: written */
    int32_t steer;            /* 004fb798: written */
} MouseSteerWords;

/* 004380e8 (no arguments): in a replay, with the viewed car not the
 * followed one, or with 00513e6c set: steer = 0x20 (centred). Else the
 * range (0, 0, 0x100, 0x40), buttons = [004c6194](), steer = x >> 2
 * (0x40 - it when mirrored); 0x1e..0x21 become 0x20, above 0x3c 0x3c,
 * below 0 0. */
void mouse_steer(const MouseSteerOps *ops, MouseSteerWords *w);

#endif

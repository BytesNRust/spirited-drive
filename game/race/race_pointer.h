/* The mouse pointer over the race's panels (004418a4 / 00441938): what lies
 * under it kept, the pointer drawn, and put back on the next frame. */
#ifndef TNFS_RACE_POINTER_H
#define TNFS_RACE_POINTER_H

#include "game/race/hud_mask.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*select)(void *ctx, HudSurface s);                                  /* 00473680(004c2da0) / 004735dc */
    void (*grab)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);      /* 00476e28 */
    void (*draw)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);      /* 00473ad0: at x, y less its hot spot */
    void (*place)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);     /* 004737d0 */
} RacePointerOps;

typedef struct {
    int32_t back_buffer;        /* 004b95b4 */
    int32_t x, y;               /* 004c6110, 004c6114: the pointer */
    int32_t phase;              /* 004c5390 */
    uint8_t *const *shapes;     /* 00509ebc..: each bank's +0x2c; [0] the kept background, [1 + i] pointer i */
} RacePointerInputs;

typedef struct {
    int16_t x, y;               /* 00524794, 00524796: where the background was kept */
    int32_t phase;              /* 004bc6a0: the phase it was kept in */
} RacePointerKept;

/* 004418a4 (watcall (pointer)): the screen selected (004b95b4: 00473680
 * (004c2da0), else 004735dc); the background at x, y grabbed into shapes[0];
 * shapes[1 + pointer] drawn there; x, y (as shorts) and the phase kept. */
void race_pointer_show(const RacePointerOps *ops, const RacePointerInputs *in, RacePointerKept *kept, int32_t pointer);

#endif

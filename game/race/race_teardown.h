/* The race's teardown frees (00455a5c's end): each frees what a setup
 * allocated through 00475260 and clears the pointer it was held in. The
 * port's owners free their own, so --drive runs none of them; they are
 * here as the exe has them. */
#ifndef TNFS_RACE_TEARDOWN_H
#define TNFS_RACE_TEARDOWN_H

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*free)(void *ctx, uint32_t block);   /* 00475260 */
} RaceFreeOps;

/* 0040d6f4 (watcall ()): free the scenery state buffer (004b9098), then
 * the pointer = 0. */
void race_scenery_state_free(const RaceFreeOps *ops, uint32_t *scenery_state);

/* 0045106c (watcall ()): with the "RRSM_Buffer" (00513e3c, 0x162400 bytes
 * from 00451040) held, free it and the pointer = 0. */
void race_rrsm_free(const RaceFreeOps *ops, uint32_t *rrsm);

/* 004c2d88..004c2da3: the draw's surfaces (0044ca3c) and two flags. */
typedef struct {
    int32_t set_up;     /* 004c2d88: 0044ca3c ran */
    int32_t drawn;      /* 004c2d8c: 0044d26c drew */
    uint32_t view;      /* 004c2d90: the view surface */
    uint32_t mirror;    /* 004c2d94: the mirror's picture */
    uint32_t back;      /* 004c2d98 */
    uint32_t map;       /* 004c2d9c */
    uint32_t frame;     /* 004c2da0: with a back buffer (004b95b4) */
} RaceDrawSurfaces;

typedef struct {
    void *ctx;
    void (*free)(void *ctx, uint32_t block);   /* 00475260 */
    void (*track_info_free)(void *ctx);        /* 0043b924 */
    void (*track_free)(void *ctx);             /* 0043e8bc */
} RaceDrawFreeOps;

/* 0044d2e4 (watcall ()): the mirror, view, back and map surfaces, then
 * with a back buffer the frame, each freed and cleared when held; then
 * track_info_free, track_free, and both flags 0. */
void race_draw_free(const RaceDrawFreeOps *ops, RaceDrawSurfaces *s, int32_t back_buffer);

#endif

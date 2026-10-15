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

/* 00446acc (watcall ()): with the pointer's bank (004bcbd0, gmouse.fsh
 * from 00446a0c) held, free it and the pointer = 0. */
void race_pointer_bank_free(const RaceFreeOps *ops, uint32_t *bank);

/* 0043b924 (watcall ()): the sky ring's object (004fccf0, 0043abfc's)
 * freed whatever it holds; not cleared. */
void race_sky_ring_free(const RaceFreeOps *ops, uint32_t ring);

/* 005041d4..005041fc and 004bba5c: the track's open file and loads. */
typedef struct {
    uint32_t file;              /* 005041d4: closed through 0043dc38 */
    uint32_t cache;             /* 005041d8: the segment records' ring */
    int32_t unknown_5041ec;
    uint32_t unknown_5041f0;
    uint32_t terrain;           /* 005041f4: the chunks */
    uint32_t header;            /* 005041fc */
    int32_t unknown_4bba5c;
} RaceTrackFiles;

typedef struct {
    void *ctx;
    void (*close)(void *ctx, uint32_t file);   /* 0043dc38 (eax): 004884c8(file) */
    void (*free)(void *ctx, uint32_t block);   /* 00475260 */
} RaceTrackFreeOps;

/* 0043e8bc (watcall ()): the file closed, the cache freed, 005041ec and
 * 004bba5c 0, then 005041f0, the header and the terrain freed; none of the
 * pointers cleared. */
void race_track_free(const RaceTrackFreeOps *ops, RaceTrackFiles *t);

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

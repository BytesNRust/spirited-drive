/* After a race the cops ended: the ticket or a police movie (00428610). */
#ifndef TNFS_COP_STOP_H
#define TNFS_COP_STOP_H

#include <stdint.h>

typedef struct CopStopOps {
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);              /* 0047310c (cdecl) */
    void (*screen_show)(void *ctx, const char *file, int32_t mode, int32_t one);         /* 00466f68: eax, edx, ebx */
    void (*picture_free)(void *ctx, int32_t picture);                                    /* 004746a0 (cdecl) */
    int32_t (*random_pick)(void *ctx);                                                   /* 00466f34: 0..2 */
    void (*movie)(void *ctx, const char *file);                                          /* 004419c0: eax */
    void *ctx;
} CopStopOps;

typedef struct CopStopState {
    const int16_t *movie_on;      /* the short 0052b2fc (the game state's +0x5b8): the movie rather than the ticket */
    const int16_t *german;        /* the short 0052b31c */
    int32_t *screen;              /* 00524834: the screen's picture, a new 640 x 480 one for the ticket */
} CopStopState;

/* 00428610 (all registers kept): while 0052b2fc is 0, ticket.qfs on a new 640 x 480 screen picture (00466f68 mode
 * 0), the picture freed after (00524834 left pointing at it); else the movie cop1..3.tgv (gcop1..3.tgv in German),
 * 00466f34's pick plus 1. */
void cop_stop(const CopStopState *st, const CopStopOps *ops);

#endif

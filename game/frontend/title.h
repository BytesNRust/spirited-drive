/* The title screen. */
#ifndef TNFS_TITLE_H
#define TNFS_TITLE_H

#include "game/frontend/yes_no_box.h"

#include <stdint.h>

typedef struct TitleOps {
    void (*deadline_set)(void *ctx, int32_t time);                               /* 0047334f (cdecl) */
    void (*screen_show)(void *ctx, const char *file, int32_t mode, int32_t one);  /* 00466f68: eax, edx, ebx */
    void (*banner_wait)(void *ctx, const char *text, int32_t time);              /* 00467214 (message_banner_wait) */
    int32_t (*pointer_poll)(void *ctx);                                          /* [004c6194]: the buttons */
    void (*music_service)(void *ctx);                                            /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                       /* 0047336d */
    int32_t (*key)(void *ctx);                                                   /* 00464bd0 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);                   /* 00463ef0: eax, edx */
    void *ctx;
} TitleOps;

typedef struct TitleState {
    int music_off;     /* the short 0052b31e */
    int german;        /* the short 0052b31c */
    int link_game;     /* the short 0052b312: a two-machine game (meaning guessed) */
    int link_lost;     /* the short 0052b314: its connection lost (meaning guessed) */
} TitleState;

/* 004372e4: title.qfs (00466f68, mode 3 for state 3, else 1) for 500 ticks
 * or until a key or a button of the active pointer; with the music off and a
 * lost link the "Connection Terminated" banner first (and both link shorts
 * cleared); then, with the music on, a fade out (00463ef0 0, 0x100). */
void title_screen_show(int32_t state, TitleState *s, DialogPointer *p, const TitleOps *ops);

#endif

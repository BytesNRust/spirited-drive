/* The intro: a demo replay, the EA and attract movies, the title movie. */
#ifndef TNFS_INTRO_H
#define TNFS_INTRO_H

#include "game/frontend/palette_put.h"

#include <stdint.h>

enum { INTRO_PATH = 0x50, INTRO_REPLAY_NAME = 0x10 };

typedef struct IntroOps {
    void (*pointer_hide)(void *ctx);                                   /* [004c618c] */
    void (*drain)(void *ctx);                                          /* 004770ac */
    void (*window)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h);  /* 0047723c (cdecl) */
    int32_t (*file_exists)(void *ctx, const char *path);              /* 004886e0 (cdecl) */
    int32_t (*replay)(void *ctx, int32_t mode, const char *path);     /* 004275f8: eax 2, edx the path; a short */
    void (*video_reset)(void *ctx);                                    /* 004735bc */
    void (*palette_set)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*clear_both)(void *ctx);                                     /* 00464a30 (screen_clear_both) */
    void (*mode_set)(void *ctx, int32_t w, int32_t h, int32_t bits);  /* 00477980 (cdecl) */
    int32_t (*movie)(void *ctx, const char *path);                    /* 004423d4: eax the path; a short */
    int32_t (*random_pick)(void *ctx);                                 /* 00466f34 (random_pick_of_three) */
    int32_t (*screen_show)(void *ctx, const char *name, int32_t mode, int32_t music);  /* 00466f68 */
    void *ctx;
} IntroOps;

typedef struct IntroState {
    int intro_movies;       /* the short 0052b2fc: the movies, else eatitle.qfs */
    int16_t *replay_seen;   /* the short 004bc6a6: a replay shown already */
    int16_t *replay;        /* the short 004bc6a8: which (0..2) */
    int16_t *attract_seen;  /* the short 004bc6a4: an attract movie shown already */
    int16_t *attract;       /* the short 004bc6aa: which (0..2) */
    int16_t *replay_flag;   /* config.dat's short 00524e9f: 3 while the replay plays */
    char *replay_name;      /* config.dat's 00524e2b: "REPLAY%d.RPL" */
    const char *movie_dir;  /* 005243c8: paths.dat's slot 7 */
    char *movie_path;       /* 00509ecc */
} IntroState;

/* 00441abc: the screen black; with 0052b2fc the replay (after the first
 * time: REPLAY1..3.RPL in turn, when it exists, in a 0xc0 x 0x120 window at
 * 0x20, 0xc8), black at 320 x 200, then ea.tgv, attract1..3.tgv (the first
 * picked at random, then in turn) and title.tgv, each only while the ones
 * before ran to their end; else eatitle.qfs and, when it was not skipped,
 * the replay. Then black at 640 x 480. Returns 1. */
int intro_show(IntroState *s, const IntroOps *ops);

#endif

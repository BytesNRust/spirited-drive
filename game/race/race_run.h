/* The race (00455a5c, from the frontend): everything set up, the start,
 * the loop that runs the frames the timer says are due and draws, and
 * everything taken down. */
#ifndef TNFS_RACE_RUN_H
#define TNFS_RACE_RUN_H

#include "game/race/input_queue.h"
#include "game/race/race_state.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*service)(void *ctx);                                     /* 00475c14(0): moves the timer on */
    void (*screen_mode)(void *ctx, int32_t width, int32_t height, int32_t bits);  /* 00477980 */
    void (*load_pick)(void *ctx, int32_t first);                    /* 004571e8 */
    void (*setup_43cbe0)(void *ctx);                                /* 0043cbe0 */
    void (*force_43cbf4)(void *ctx);                                /* 0043cbf4 */
    void (*setup_443b64)(void *ctx);                                /* 00443b64 */
    void (*setup_443924)(void *ctx);                                /* 00443924 */
    void (*start_voices)(void *ctx);                                /* 00443820 */
    void (*engines_start)(void *ctx);                               /* 00444394 */
    void (*detail_setup)(void *ctx, int32_t high);                  /* 0044c9a4(high, 0, 0, 0, 0) */
    void (*track_info)(void *ctx);                                  /* 0043a370(004fb7c6, ...) */
    void (*setup_457594)(void *ctx);                                /* 00457594 */
    void (*setup_44edd8)(void *ctx);                                /* 0044edd8 */
    void (*cars_enter)(void *ctx);                                  /* 00440858 */
    void (*trail_bank)(void *ctx);                                  /* 00456a7c(005146ac) */
    void (*manager_start)(void *ctx);                               /* 0040fec8 */
    void (*net_send)(void *ctx, int32_t to);                        /* 00410b7c */
    int32_t (*net_wait)(void *ctx, int32_t kind);                   /* 00410c18 */
    void (*setup_44d2c4)(void *ctx);                                /* 0044d2c4 */
    void (*ai_setup)(void *ctx);                                    /* 004702d4 */
    void (*palette_cars)(void *ctx);                                /* 004572e0 */
    void (*setup_446a0c)(void *ctx);                                /* 00446a0c */
    void (*pointer_load)(void *ctx, int32_t variant);               /* 00441530 */
    void (*dash_colours)(void *ctx);                                /* 0041f988 */
    void (*dash_load)(void *ctx);                                   /* 00450838 */
    void (*setup_48f8e0)(void *ctx);                                /* 0048f8e0(0) */
    void (*setup_473578)(void *ctx);                                /* 00473578 */
    void (*palette_writes)(void *ctx);                              /* 00447048 */
    void (*cockpit_colours)(void *ctx);                             /* 0041f818 */
    void (*setup_4574ec)(void *ctx);                                /* 004574ec */
    void (*hud_colours)(void *ctx);                                 /* 00446bac */
    void (*setup_446aec)(void *ctx);                                /* 00446aec */
    void (*palette_banks)(void *ctx);                               /* 00447098 */
    void (*setup_446918)(void *ctx);                                /* 00446918 */
    void (*setup_4470dc)(void *ctx);                                /* 004470dc */
    void (*setup_446af0)(void *ctx);                                /* 00446af0 */
    void (*music_setup)(void *ctx);                                 /* 00456244 */
    void (*music)(void *ctx);                                       /* 004640d0 */
    void (*select_front)(void *ctx);                                /* 00473604 */
    void (*clip)(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom);  /* 0047383c */
    void (*queue_reset)(void *ctx, InputQueue *queue);              /* 00439430 */
    void (*queue_clear)(void *ctx, InputQueue *queue);              /* 00439478 */
    void (*show_loading)(void *ctx);                                /* 004737ec(00513970) */
    void (*video_flip)(void *ctx);                                  /* 004733ec */
    void (*wait_screen)(void *ctx);                                 /* 0041ec0c */
    void (*laps_start)(void *ctx, int32_t loop_nodes, int32_t laps);  /* 0043fbac */
    void (*tick)(void *ctx);                                        /* 00440ef0 */
    void (*setup_461f54)(void *ctx);                                /* 00461f54 */
    void (*setup_410250)(void *ctx);                                /* 00410250 */
    void (*frame)(void *ctx, int32_t frame);                        /* 0045597c (its low 16 bits) */
    void (*draw)(void *ctx);                                        /* 0044d26c */
    void (*end_43cbcc)(void *ctx);                                  /* 0043cbcc */
    void (*end_hook)(void *ctx);                                    /* [004c618c] */
    void (*end_446acc)(void *ctx);                                  /* 00446acc */
    void (*end_44d2e4)(void *ctx);                                  /* 0044d2e4 */
    void (*end_450a68)(void *ctx);                                  /* 00450a68 */
    void (*cockpit_teardown)(void *ctx);                            /* 00422c20 */
    void (*end_45106c)(void *ctx);                                  /* 0045106c */
    void (*end_40d6f4)(void *ctx);                                  /* 0040d6f4 */
    void (*end_44eef8)(void *ctx);                                  /* 0044eef8 */
    void (*end_440968)(void *ctx);                                  /* 00440968 */
    void (*trail_free)(void *ctx);                                  /* 00456c84(005146ac) */
    void (*results_45aa34)(void *ctx);                              /* 0045aa34 */
    void (*end_443be0)(void *ctx);                                  /* 00443be0 */
} RaceRunOps;

typedef struct {
    int32_t unknown_513e5c;     /* 00513e5c */
    int32_t done;               /* 004c5590: the frames run */
    int32_t due;                /* 004c5594: the frames the timer says are due */
    int32_t replay_due;         /* 004c5598: playback 1's due */
    int32_t running;            /* 00513e68 */
    int32_t net_failed;         /* 00513e58 */
    int16_t laps_low;           /* 00524e9f (a short) */
    int16_t music_on;           /* 00524e63 */
    int16_t music_started;      /* 004c5cca */
    int32_t back_buffer;        /* 004b95b4 */
    int32_t surface_width;      /* 004c78b4 */
    int32_t surface_height;     /* 004c78b8 */
    int32_t high;               /* 004fa0c0 */
    int32_t unknown_4fcdac;     /* 004fcdac */
    int32_t waiting;            /* 004bb8e8 */
    int32_t timer;              /* 004bb8d8: moved on by the services */
    int32_t unknown_4fcd90;     /* 004fcd90 */
    int32_t notice_on;          /* 004fb7a8 */
    int32_t tick_flags_upper;   /* 004fcdb0's upper three bytes: the start writes the dword -1 */
    InputQueue *queues;         /* 004faa7c: RACE_PLAYERS */
    int32_t track_header_4;     /* [005041fc] +4: a stage's node count << 16 */
    int32_t wait_ticks;         /* 004fbac8 */
    int32_t unknown_4c53a0;     /* 004c53a0: phase 2's frames between ticks */
    int32_t unknown_513e84;     /* 00513e84 */
    int32_t unknown_5120d4;     /* 005120d4 */
} RaceRunWords;

/* 00455a5c (watcall ()). See race_run.c for the steps. */
void race_run(const RaceRunOps *ops, RaceState *race, RaceRunWords *w);

#endif

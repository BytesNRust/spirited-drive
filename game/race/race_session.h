/* A race session (004275f8, from the frontend): the frontend's settings
 * taken into the race's words, the network's handshake and the setup's
 * exchange, a replay's setup read, the race (00455a5c), then the settings
 * and the results handed back. */
#ifndef TNFS_RACE_SESSION_H
#define TNFS_RACE_SESSION_H

#include <stdint.h>

enum {
    RACE_SESSION_RECORD = 0x4b,       /* the two records 0042702c fills and 004273d0 sends */
    RACE_SESSION_SETUP = 0x348,       /* 004fb7bc */
    RACE_SESSION_CARS = 0x98,         /* 004fb724: 8 x 12 bytes, then the option bytes 004fb784..004fb789 */
    RACE_SESSION_SETTINGS = 0x42,     /* 00524e5f..00524ea0: the frontend's words */
    RACE_SESSION_ROLLS = 6,           /* 004c6f90: rand() x rand() each */
    RACE_SESSION_SAVED = 5            /* 0052b264..0052b26d: the options handed back */
};

typedef struct {
    void *ctx;
    void (*from_menu)(void *ctx, uint8_t *first, uint8_t *second);       /* 0042702c */
    int32_t (*time)(void *ctx, int32_t *at);                             /* 00487bbf */
    void (*seed)(void *ctx, int32_t seed);                               /* 00487c1c */
    int32_t (*roll)(void *ctx);                                          /* 00487bf8 */
    void (*setup_4381ac)(void *ctx);                                     /* 004381ac */
    void (*window_size)(void *ctx, int32_t a, int32_t b, int32_t c, int32_t d);  /* 0047723c (cdecl) */
    void (*keys_setup)(void *ctx);                                       /* 00426ddc */
    void (*rrsm_alloc)(void *ctx);                                       /* 00451040 */
    void (*timer_start)(void *ctx);                                      /* 0043ba6c */
    void (*schedule_clear)(void *ctx);                                   /* 00410b5c */
    void (*net_deadline)(void *ctx, int32_t ticks);                      /* 00410b7c */
    int32_t (*net_wait)(void *ctx, int32_t kind);                        /* 00410c18 */
    int32_t (*ping_all)(void *ctx);                                      /* 00410d80 */
    void (*grid)(void *ctx);                                             /* 0042748c */
    void (*exchange)(void *ctx, uint8_t *first, uint8_t *second);        /* 004273d0 */
    void (*end_clear)(void *ctx);                                        /* 0044f528 */
    void (*replay_name)(void *ctx);                                      /* 00473d43(00513744, "%s%s", 00524418, 00524e2b) */
    void (*replay_request)(void *ctx, int32_t request);                  /* 0044f534 */
    void (*setup_read)(void *ctx);                                       /* 0044d3b0 */
    void (*random_seed)(void *ctx, int32_t seed);                        /* 00428fe0 */
    void (*models)(void *ctx);                                           /* 00457870 */
    void (*stem_set)(void *ctx, const uint8_t *name);                    /* 0043e9b4(004fb7c6) */
    void (*timer_level)(void *ctx);                                      /* 0043bb44 */
    void (*stand_in_start)(void *ctx);                                   /* 00410250 */
    void (*service)(void *ctx);                                          /* 00475c14(0) */
    void (*run)(void *ctx);                                              /* 00455a5c */
    void (*timer_stop)(void *ctx);                                       /* 0043bbec */
    void (*window)(void *ctx, int32_t *a, int32_t *b);                   /* 004054cc */
    void (*results_end)(void *ctx);                                      /* 0045af80 */
    void (*menu_4644f0)(void *ctx);                                      /* 004644f0, the frontend's */
    void (*menu_464568)(void *ctx);                                      /* 00464568, the frontend's */
    void (*flush)(void *ctx);                                            /* 00487d38 */
} RaceSessionOps;

/* Read again after every call (the ctx may change them) */
typedef struct {
    uint8_t *setup;                   /* 004fb7bc: RACE_SESSION_SETUP bytes */
    uint8_t *cars;                    /* 004fb724: RACE_SESSION_CARS bytes */
    uint8_t *settings;                /* 00524e5f: RACE_SESSION_SETTINGS bytes */
    int32_t rolls[RACE_SESSION_ROLLS];  /* 004c6f90 */
    int32_t unknown_4b95b4;
    int32_t unknown_4b95c0;
    int32_t high;                     /* 004fa0c0: 1 with the settings' word 00524e6f 0 */
    int32_t unknown_4fa0c4, unknown_4fa0c8, unknown_4fa0cc;
    int32_t unknown_4fa0ec;
    int32_t latency;                  /* 004fa0f0: 00410d80's */
    int32_t once;                     /* 00513e58 */
    int32_t followed;                 /* 004fbb08 */
    int32_t viewed;                   /* 004fbb04: read as 004fbae0[9] (track 9) */
    int32_t state;                    /* 00509d6c */
    int32_t unknown_509d70;
    int16_t play;                     /* 0052b312 */
    int32_t sound;                    /* 0052b30c (its high word read) */
    int16_t saved[RACE_SESSION_SAVED];  /* 0052b264.. */
    int16_t unknown_52b2fe;
} RaceSession;

/* 004275f8 (eax argc, edx argv; returns eax): the session. 0042702c fills
 * two records; the words reset; 004fa0c0 1 with 00524e6f 0 (00524e6f
 * non-zero: 004b95b4 1); the option bytes 004fb784.. from the settings'
 * words; srand(time(0)), the six rolls; 004381ac, 0047723c (high: 0x40,
 * 0x1cc, 0x240, 0x1e0, else 0x20, 0xb4, 0x120, 0xc8), 00426ddc; the
 * followed player's 12 car bytes (004fb724 + 12 x 004fbb08) into the first
 * record at +0x33. Not a
 * replay (00524e9f 0) with argc above 1: argv[1] to the setup's name
 * (004fb7c6). 00451040, 0043ba6c; 00513e58 = 0; a network race (004fb7d4 1,
 * not a replay): 00410b5c, 00410b7c(-1), 00410c18(2): 0 gives 00513e58 =
 * 1, else 004fa0f0 = 00410d80. Variant 00524e83 0: 004fba70's nine dwords
 * = 99. 0042748c, time(004fb7f4); not a replay: 004273d0 with the records.
 * 0044f528; a replay: the file's name, 0044f534(4 / 5 / 8 for kinds 1 / 2
 * / else) and 0044d3b0; else 0044f534(0). The followed player's entry
 * (+0x1d / +0x21 / +0x29 the sound, music and sound-open words, 0 without
 * the sound; +0x25 00524e61). 004fa0ec: 1 with 004fbadc or 004fbad8, else
 * 004fbae0[track]. 00428fe0(004fb7f4), 00457870; each human's 12 bytes
 * (entry +0x31) to 004fb724 + 12 i. 004fbadc with track index 3 (00524e85
 * and 004fb7c2): the name "tr5". 0043e9b4; 004fb7d8 bit 3: 004fbabc = 3,
 * 0043bb44. 00410250, 00475c14, 00455a5c, 00475c14, 0043bbec. Not high:
 * 004fb789 at most 1. 004054cc; the options back to 0052b264.. and
 * 00524e71..; a network race not played back: 00509d6c = 2; 0052b2fe =
 * 00509d6c; 0045af80; 00524e63 set: 004644f0, 00464568; 00487d38. Returns
 * 1 for replay kind 3 ending with 00509d70 0x40. Port guards: the name
 * within the setup, the followed car, entry and the humans' within theirs. */
int32_t race_session(const RaceSessionOps *ops, RaceSession *s, int32_t argc, const char *const *argv);

#endif

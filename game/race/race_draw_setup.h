/* The race's drawing set up (0044ca3c, from 0044d2c4 in 00455a5c's setup):
 * the frame's restart, the surfaces, the track and its textures, the
 * camera on the followed car, the draw distance asked for by the detail. */
#ifndef TNFS_RACE_DRAW_SETUP_H
#define TNFS_RACE_DRAW_SETUP_H

#include <stdint.h>

enum {
    DRAW_SETUP_MIRROR_COUNT = 4,    /* 00504224: the mirror's segments */
    DRAW_SETUP_PALETTE = 0x300      /* the stack buffer 00463df0 fills: 256 colours */
};

/* 0044ca3c's callees. */
typedef struct {
    void *ctx;
    void (*palette_get)(void *ctx, uint8_t rgb[DRAW_SETUP_PALETTE]);  /* 00463df0 */
    void (*front)(void *ctx);                                       /* 004735bc */
    void (*palette_set)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 */
    void (*clear)(void *ctx, int32_t colour);                       /* 0048f8e0 */
    void (*screen_mode)(void *ctx, int32_t width, int32_t height, int32_t bits);  /* 00477980 */
    int32_t (*surface)(void *ctx, int32_t width, int32_t height, int32_t flags);  /* 0047310c: a record */
    void (*track_load)(void *ctx);                                  /* 0043e4e0(004fd0f4) */
    void (*textures)(void *ctx);                                    /* 0044c36c */
    void (*follow)(void *ctx, int32_t kind);                        /* 00404530(004f801c, cars[004fbb08], kind) */
    void (*views)(void *ctx);                                       /* 00405630 */
    void (*setup_404404)(void *ctx);                                /* 00404404 */
    void (*setup_44c140)(void *ctx, int32_t v);                     /* 0044c140 */
    void (*setup_44c148)(void *ctx, int32_t v);                     /* 0044c148 */
    void (*setup_449be4)(void *ctx);                                /* 00449be4 */
    void (*setup_449430)(void *ctx);                                /* 00449430 */
    void (*screen)(void *ctx, int32_t size, int32_t kind);          /* 0040540c(004fb784, 004fb789) */
} RaceDrawSetupOps;

typedef struct {
    int32_t restart;            /* 00512008 */
    int32_t unknown_512010;     /* 00512010 */
    int32_t mirror_state;       /* 00511ff4 */
    int32_t use_parts;          /* 004f9b3c */
    int32_t high;               /* 004fa0c0 (read) */
    int32_t width, height;      /* 0051201c, 00512020 (read) */
    int32_t view_surface;       /* 004c2d90 */
    int32_t unknown_4f9b64;     /* 004f9b64 */
    int32_t back_surface;       /* 004c2d98 */
    int32_t map_surface;        /* 004c2d9c */
    int32_t back_buffer;        /* 004b95b4 (read) */
    int32_t screen_surface;     /* 004c2da0: made only with a back buffer */
    int32_t replay_kind;        /* 00512000 (read): 1..13 (or negative) keeps the chase's kind */
    int32_t chase_kind;         /* 004f801c (read) */
    int32_t next_kind;          /* 004f8020 */
    int32_t replay_next;        /* 00512004 (read) */
    int32_t mirror_count;       /* 00504224 */
    int32_t unknown_511fb4;     /* 00511fb4 */
    int32_t unknown_4c2d88;     /* 004c2d88 */
    uint8_t detail;             /* 004fb785 (read) */
    int32_t pending_segments;   /* 004c5360 */
    uint8_t window_size;        /* 004fb784 (read) */
    uint8_t screen_kind;        /* 004fb789 (read) */
    uint8_t clear_view;         /* 004fb78a */
    int32_t unknown_511fc4;     /* 00511fc4 */
    int32_t unknown_511fc0;     /* 00511fc0 */
} RaceDrawSetup;

/* 0044ca3c (watcall ()): 00512008 = 1, 00512010 = 00511ff4 = 0, 00511fe0 =
 * 004f801c (the port's chase is fixed), 004f9b3c = 1. Low resolution: twice
 * front, the palette (00463df0) written back whole, clear(0), with the
 * screen mode (width, height, 8) between. The surfaces: the view (width x
 * height), 004f9b64 = 0, the back 0x71 x 0x8c / 0x39 x 0x34 and the map
 * 0x74 x 0x8e / 0x3a x 0x36 high / low, the screen with a back buffer.
 * track_load, textures; follow with the chase's kind when 00512000 is not 0
 * and below 14 (signed), 004f8020 = 00512004 after it; else follow(8).
 * views, 00504224 = 4, 00404404, 0044c140(0), 0044c148(0), 00511fb4 = 1,
 * 00449be4, 00449430, 004c2d88 = 1; the draw distance asked for 0x1c,
 * 0x12, 0xd for detail 0, 1, 2 (else left); screen kind 3 sets clear_view;
 * screen(window size, screen kind). */
void race_draw_setup(const RaceDrawSetupOps *ops, RaceDrawSetup *s);

/* port: the draw distance a detail (0, 1, 2) asks for, as the race's start does; -1 for any other detail */
int32_t race_draw_detail_segments(int detail);

/* 0044d2c4 (watcall (), from 00455a5c): 00511fc4 = 1, 00511fc0 = 0,
 * 0044ca3c; returns 1. */
int32_t race_draw_start(const RaceDrawSetupOps *ops, RaceDrawSetup *s);

#endif

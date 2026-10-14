/* The race's frame draw (0044cc74, from 0044d26c every frame): the camera
 * placed, the view (re)chosen, the 3D view, the mirror, the scenery, the
 * cockpit or the HUD, the page shown, and the frame rate counted. */
#ifndef TNFS_FRAME_DRAW_H
#define TNFS_FRAME_DRAW_H

#include "game/race/car.h"
#include "game/race/chase_struct.h"
#include "game/race/hud_mask.h"
#include "game/race/race_detail.h"
#include "game/race/race_view.h"

#include <stdint.h>

enum {
    FRAME_TEXT = 0xa0,          /* 0050ed84 and 0050ee24: 0xa0 bytes apart */
    FRAME_MONO_WORDS = 2000     /* 000b0000: the monochrome text screen, 80 x 25 */
};

typedef struct {
    void *ctx;
    void (*chase_tick)(void *ctx);                          /* 004046b0(00511fe0) */
    void (*mirror_create)(void *ctx);                       /* 0044bd64 */
    void (*net_send)(void *ctx, int32_t to);                /* 00410b7c */
    int32_t (*net_wait)(void *ctx, int32_t kind);           /* 00410c18 */
    void (*restream)(void *ctx);                            /* 0043f51c */
    void (*stream)(void *ctx);                              /* 0043f540(00511fe0) */
    void (*view)(void *ctx, int rear);                      /* 0040586c(00511fe0, rear) */
    void (*service)(void *ctx);                             /* 00475c14(0) */
    void (*select)(void *ctx, HudSurface s);                /* 00473680 */
    void (*view_choose)(void *ctx, int32_t view, int32_t size);  /* 00405568 */
    void (*tiles)(void *ctx);                               /* 004218d0 */
    void (*matrix)(void *ctx);                              /* 004496ac */
    void (*transform)(void *ctx);                           /* 0044aa94 */
    void (*rows)(void *ctx, int32_t node);                  /* 00459c50 */
    void (*ahead)(void *ctx, int32_t node);                 /* 00459d70 */
    void (*mirror_transform)(void *ctx);                    /* 0044b970 */
    void (*mirror_select)(void *ctx);                       /* 00473680(004c2d94) */
    void (*mirror_sky)(void *ctx);                          /* 0044bddc */
    void (*mirror_scene)(void *ctx, int32_t count);         /* 0044baa4 */
    void (*mirror_flip)(void *ctx);                         /* 0048f7a0(004c2d94 +0x2c) */
    void (*objects_from_last)(void *ctx);                   /* 0044a804 */
    void (*clip)(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom);  /* 0047383c */
    void (*horizon)(void *ctx);                             /* 0043af80(00511fe0) */
    void (*fill)(void *ctx, int32_t colour);                /* 00482980 */
    void (*near)(void *ctx, int32_t segments, int32_t node);  /* 0044b510 */
    void (*map)(void *ctx);                                 /* 00422ce0 */
    void (*select_front)(void *ctx);                        /* 00473604: 00473680(0052d2f4) */
    void (*text_colour)(void *ctx, int32_t colour, int32_t back);  /* 0048f370 */
    /* 0048f940(x, y, "sub:%4d cam:%c" (004b0690), node, letter) */
    void (*text)(void *ctx, int32_t x, int32_t y, int32_t node, int32_t letter);
    void (*hud_clear)(void *ctx);                           /* 0041e364 */
    void (*cockpit)(void *ctx, Car *car);                   /* 0042270c */
    void (*line)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t colour);  /* 00484254 */
    void (*hud)(void *ctx, Car *car, int32_t changed);      /* 00422598 */
    void (*show)(void *ctx, HudSurface s);                  /* 004737ec(the surface's +0x2c shape) */
    void (*video_flip)(void *ctx);                          /* 004733ec */
    void (*present)(void *ctx);                             /* 00473614 */
    int32_t (*time)(void *ctx);                             /* 00473334: the time since its last call */
    /* 00473d43(0050ed84, "%2ld.%1ld" (004b06a0), whole, tenth) */
    void (*format_rate)(void *ctx, char *text, int32_t whole, int32_t tenth);
    void (*music)(void *ctx);                               /* 004640d0 */
} FrameDrawOps;

typedef struct {
    const RaceChase *chase;     /* 00511fe0 */
    const RaceCamera *camera;   /* 00511fe8: the side view() last set */
    Car *const *cars;           /* 00509e1c: by slot */
    int32_t followed;           /* 004fbb08: the cockpit's car */
    int32_t viewed;             /* 004fbb04: the HUD's car */
    int32_t unknown_513e60;     /* 00513e60 */
    int32_t restart;            /* 00512008: the first frame of a race */
    int32_t changed;            /* 00511fec: the camera took a view (999 after F9) */
    int32_t view_pending;       /* 00512050 */
    int32_t window_size;        /* 00511ff0 */
    int32_t mirror_state;       /* 00511ff4 */
    int32_t unknown_4b91c8;     /* 004b91c8: the mirror is drawn below 2 */
    uint8_t alternate;          /* 004fb787: the mirror drawn every other frame */
    int32_t mirror_count;       /* 00504224 */
    int32_t net_start;          /* 004fb7d4 */
    int16_t laps_low;           /* 00524e9f (a short) */
    int32_t unknown_4fcdac;     /* 004fcdac */
    int32_t waiting;            /* 004bb8e8 */
    int16_t dropped;            /* 0052b314 */
    int32_t draw_segments;      /* 00504220 */
    int32_t pending_segments;   /* 004c5360: -1 none */
    int32_t auto_detail;        /* 005104cc */
    RaceFrameRate rate;         /* 004c5364, 004c5368, 005104c4 */
    int32_t rate_target;        /* 005104c8 */
    int32_t unknown_511fc4;     /* 00511fc4 */
    int32_t unknown_511fbc;     /* 00511fbc: the view filled with 0xff first */
    int32_t unknown_4c2d8c;     /* 004c2d8c */
    int32_t mono;               /* 004b95bc: the texts on the monochrome screen too */
    uint16_t *mono_screen;      /* 000b0000 (port: NULL, none) */
    char rate_text[FRAME_TEXT]; /* 0050ed84 */
    char text_2[FRAME_TEXT];    /* 0050ee24 */
    int32_t debug_text;         /* 004c2d80, read as a dword */
    int32_t text_colour;        /* 0050b7a8 */
    int32_t width, height;      /* 0051201c, 00512020 */
    uint8_t screen_kind;        /* 004fb789 */
    uint8_t clear_view;         /* 004fb78a */
    int32_t unknown_4f9b7c;     /* 004f9b7c */
    int32_t in_car;             /* 00512054: the last frame was the cockpit's */
    int32_t line_colour;        /* 0050b7b4 */
    int32_t back_buffer;        /* 004b95b4 */
    int32_t unknown_51205c;     /* 0051205c */
    int32_t unknown_512010;     /* 00512010 */
    int32_t last_time;          /* 00512060 */
    int32_t rate_elapsed;       /* 00512064: since the rate text */
    int32_t run_time;           /* 00512068 */
    int32_t rate_frames;        /* 0051206c */
    int32_t run_frames;         /* 00512070 */
    int32_t per_minute;         /* 00512058: frames x 6000 / elapsed */
    int16_t music_on;           /* 00524e63 */
} FrameDraw;

/* 0044cc74 (watcall ()). See frame_draw.c for the steps. 0043f624 (dead in
 * the shipped game) and 004733b0 (a bare ret) are left out. */
void frame_draw(const FrameDrawOps *ops, FrameDraw *d);

#endif

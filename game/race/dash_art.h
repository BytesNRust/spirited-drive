/* The race's on-screen art from the mask bank (00450838 loads masklo /
 * maskhi.fsh into 005137f0): the dashboard overlay and the replay panel,
 * each a shape found by name. */
#ifndef TNFS_DASH_ART_H
#define TNFS_DASH_ART_H

#include <stdint.h>

enum {
    REPLAY_BUTTONS = 13,    /* index 0 unused, 1..12 */
    REPLAY_VIEWS = 6,       /* index 0 unused, 1..5 */
    REPLAY_GLYPHS = 36,
    DASH_ART_MISSING = -1   /* port guard: a name the bank lacks (the original aborts) */
};

/* A shape's screen box: its place (+0xc, +0xe) and place + size (+4, +6). */
typedef struct {
    int32_t left, top, right, bottom;
} DashRect;

/* 0052b69c..0052b8df: the replay panel's shapes (00471180). */
typedef struct {
    uint8_t *replay;                    /* 0052b69c 'repl' */
    uint8_t *bar;                       /* 0052b6a0 'bard' */
    uint8_t *names;                     /* 0052b6a4 'namd' */
    uint8_t *button[REPLAY_BUTTONS];    /* 0052b6a8 004c5e34's: 'dond', '0<d '.. 'camd' */
    uint8_t *button_up[REPLAY_BUTTONS]; /* 0052b6dc 004c5e9c's: 'donl', '0<lu'.. 'caml' */
    uint8_t *button_down[REPLAY_BUTTONS]; /* 0052b710 004c5e68's: 'doni', '0<ld'.. 'cami' */
    uint8_t *view[REPLAY_VIEWS];        /* 0052b744 004c5ed0's: 'bumd'.. 'chad' */
    uint8_t *view_lit[REPLAY_VIEWS];    /* 0052b75c 004c5ee8's: 'buml'.. 'chal' */
    uint8_t *glyph[REPLAY_GLYPHS];      /* 0052b774 004c5f00's: '00d ', '1d  '.. '0z  ' */
    DashRect box[REPLAY_BUTTONS];       /* 0052b804: button_down's boxes */
    int32_t ready;                      /* 0052b8dc */
} ReplayArt;

/* 00471180: finds the replay panel's shapes in the mask bank; outside a
 * replay (replay 0, 0052b316) button 1 is 'donm'. Returns 0, or
 * DASH_ART_MISSING when a name is not in the bank, with the shapes found
 * before it set. */
int dash_replay_art(ReplayArt *art, uint8_t *bank, int16_t replay);

#endif

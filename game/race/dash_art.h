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
    DASH_CONTROLS = 9,      /* index 0 unused, 1..8 */
    DASH_VIEWS = 8,
    DASH_SPEEDS = 3,
    DASH_DIGITS = 11,       /* 0..9 and a blank */
    DASH_YELLOW = 15,       /* 0..9, 'yelr', 'yeln', 'plus', 'mnus', 'yelb' */
    DASH_NEEDLE_LOW = 30,   /* 004fa0c0 == 0: 30 needle points, 0x20 apart */
    DASH_NEEDLE_HIGH = 40,  /* 40, 0x13 apart */
    DASH_PART_ROWS = 3, DASH_PART_COLUMNS = 4,  /* maskhi.b00 .. maskhi.b23 */
    DASH_STATE_WORDS = 7,
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
    int32_t camera;                     /* 0052b8dc: the panel's camera button, 1..5 (0044d640) */
} ReplayArt;

/* 005137f0..00513d97: the dash's shapes and the tachometer's needle
 * points (004502b0; 00450838 sets the bank, the palette and 00513888..). */
typedef struct {
    uint8_t *bank;                          /* 005137f0: the mask bank */
    const uint8_t *palette;                 /* 005137f4: its '!pal' colours (6 bits) */
    DashRect control_box[DASH_CONTROLS];    /* 005137f8 + 16 i: control_down's boxes */
    int32_t unknown_513888[DASH_STATE_WORDS]; /* 00513888..005138a3: 0, 0, 1, 0, 0, 1, 0 at load */
    uint8_t *grey_shown[DASH_DIGITS];       /* 005138a4: grey, copied */
    uint8_t *yellow[DASH_YELLOW];           /* 005138d4 004c54cc's */
    uint8_t *red[DASH_DIGITS];              /* 00513914 004c5508's: 'red0'.. 'redb' */
    uint8_t *tach;                          /* 00513944 */
    uint8_t *wait;                          /* 00513948 */
    uint8_t *lost;                          /* 0051394c */
    uint8_t *paused;                        /* 00513950 'paus' */
    uint8_t *srdd, *srdl, *syld, *syll;     /* 00513954..00513960 */
    uint8_t *green;                         /* 00513964 'sgrn' */
    uint8_t *acct;                          /* 0051396c */
    uint8_t *sync;                          /* 00513970 */
    int32_t centre_x, centre_y;             /* 00513974, 00513978: the tach's middle */
    int32_t radius_x, radius_y;             /* 0051397c, 00513980 */
    int32_t needle[DASH_NEEDLE_HIGH][2];    /* 00513984: x, y round the tach */
    int32_t needle_mid[DASH_NEEDLE_HIGH][2]; /* 00513acc: halfway to the middle */
    int32_t mmb_words[DASH_PART_ROWS][DASH_PART_COLUMNS]; /* 00513c14: the .mmb file's words 2..13 */
    uint8_t *parts[DASH_PART_ROWS][DASH_PART_COLUMNS];    /* 00513c44: the .b%d%d files (the caller loads them) */
    uint8_t *mmb;                           /* 00513c74: the .mmb file */
    uint8_t *replay;                        /* 00513c78 'repl' */
    uint8_t *bar;                           /* 00513c7c 'bar1' */
    uint8_t *control[DASH_CONTROLS];        /* 00513c80 004c53dc's: 'dond' 'rtsd' 'rewd' 'ford' 'fted' 'plad' 'paud' 'camd' */
    uint8_t *control_lit[DASH_CONTROLS];    /* 00513ca4 004c5424's: the '..l' ones */
    uint8_t *control_down[DASH_CONTROLS];   /* 00513cc8 004c5400's: the '..i' ones */
    uint8_t *view[DASH_VIEWS];              /* 00513cec 004c5448's: 'card' 'bumd' 'taid' 'held' 'skyd' 'chad' 'oppd' 'copd' */
    uint8_t *view_lit[DASH_VIEWS];          /* 00513d0c 004c5468's */
    uint8_t *speed[DASH_SPEEDS];            /* 00513d2c 'fuld' '1/2d' '1/4d' */
    uint8_t *speed_lit[DASH_SPEEDS];        /* 00513d4c 'full' '1/2l' '1/4l' */
    uint8_t *grey[DASH_DIGITS];             /* 00513d6c 'gry0'.. 'gryb' */
} DashArt;

/* 004502b0: finds the dash's shapes in art->bank, the tach's middle and
 * radii (less a sixteenth at low resolution, 004fa0c0 == 0) and the needle
 * points from angle 0x1a9 on, then 00471180. Returns 0 or DASH_ART_MISSING. */
int dash_art_find(DashArt *art, ReplayArt *replay_art, int32_t high, int16_t replay);

/* 00471180: finds the replay panel's shapes in the mask bank; outside a
 * replay (replay 0, 0052b316) button 1 is 'donm'. Returns 0, or
 * DASH_ART_MISSING when a name is not in the bank, with the shapes found
 * before it set. */
int dash_replay_art(ReplayArt *art, uint8_t *bank, int16_t replay);

#endif

/* The race's screen size and the words beside it (0044c9a4). */
#ifndef TNFS_RACE_SCREEN_H
#define TNFS_RACE_SCREEN_H

#include <stdint.h>

typedef struct {
    int32_t unknown_511ffc;     /* 00511ffc */
    int32_t unknown_512000;     /* 00512000 */
    int32_t next_kind;          /* 00512004: the record's next kind (004f8020) */
    int32_t unknown_51200c;     /* 0051200c */
    int32_t unknown_512014;     /* 00512014 */
    int32_t mode;               /* 00512018: 1 for 640 x 480 */
    int32_t width, height;      /* 0051201c, 00512020 */
    int32_t clip_right, clip_bottom;  /* 00512024, 00512028 */
    int32_t clip_left, clip_top;      /* 0051202c, 00512030 */
} RaceScreen;

/* 0044c9a4 (watcall mode, a, b, c, then d on the stack; ret 4): the mode;
 * 640 x 480 for a mode other than 0, else 320 x 200, the clip the whole
 * screen; 0051200c a, 00511ffc b, 00512000 c, 00512014 d, 00512004 the
 * record's next kind (004f8020). */
void race_screen_setup(RaceScreen *s, int32_t mode, int32_t a, int32_t b, int32_t c, int32_t d, int32_t next_kind);

#endif

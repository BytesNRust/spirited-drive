/* The checked menus run over game/sys and ea_lib (glue, no oracle): each takes the build's state and gives what
 * its original gives. */
#ifndef TNFS_MENU_GLUE_H
#define TNFS_MENU_GLUE_H

#include <stdint.h>

/* the words the menus share (the original's globals) */
typedef struct MenuWords {
    int16_t link_game;       /* 0052b312 */
    int16_t link_lost;       /* 0052b314 */
    int16_t mode_menu;       /* 00524e7b */
    int16_t redraw;          /* 0052b31a */
    int32_t intro_seen;      /* 004fbb0c */
    int32_t single;          /* 004fbb08 */
    int32_t network_raced;   /* 004fb7d4 */
} MenuWords;

extern MenuWords menu_words;

/* the cheats' words (00432ff4 sets them, 004667fc clears them) */
typedef struct MenuCheats {
    int32_t keys_506;  /* 0052b306 */
    int32_t keys_50a;  /* 0052b30a */
} MenuCheats;

extern MenuCheats menu_cheats;

void glue_title_screen(int32_t state);  /* 004372e4 */
int32_t glue_game_mode(void);           /* 004376ac: 0 race, 1 leave, 2 the intro again */
int32_t glue_central(void);             /* 00432ff4 (central_glue.c): 2 the timer, else 0 */

/* the boxes as ops (ctx unused) for the other screens' glue */
int32_t glue_yes_no(void *ctx, const char *text);  /* 00465f4c */
void glue_banner(void *ctx, const char *text);     /* 0046667c */
void glue_drain(void *ctx);                        /* 004770ac */

#endif

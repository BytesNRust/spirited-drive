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
void glue_game_reset(void);            /* 00466ae4 (central_glue.c) */
int16_t glue_movies(void);             /* the short 0052b2fc, the game state's +0x5b8 (central_glue.c) */
int32_t glue_check_options(int32_t mode);  /* 00403760 (central_glue.c): the options after a race, eax 0 or 1 */
void glue_drive_race(int32_t track, int32_t segment, int32_t car, int32_t race_type, int32_t variant_m,
                     int32_t variant_r);  /* --drive (central_glue.c) */
void glue_race_option(uint32_t address);  /* the race menu's 004015dc (audio) or 00423d28 (graphics) on config.dat's image */
void glue_settings_to_config(void);    /* a window closed: Control Central's race settings into config.dat's image */

/* the boxes as ops (ctx unused) for the other screens' glue */
int32_t glue_yes_no(void *ctx, const char *text);  /* 00465f4c */
void glue_banner(void *ctx, const char *text);     /* 0046667c */
void glue_drain(void *ctx);                        /* 004770ac */
void glue_boxed(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463934 */
void glue_banner_wait(void *ctx, const char *text, int32_t time);   /* 00467214 */
int32_t glue_network_found(void *ctx);  /* 00442514 not NULL: a network service */
int32_t glue_network_game(void *ctx);   /* 00454b78: 0 to race, 1 back, 2 the time ran out */
void glue_save_box(char *name, uint8_t *config, uint8_t *game, int16_t lost_vegas);  /* 0042a680 */
void glue_replay_box(char *name, uint8_t *config);                  /* 0042a9f8 */

/* 00408724's words, Control Central's aligned copies of config.dat's shorts, and its 00465ec8 */
typedef struct ShowcaseGlue {
    int16_t *player_car;
    const int16_t *race_type, *track, *pointer_active;
    int16_t *buttons;
    void (*car_flags)(void *ctx);
    void *ctx;
} ShowcaseGlue;
void glue_showcase(const ShowcaseGlue *in);

#endif

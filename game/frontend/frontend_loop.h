/* The front end's loop (004373c4): title, menus, races, until the player quits. */
#ifndef TNFS_FRONTEND_LOOP_H
#define TNFS_FRONTEND_LOOP_H

#include <stdint.h>

typedef struct FrontendLoopOps {
    int32_t (*config_load)(void *ctx);          /* 0042a010 (game_file_load_config): 1 when the defaults were taken */
    void (*title)(void *ctx, int32_t state);    /* 004372e4 (title_screen_show): eax the state */
    int32_t (*network_found)(void *ctx);        /* 00442514: 0 when there is no network service */
    void (*fatal)(void *ctx, const char *message);  /* 00477a40 (cdecl) */
    int16_t (*name_entry)(void *ctx);           /* 004667fc(00524df0, 0, 004afac4, 00524df0): 0 when given up */
    int16_t (*ask)(void *ctx, const char *text);  /* 00465f4c (yes_no_box): eax the question */
    int32_t (*network_game)(void *ctx);         /* 00454b78: 0 to race, 1 to leave */
    void (*game_reset)(void *ctx);              /* 00466ae4 */
    int32_t (*control_central)(void *ctx);      /* 00432ff4: what the loop goes on with */
    void (*config_save)(void *ctx);             /* 0042a504 (game_file_save_config) */
    void (*link_close)(void *ctx);              /* 00446220: with 0052b312 set */
    void (*music_fade_out)(void *ctx);          /* 004644f0 */
    void (*music_end)(void *ctx);               /* 00464568 */
    void (*intro)(void *ctx);                   /* 00441abc */
    void (*after_intro)(void *ctx);             /* 0045638c */
    void (*music_feed)(void *ctx);              /* 004640d0 */
    int32_t (*main_menu)(void *ctx);            /* 004376ac: 0 race, 1 quit, 2 the intro again, 3 .. */
    void *ctx;
} FrontendLoopOps;

typedef struct FrontendLoopState {
    int network;          /* the short 0052b31e: the network branch (also no music, menu_sound.h music_off) */
    int german;           /* the short 0052b31c */
    const int16_t *link;  /* the short 0052b312: read after each race */
    int32_t *network_raced;  /* 004fb7d4: set to 1 when 00454b78 sends to a race */
} FrontendLoopState;

/* 004373c4: with 0052b31e the network loop: config.dat, the title (state 3
 * when the defaults were taken, else 2), "No network services found." as a
 * fatal error when 00442514 finds none, the player's name asked until given
 * (or "Exit to system ?" answered yes, which ends), then 00454b78 / Control
 * Central until 00454b78 gives 1. Else the intro (once, with the music
 * faded out and ended first), config.dat (once), 0045638c and 004640d0;
 * the title for states 2 and 3; the main menu (2 asks for the intro again,
 * 0 races through Control Central and saves config.dat); 00446220 when
 * 0052b312 is set; until the state is 1. */
void frontend_loop(const FrontendLoopState *s, const FrontendLoopOps *ops);

#endif

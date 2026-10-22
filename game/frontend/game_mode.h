/* The "Game Mode" menu: single player, modem, network, exit (004376ac). */
#ifndef TNFS_GAME_MODE_H
#define TNFS_GAME_MODE_H

#include <stdint.h>

/* the palette's bytes (004376ac's 0x300 on its stack) */
enum { GAME_MODE_PALETTE = 0x300 };

typedef struct GameModeOps {
    void (*palette_clear)(void *ctx, uint8_t *rgb);                          /* 00463df0: eax */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    void (*sound_load)(void *ctx);                                           /* 004648dc */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*unlock)(void *ctx);                                               /* 004733b0 */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*inverse_build)(void *ctx);                                        /* 0047463c */
    void (*banner_wait)(void *ctx, const char *text, int32_t time);          /* 00467214 */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    int32_t (*name_box)(void *ctx, char *name);                              /* 004667fc: eax */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    int32_t (*modem)(void *ctx);                                             /* 00453000 */
    int32_t (*network_found)(void *ctx);                                     /* 00442514 */
    int32_t (*network_game)(void *ctx);                                      /* 00454b78 */
    int32_t (*yes_no)(void *ctx, const char *text);                          /* 00465f4c: eax */
    void (*drain)(void *ctx);                                                /* 004770ac */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void (*sound_free)(void *ctx);                                           /* 004649c8 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void *ctx;
    /* port, after ctx so the original's initializers leave them NULL: the Modem item opens the EXTRAS screen
     * (without the name box) and is labelled "Customize game" over its art. NULL: the original's Modem. */
    void (*extras)(void *ctx);
    void (*plus_text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);
    int32_t (*plus_text_width)(void *ctx, const char *text);
    /* port: the picture's own background put back over the w x h at (x, y) (the Modem item's words); NULL leaves
     * the label over them */
    void (*extras_cover)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h);
} GameModeOps;

typedef struct GameModeState {
    int32_t screen;          /* 00524834: the screen's picture */
    int16_t pointer_active;  /* the short 00524790 */
    int16_t buttons;         /* the short 00524792 */
    int16_t mode_menu;       /* the short 00524e7b: set to 1 */
    int32_t intro_seen;      /* 004fbb0c: set to 1 */
    int32_t single;          /* 004fbb08: cleared for a single player race */
    int32_t network_raced;   /* 004fb7d4: 0 single, 1 after the modem or the network */
    int16_t lost;            /* the short 0052b312: the connection lost ... */
    int16_t lost_shown;      /* the short 0052b314: ... and not yet told (both cleared once told) */
    int16_t redraw;          /* the short 0052b31a: set by others to have the menu drawn again (cleared) */
    char *name;              /* 00524df0: the player's name, 004667fc's */
} GameModeState;

/* 004376ac (all registers kept; the language the short 0052b31c): the menu
 * of control/gamemode.qfs over four bands of the pointer (y 0..0xb1,
 * 0xb2..0xfd, 0xfe..0x149, 0x14a..0x1e0), Up / Down moving the pointer to
 * the next option's centre (0x140; 0x91, 0xda, 0x122, 0x16f), Enter, Space
 * or the left button choosing once the keys rest (no key, or 0x19 ticks of
 * one other than those); Escape or the right button ask "Exit to system ?".
 * Gives 0 to race, 1 to leave, 2 when the 0x1194 ticks run out. */
int32_t game_mode_menu(int german, GameModeState *st, const GameModeOps *ops);

#endif

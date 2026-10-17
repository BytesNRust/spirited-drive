/* The network game screen (00454b78): the service, Select (host or join),
 * View (only watching), Cancel, over option/netset.qfs. */
#ifndef TNFS_NET_GAME_H
#define TNFS_NET_GAME_H

#include <stdint.h>

/* the palettes' bytes (00454b78's two 0x300 on its stack) */
enum { NET_GAME_PALETTE = 0x300 };

typedef struct NetGameOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    void (*flush)(void *ctx);                                                /* 00464a30 */
    void (*palette_clear)(void *ctx, uint8_t *rgb);                          /* 00463df0: eax */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*palette_put)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);   /* 004763cc (cdecl) */
    void (*inverse_build)(void *ctx);                                        /* 0047463c */
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194] */
    const char *(*service_first)(void *ctx);                                 /* 00442514: the service's entry */
    const char *(*service_next)(void *ctx);                                  /* 004425a0 */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*text_select)(void *ctx, int32_t style);                           /* 004638c0: eax */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*text_fit)(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);  /* 00463d34 */
    void (*palette_show)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*unlock)(void *ctx);                                               /* 004733b0 */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    int32_t (*host_join)(void *ctx);                                         /* 00453ef4: not 0 to race */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void *ctx;
} NetGameOps;

typedef struct NetGameState {
    int32_t screen;          /* 00524834: the screen's picture */
    int16_t pointer_active;  /* the short 00524790 */
    int16_t buttons;         /* the short 00524792 */
    int16_t view;            /* the short 0052b316: only watching (cleared first, and on leaving with 1 or 2) */
    int32_t bank;            /* 004c5570: option/netset.qfs */
} NetGameState;

/* 00454b78 (all registers kept, from 004373c4 / 004376ac): four items over
 * the pointer's bands (y 0..0x8c, 0x8d..0xbe, 0xbf..0x15e, and x 0x141..0x280
 * of 0x15f..0x1e0), Up / Down moving the pointer to the next one's spot
 * (00453e30: x 0x3c, 0x3c, 0x3c, 0x23a; y 0x76, 0xa5, 0xd2, 0x1c4), Enter,
 * Space or the left button choosing once the keys rest (no key, or 0x19
 * ticks); the right button chooses Cancel. The service: 004425a0's next one
 * drawn at (0x84, 0x68); Select: 00453ef4, not 0 to race; View: 0052b316
 * flipped; Cancel and Escape: 1. Gives 0 to race, 1 to leave, 2 when the
 * 12000 ticks run out. */
int32_t net_game_menu(NetGameState *st, const NetGameOps *ops);

#endif

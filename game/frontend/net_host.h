/* The network host / join screen (00453ef4) and the players it lists (00453e40). */
#ifndef TNFS_NET_HOST_H
#define TNFS_NET_HOST_H

#include "game/race/race_net_games.h"
#include "game/race/race_net_players.h"

#include <stdint.h>

typedef struct NetPlayersDrawOps {
    RaceNetPlayers *(*players)(void *ctx, const uint8_t *address);          /* 004428fc: eax */
    void (*text_select)(void *ctx, int32_t style);                           /* 004638c0: eax */
    void (*text_boxed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463934: eax, edx, ebx */
    void *ctx;
} NetPlayersDrawOps;

/* 00453e40 (watcall (_, address), from 00453ef4): 004428fc(address)'s list;
 * style 2: "Racers:" (or "Rennfahrer" with 0052b31c (german) set) at (0x3a,
 * 0xf8); style 0: each player's name at (0x3a, 0x113 + 0x12 i), the count
 * (signed) and the block read again each time; none with no list. */
void net_players_draw(int16_t german, const uint8_t *address, const NetPlayersDrawOps *ops);

typedef struct NetHostOps {
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);    /* 0047310c (cdecl) */
    void (*select)(void *ctx, int32_t picture);                                 /* 00473680 (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                       /* the picture's dword +0x2c */
    void (*save)(void *ctx, int32_t shape, int32_t x, int32_t y);              /* 00476e28 (cdecl) */
    RaceNetGames *(*games)(void *ctx, int32_t timeout);                         /* 00442684: eax */
    void (*clip)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1);   /* 0047723c (cdecl) */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                   /* 00464038: eax, edx */
    int32_t (*ticks)(void *ctx);                                                /* 00473334 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*sound)(void *ctx, int32_t sound);                                    /* 0046492c: eax */
    void (*deadline_set)(void *ctx, int32_t time);                              /* 0047334f (cdecl) */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);          /* 004732dd (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                          /* 004737ec (cdecl) */
    void (*text_select)(void *ctx, int32_t style);                              /* 004638c0: eax */
    int32_t (*text_width)(void *ctx, const char *text);                         /* 004770f4 (cdecl) */
    void (*text_fit)(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);  /* 00463d34 */
    void (*blit_masked)(void *ctx, const uint8_t *shape);                       /* 00473ab4 (cdecl) */
    void (*restore)(void *ctx, int32_t shape, int32_t x, int32_t y);           /* 004737d0 (cdecl) */
    void (*players_draw)(void *ctx, const uint8_t *address);                    /* 00453e40: edx */
    void (*select_default)(void *ctx);                                          /* 00473604 */
    void (*lock)(void *ctx);                                                    /* 00473578 */
    void (*unlock)(void *ctx);                                                  /* 004733b0 */
    void (*flip)(void *ctx);                                                    /* 004733ec */
    void (*pointer_show)(void *ctx);                                            /* 00441798 */
    int32_t (*deadline_passed)(void *ctx);                                      /* 0047336d */
    int32_t (*pointer_buttons)(void *ctx);                                      /* 004417f8 */
    void (*pointer_activate_if_moved)(void *ctx);                               /* 00464088 */
    void (*music)(void *ctx);                                                   /* 0046417c */
    int32_t (*key)(void *ctx);                                                  /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                           /* 00441814 */
    void (*pointer_off)(void *ctx);                                             /* 00464060 */
    void (*picture_free)(void *ctx, int32_t picture);                           /* 004746a0 (cdecl) */
    int32_t (*host)(void *ctx, const char *name);                               /* 00442970: eax (00524df0) */
    int32_t (*connect)(void *ctx, const uint8_t *address);                      /* 00442ad0: eax */
    void *ctx;
} NetHostOps;

typedef struct NetHostState {
    int32_t screen;          /* 00524834: the screen's picture */
    int16_t pointer_active;  /* the short 00524790 */
    int16_t buttons;         /* the short 00524792 */
    int16_t view;            /* the short 0052b316: only watching */
    int16_t no_music;        /* the short 0052b31e */
    int16_t key;             /* 0050a364: a key 004434d8 kept while the network was asked */
    int32_t bank;            /* 004c5570: option/netset.qfs */
    const char *name;        /* 00524df0: the player's name, the game hosted under it */
} NetHostState;

/* 00453ef4 (all registers kept, from 00454b78): a box over the network
 * screen (two pictures saved first, 0xd2 x 0x10a and 0x104 x 0xf0) opened
 * in seven steps (Box2..Box6, Box1, Box1 from 004c5574), listing 00442684's
 * games eight at a time (0x18b, 0xb3 + 0x15 row; names wider than 0x9b
 * lowered by 00438090) under the up arrow (0, Arl1), with the down arrow
 * (9, Arl2), New (10, Newl; not when only watching or 0052b31e is set) and
 * Cancel (11, Cacl); the games asked again every 200 ticks, with the
 * players of the one picked (00453e40). Closed in seven steps, it gives:
 * New: 00442970(00524df0) not 0; a game: 00442ad0(its address); Cancel,
 * Escape, the right button: 0. */
int32_t net_host_join(NetHostState *st, const NetHostOps *ops);

#endif

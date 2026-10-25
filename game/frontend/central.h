/* Control Central (00432ff4): the race setup screen, its network side and the cheat codes. */
#ifndef TNFS_CENTRAL_H
#define TNFS_CENTRAL_H

#include <stdint.h>

/* The cheat codes in the order 00432ff4 steps them (00437258 gets each one's position and letters). */
enum {
    CENTRAL_SKCART, CENTRAL_EARALLYE, CENTRAL_EXPLODE, CENTRAL_NITRO, CENTRAL_CASINOVILLE, CENTRAL_CONCEPTCAR,
    CENTRAL_TURBO, CENTRAL_ROCKYROAD, CENTRAL_CHEATS
};

/* The calls 00432ff4 makes (watcall unless marked cdecl). */
typedef struct ControlCentralOps {
    void (*wait)(void *ctx, int32_t v);                        /* 00475c14 (cdecl), always 0 */
    void (*palette_clear)(void *ctx, uint8_t *rgb);            /* 00463df0: eax (edx 0xd6, unused) */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);  /* 00464038: eax, edx */
    void (*music)(void *ctx);                                  /* 0046417c */
    int32_t (*load_art)(void *ctx, const char *name);          /* 00466c34 */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    void (*sound_load)(void *ctx);                             /* 004648dc */
    int32_t (*load_inverse)(void *ctx, const char *name);      /* 00466c00 */
    void (*music_feed)(void *ctx);                             /* 004640d0 */
    void (*pointer_mark)(void *ctx);                           /* 00441858 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);  /* 004732dd (cdecl) */
    void (*palette_put)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode);
                                                               /* 00463e08: eax, edx, ebx, ecx, the stack */
    void (*net_service)(void *ctx);                            /* 004104d0 (what it does is not read) */
    int32_t (*in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*car_flags)(void *ctx);                              /* 00465ec8 */
    void (*type_changed)(void *ctx);                           /* 00432974 */
    int32_t (*ticks)(void *ctx);                               /* 00473334 */
    int32_t (*chat_waiting)(void *ctx);                        /* 004109b4 (a short; not read) */
    void (*select)(void *ctx, int32_t picture);                /* 00473680 (cdecl) */
    void (*blit)(void *ctx, const uint8_t *shape);             /* 004737ec (cdecl): at its own place */
    void (*blit_masked)(void *ctx, const uint8_t *shape);      /* 00473ab4 (cdecl) */
    void (*blit_word)(void *ctx, int32_t shape);               /* 004737ec with a picture's shape */
    int32_t (*picture_shape)(void *ctx, int32_t picture);      /* the picture's dword +0x2c */
    int32_t (*text_width)(void *ctx, const char *text);        /* 004770f4 (cdecl) */
    void (*text_select)(void *ctx, int32_t style);             /* 004638c0: eax */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8: eax, edx, ebx */
    void (*frame_rect)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t colour);  /* 004827f0 */
    void (*free)(void *ctx, int32_t bank);                     /* 00475260 (cdecl) */
    int32_t (*load_bank)(void *ctx, const char *path, int32_t flags);  /* 00476268 (cdecl) */
    void (*text_at)(void *ctx, const char *text, int32_t x, int32_t y);  /* 0048c264 (cdecl) */
    void (*select_default)(void *ctx);                         /* 00473604 */
    void (*flip)(void *ctx);                                   /* 004733ec */
    void (*pointer_show)(void *ctx);                           /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                     /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*inverse_build)(void *ctx);                          /* 0047463c */
    void (*text_styles)(void *ctx);                            /* 0046376c */
    void (*sound)(void *ctx, int32_t sound);                   /* 0046492c: eax */
    void (*deadline_set)(void *ctx, int32_t time);             /* 0047334f (cdecl) */
    int32_t (*deadline_passed)(void *ctx);                     /* 0047336d */
    int32_t (*key)(void *ctx);                                 /* 00464bd0 */
    int32_t (*scancode)(void *ctx, int32_t c);                 /* 00487b84 (cdecl): a character's */
    int32_t (*cheat_step)(void *ctx, int which);               /* 00437258: eax its position, edx its letters */
    int32_t (*key_down)(void *ctx, int32_t scancode);          /* 00483f8c (cdecl) */
    void (*pointer_activate_if_moved)(void *ctx);              /* 00464088 */
    int32_t (*net_start)(void *ctx);                           /* 0044633c (a short; not read) */
    void (*pointer_click)(void *ctx);                          /* 00441814 */
    void (*kick)(void *ctx, int32_t player);                   /* 00432e90: eax (left out, see STATE) */
    void (*music_fade_out)(void *ctx);                         /* 004644f0 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count); /* 00463ef0: eax, edx */
    void (*music_end)(void *ctx);                              /* 00464568 */
    void (*music_start)(void *ctx, const char *name);          /* 00464428: eax */
    void (*showcase)(void *ctx);                               /* 00408724 (the showcase; not read) */
    void (*showcase_after)(void *ctx);                         /* 004562e8 (not read) */
    void (*race_type_step)(void *ctx, int32_t key);            /* 00432720: eax 0x4b00 or 0x4d00 */
    void (*track_forward)(void *ctx);                          /* 00432c40 */
    void (*track_backward)(void *ctx);                         /* 00432d48 */
    void (*free_inverse)(void *ctx);                           /* 00474558 */
    void (*standings)(void *ctx, int32_t mode);                /* 00461540: eax */
    void (*track_select)(void *ctx);                           /* 004687d0 (its answer dropped) */
    void (*art_free)(void *ctx);                               /* 00432884 */
    void (*options)(void *ctx);                                /* 00444bb0 (not read) */
    void (*chat)(void *ctx);                                   /* 0040a5d8 (not read) */
    int32_t (*yes_no)(void *ctx, const char *text);            /* 00465f4c: eax */
    void (*drain)(void *ctx);                                  /* 004770ac */
    void (*banner)(void *ctx, const char *text);               /* 0046667c: eax */
    void (*pre_race)(void *ctx);                               /* 00446284 (not read) */
    void (*sound_free)(void *ctx);                             /* 004649c8 */
    void (*picture_free)(void *ctx, int32_t picture);          /* 004746a0 (cdecl) */
    void (*flush)(void *ctx);                                  /* 00464a30 */
    void (*race)(void *ctx);                                   /* 00427ae0 (the race) */
    void (*pointer_off)(void *ctx);                            /* 00464060 */
    void (*plus_text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);  /* port: the banner's words,
                                                                * text_draw_smooth at 1/1 (central_glue.c plus_text) */
    void *ctx;
    int32_t (*pad_buttons)(void *ctx);                         /* port: the pad's VirtualPad buttons now (control_map.h),
                                                                * the car select's Color line reads bit 2 (central_livery.c) */
} ControlCentralOps;

/* The globals 00432ff4 reads and writes; a pointer is written, a value only read. */
typedef struct ControlCentralState {
    int16_t *race_type;        /* config.dat's short 00524e5d (race_settings.h) */
    int16_t *opponent;         /* 00524e81 */
    int16_t *track;            /* 00524e85 */
    int16_t *player_car;       /* 00524e8b */
    const int16_t *other_car;  /* 00524e8d..: a lost link game's opponents, by player */
    int16_t *net_car;          /* 00524e9d: a lost link single race's car (0..8) */
    int16_t *laps;             /* 00524ea1 */
    int16_t difficulty;        /* 00524ea3 */
    int16_t e7b;               /* 00524e7b */
    int16_t *volumes;          /* 00524e65, 00524e67, 00524e69 */
    int16_t lost;              /* 0052b312: a link game (1 one kind) */
    int16_t view_only;         /* 0052b316 */
    int16_t *redraw_asked;     /* 0052b31a */
    int16_t german;            /* 0052b31c */
    int16_t no_music;          /* 0052b31e */
    int16_t *tournament;       /* 0052b310 */
    int16_t *link_lost;        /* 0052b314 */
    const int16_t *races;      /* 0052b2d6..0052b2e8: the tournament's, by track */
    const int16_t *kept;       /* 0052b322..0052b329, read live: +0, +4 and +6 used */
    uint32_t *track_flags;     /* 0052b306 */
    int16_t *unlock_level;     /* 0052ad40 */
    int32_t *unlock_m;         /* 004baf0c */
    int32_t *unlock_r;         /* 004baf10 */
    int32_t *lost_vegas;       /* 004baf18 */
    int32_t *vegas_cheat;      /* 004baf1c */
    int16_t *net_player;       /* 004baf20 */
    int16_t *net_player_was;   /* 004baf22 */
    int16_t *was;              /* 004bafb4..004bafbe: the art's opponent, race, track, car, m, r */
    int32_t *art;              /* 004bafc0..004bafd8: track, race, car, opponent, central, ani, the panel */
    int32_t *variant_m;        /* 004fbad8 */
    int32_t *variant_r;        /* 004fbadc */
    int32_t net_side;          /* 004fbb08 */
    int32_t net_host;          /* 004fbb0c */
    int16_t *nomercy;          /* 004c5cc2 */
    int16_t *warrior;          /* 004c5cc4 */
    int32_t *explode;          /* 004c5cd4 */
    int16_t *music_silent;     /* 004c5cbe */
    int32_t *inverse;          /* 004c5ff4 */
    int32_t *nitro;            /* 004faa50 */
    int16_t *net_hover;        /* 004faa54 */
    int16_t *net_self;         /* 004faa56 */
    int16_t *net_second;       /* 004faa58 */
    int16_t *letter_held;      /* 004faa5a */
    int16_t players;           /* 004f81d2 */
    const char *names;         /* 004f8166: 9 bytes a player */
    int16_t *pointer_active;   /* 00524790 */
    int16_t *buttons;          /* 00524792 */
    int32_t *screen;           /* 00524834 */
    const char *paths;         /* 00524328: the data path */
    const uint8_t *keys;       /* 004c693c: by scancode */
    const int16_t *warrior_rival;  /* port: the extra (extras.cfg): the head to head opponent's arrows reach the
                                    * Warrior (11) between 7 and 0; NULL or 0 the original's 0..7 */
    const int16_t *warrior_keep;   /* port: the extra (extras.cfg) "Warrior Player Car": a Warrior picked for the
                                    * player stays picked when the screen starts again (central_car_start); NULL or 0
                                    * the original's, which sends it to car 0 */
} ControlCentralState;

/* 00432ff4: Control Central. The pointer, the arrows, Enter / space / the left button pick and change the
 * opponent, the car, the track, the race type, the race, Options, chat and Cancel (and, in a link game, the
 * players); letters typed are the cheat codes. Escape, Cancel, the right button, the 0x2ee0 ticks (outside a
 * link game and a tournament) or a lost link leave. Returns 2 for the timer, or for a link lost before a race
 * (00437249), 0 otherwise. */
int32_t control_central(ControlCentralState *st, const ControlCentralOps *ops);

#endif

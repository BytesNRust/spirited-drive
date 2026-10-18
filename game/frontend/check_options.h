/* The options reached from the screens after a race: audio, graphics, controls, load / save (00403760). */
#ifndef TNFS_CHECK_OPTIONS_H
#define TNFS_CHECK_OPTIONS_H

#include <stdint.h>

enum { CHECK_OPTIONS_PALETTE = 0x300, CHECK_OPTIONS_KEPT = 0xac }; /* the palette and the dwords 0x2b on its stack */

typedef struct CheckOptionsOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);                    /* 00466c00: eax the name */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194] */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*audio)(void *ctx, int32_t mode);                                  /* 004015dc: eax 1 */
    void (*graphics)(void *ctx, int32_t mode);                               /* 00423d28: eax 0 */
    void (*controls)(void *ctx, int32_t mode);                               /* 00413cb0: eax 1 */
    void (*load_save)(void *ctx, int32_t mode);                              /* 0042c498: eax 1 (load_save.h) */
    void (*load_save2)(void *ctx, int32_t mode);                             /* 0042d894: eax 1 (load_save2.h) */
    void (*music_fade_to)(void *ctx, int32_t volume);                        /* 00464604: eax */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void (*wait)(void *ctx, int32_t v);                                      /* 00475c14 (cdecl), 0 */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    void (*config_save)(void *ctx);                                          /* 0042a504 */
    void *ctx;
} CheckOptionsOps;

/* the words the screen reads, through pointers as the original reads memory (the sub-screens change them) */
typedef struct CheckOptionsState {
    uint8_t *config;                /* 00524df0: config.dat's image; 00524df9's 0xac bytes put back by Cancel */
    const int16_t *lost;            /* the short 0052b312: the connection lost (no Load / save) */
    const int16_t *language;        /* the short 0052b31c: "loam" over Load / save only while 0 */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;               /* the short 00524792 */
    int32_t *screen;                /* 00524834: the screen's picture */
    int32_t *inverse;               /* 004c5ff4: 00466c00's table */
    const int16_t *quit;            /* the short 0050a5b6: 1 leaves */
} CheckOptionsState;

/* 00403760 (eax mode: its low half 0 from 0040b648, Load / save through 0042c498, else 0042d894 as from
 * 0040c710): check/chkopts.qfs's six items over the pointer's bands: audio (004015dc 1), graphics (00423d28 0),
 * controls (00413cb0 1), load / save (not while the connection is lost, Up / Down step over it), Done and Cancel
 * (Left / Right between them; Down from Cancel goes to Graphics). Cancel and Escape put config.dat's 00524df9..
 * back as it was on entry and the music's volume (00524e67) with it; the 0x2ee0 ticks, the right button and
 * 0050a5b6 leave too. config.dat is saved (0042a504) on leaving. Returns 1. */
int32_t check_options(int32_t mode, const CheckOptionsState *st, const CheckOptionsOps *ops);

#endif

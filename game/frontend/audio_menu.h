/* The audio screen: the song, the race music's, menu music's and effects' volumes, the sound test, speech, the audio
 * mode and 3D sound (004015dc). */
#ifndef TNFS_AUDIO_MENU_H
#define TNFS_AUDIO_MENU_H

#include <stdint.h>

/* config.dat's image from 00524df9, kept on entry and put back by Cancel and Escape, and its shorts the screen sets */
enum { AUDIO_KEPT = 0xac, AUDIO_PALETTE = 0x300 };
enum { AUDIO_MODE = 0x68,    /* 00524e61: 0..2, 0 only with 0052b32c */
       AUDIO_SONG = 0x6a,    /* 00524e63: 0..15, 0 no music */
       AUDIO_RACE = 0x6c,    /* 00524e65: the race music's volume, 0..0x7f */
       AUDIO_MENU = 0x6e,    /* 00524e67: the menus' music's */
       AUDIO_EFFECTS = 0x70, /* 00524e69: the effects' */
       AUDIO_SPEECH = 0x72,  /* 00524e6b: its low byte toggled */
       AUDIO_3D = 0x74 };    /* 00524e6d: the same */

typedef struct AudioMenuOps {
    void (*music_fade_out)(void *ctx);                                       /* 004644f0 */
    void (*music_end)(void *ctx);                                            /* 00464568 */
    void (*track_music)(void *ctx);                                          /* 00456244: config.dat's song played */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);                    /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194], its answer unused */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*clock)(void *ctx);                                             /* 00473324 */
    void (*stop_4a)(void *ctx);                                              /* 00464980 */
    int32_t (*done_4a)(void *ctx);                                           /* 00464994: 0x4a played out */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*music_volume)(void *ctx, int32_t volume);                         /* 0046463c: eax */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    void (*blit_at)(void *ctx, int32_t shape, int32_t x, int32_t y);         /* 004737d0 (cdecl) */
    void (*blit_shape)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);  /* 004737d0, a bank's shape */
    void (*clip)(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom);  /* 0047383c (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    void (*pointer_hide)(void *ctx);                                         /* [004c618c] */
    void (*unlock)(void *ctx);                                               /* 004733b0 */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void (*race_music)(void *ctx);                                           /* 00464240 */
    void (*music)(void *ctx);                                                /* 0046417c */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    void (*blit_masked)(void *ctx, const uint8_t *shape);                    /* 00473ab4 (cdecl) */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*race_key)(void *ctx);                                          /* 00464ef4 (hot_key.h) */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    void (*music_stop)(void *ctx);                                           /* 00464534 */
    void (*voice_volume)(void *ctx, int32_t voice, int32_t volume);          /* 00473b4c: eax, edx */
    int32_t (*effects_level)(void *ctx);                                     /* 00464670 */
    void (*music_fade_to)(void *ctx, int32_t volume);                        /* 00464604: eax */
    void (*effects_volume)(void *ctx, int32_t volume);                       /* 004645d8: eax */
    void (*banner)(void *ctx, const char *text);                             /* 0046667c: eax */
    void (*sound_free)(void *ctx);                                           /* 004649c8 */
    void (*sound_card)(void *ctx);                                           /* 00464304 */
    void (*sound_load)(void *ctx);                                           /* 004648dc */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*net_sync)(void *ctx);                                             /* 004104d0 (race_net_sync) */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*picture_free)(void *ctx, int32_t picture);                        /* 004746a0 (cdecl) */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void (*music_pick)(void *ctx);                                           /* 004562e8 */
    void (*music_start)(void *ctx, const char *name);                        /* 00464428: eax */
    void *ctx;
} AudioMenuOps;

/* the globals the screen reads, through pointers as the original reads memory */
typedef struct AudioMenuState {
    uint8_t *config;              /* 00524df9: AUDIO_KEPT bytes of config.dat's image, the shorts unaligned */
    int16_t *track_silent;        /* the short 004c5cbe: the volumes in kept (0052b322..) while set */
    const int16_t *kept;          /* 0052b322..0052b329: the race music's (+0), menu music's (+4) and effects' (+6) */
    const int16_t *card_bit1_clear;  /* the short 0052b32a: no audio mode or 3D item */
    const int16_t *card_bit0;     /* the short 0052b32c: audio mode 0 offered */
    const int16_t *sound_alt;     /* the short 0052b30e: 5, no audio mode item */
    const int16_t *lost;          /* the short 0052b312: the mode not changed in a link game */
    const int16_t *german;        /* the short 0052b31c */
    const int16_t *sound_on;      /* the short 0052b304: the sound started again on a mode change */
    const int32_t *voice_4a;      /* 005247e8 */
    const int32_t *voice;         /* 00524808 */
    const int16_t *pointer_x;     /* the short 004c6110 */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;             /* the short 00524792 */
    const int32_t *screen;        /* 00524834: the screen's picture */
    int32_t *inverse;             /* 004c5ff4: 00466c00's table */
    int16_t *small_stream;        /* the short 004c5cca: set before pause.asf */
    const int16_t *net_off;       /* the short 0052b302: 004104d0 not called while set */
    const int16_t *quit;          /* the short 0050a5b6: 1 leaves */
} AudioMenuState;

/* 004015dc (eax mode: 2 the race's pause menu; all registers kept but eax): option/audio.fsh's nine items over the
 * pointer's bands: the song (the next of 16), the race music's, menu music's and effects' sliders (clicked or Left /
 * Right; the effects' with sound 0x4a played while on it), speech, the audio mode (not in the pause menu or with
 * 0052b32a, not with 0052b30e 5; refused with a banner in a link game) and 3D sound (not in the pause menu or with
 * 0052b32a), Done and Cancel (Left / Right between them). Cancel and Escape put config.dat's image back as it was on
 * entry; the 0x2ee0-tick deadline, the right button or 0050a5b6 == 1 leave too. The sound is started again on leaving
 * when the mode changed, and pause.asf played in the pause menu. Returns 0. */
int32_t audio_menu(const AudioMenuState *st, int32_t mode, const AudioMenuOps *ops);

#endif

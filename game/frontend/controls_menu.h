/* The controls screen: the devices and keys for steering, throttle / brake, shifting, the camera, the handbrake and
 * the horn (00413cb0). */
#ifndef TNFS_CONTROLS_MENU_H
#define TNFS_CONTROLS_MENU_H

#include "game/sys/control_map.h"

#include <stdint.h>

enum { CONTROLS_PALETTE = 0x300, CONTROLS_SHORTS = 13 };

typedef struct ControlsMenuOps {
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    void (*flush)(void *ctx);                                                /* 00464a30 */
    void (*palette_clear)(void *ctx, uint8_t *rgb);                          /* 00463df0: eax */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);                    /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*text_styles)(void *ctx);                                          /* 0046376c */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194], its answer unused */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*text_select)(void *ctx, int32_t style);                           /* 004638c0: eax */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*blit_grab)(void *ctx, int32_t shape, int32_t x, int32_t y);       /* 00476e28 (cdecl) */
    void (*key_name)(void *ctx, int32_t key, char *out);                     /* 00465498: eax, edx */
    void (*text_shadowed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8: eax, edx, ebx */
    void (*palette_put)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*select_default)(void *ctx);                                       /* 00473604 */
    void (*flip)(void *ctx);                                                 /* 004733ec */
    void (*pointer_show)(void *ctx);                                         /* 00441798 */
    void (*deadline_set)(void *ctx, int32_t time);                           /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    void (*blit_at)(void *ctx, int32_t shape, int32_t x, int32_t y);         /* 004737d0 (cdecl) */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    int32_t (*pointer_buttons)(void *ctx);                                   /* 004417f8 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    int32_t (*clash)(void *ctx);                                             /* 00465d54 (config_dat_controls_clash) */
    void (*text_boxed)(void *ctx, const char *text, int32_t x, int32_t y);   /* 00463934: eax, edx, ebx */
    int32_t (*key_capture)(void *ctx, char *name);                           /* 004657d0: eax (key_capture.h) */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void (*music)(void *ctx);                                                /* 0046417c */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*calibrate)(void *ctx, int32_t mode);                              /* 0043bc00: eax 1, the joystick screen */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*net_sync)(void *ctx);                                             /* 004104d0 (race_net_sync) */
    void (*picture_free)(void *ctx, int32_t picture);                        /* 004746a0 (cdecl) */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void *ctx;
    /* port: for controls_plus.c, all NULL unless the state has a `plus` */
    void (*plus_text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);  /* style 0 cream, 1 yellow */
    int32_t (*plus_text_width)(void *ctx, const char *text);
    void (*plus_panel)(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       int32_t framed);  /* the rectangle darkened (rgb: the screen's palette), a red frame if framed */
    void (*plus_save)(void *ctx, const PortControls *controls);   /* controls.cfg, and the race's pad from now on */
    int32_t (*pad_raw)(void *ctx, RawPad *out);   /* the first pad or wheel as it is now; 0 when there is none */
} ControlsMenuOps;

/* port: the bindings the added pages edit */
typedef struct ControlsPlus {
    PortControls *port;
} ControlsPlus;

/* the globals the screen reads, through pointers as the original reads memory */
typedef struct ControlsMenuState {
    int16_t *controls;            /* 0052ad26..0052ad3f, CONTROLS_SHORTS shorts: the steering, throttle / brake and
                                   * shift devices (0 the keyboard), their keys in pairs, the handbrake's, the
                                   * camera's and the horn's keys, and one more short only kept and put back */
    const int16_t *german;        /* the short 0052b31c: the devices' names in German */
    const int32_t *joystick_a;    /* 004fcdcc: a first joystick's devices offered */
    const int16_t *joystick_b;    /* the short 004baec0: the second's */
    const int32_t *calibration;   /* 004fcdb4: the Calibrate item offered */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;             /* the short 00524792 */
    const int32_t *screen;        /* 00524834: the screen's picture */
    int32_t *inverse;             /* 004c5ff4: 00466c00's table */
    const int16_t *net_off;       /* the short 0052b302: 004104d0 not called while set */
    const int16_t *quit;          /* the short 0050a5b6: 1 leaves */
    ControlsPlus *plus;           /* port: NULL is the original screen exactly */
} ControlsMenuState;

/* 00413cb0 (eax mode: 0 option/cntrlset's art, else check/chconset's; all registers kept but eax): nine items over
 * the pointer's bands: steering, throttle / brake and shifting (the device cycled over those offered; the keyboard
 * asks for its two keys, Enter asks again), the camera, handbrake and horn keys, Calibrate (0043bc00, only while
 * 004fcdb4), Done and Cancel (Left / Right between them). Keys that clash (00465d54) are asked for again. Cancel and
 * Escape put the controls back as they were on entry; the 0x2ee0-tick deadline, the right button or 0050a5b6 == 1
 * leave too. Returns 1. */
int32_t controls_menu(const ControlsMenuState *st, int32_t mode, const ControlsMenuOps *ops);

#endif

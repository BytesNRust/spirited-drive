/* The graphics screen: the resolution, the window's size, the image's detail, the view distance, the cars' detail and
 * the horizon (00423d28). */
#ifndef TNFS_GRAPHICS_MENU_H
#define TNFS_GRAPHICS_MENU_H

#include "game/sys/port_settings.h"

#include <stdint.h>

/* config.dat's image from 00524df9, kept on entry and put back by Cancel and Escape, and its shorts the screen sets */
enum { GRAPHICS_KEPT = 0xac, GRAPHICS_PALETTE = 0x300 };
enum { GRAPHICS_RESOLUTION = 0x76,  /* 00524e6f: 0 640, 1 320 (its low byte toggled) */
       GRAPHICS_WINDOW = 0x78,      /* 00524e71: 0..2 full, medium, small */
       GRAPHICS_IMAGE = 0x7a,       /* 00524e73: 0..3 chosen here (the art has six), at most 1 at 320 */
       GRAPHICS_VIEW = 0x7c,        /* 00524e75: 0..2 far, medium, close */
       GRAPHICS_CARS = 0x7e,        /* 00524e77: 0 high, 1 low (its low byte toggled) */
       GRAPHICS_HORIZON = 0x80 };   /* 00524e79: 0 on, 1 off (the same) */

typedef struct GraphicsMenuOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);                /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);                        /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);                    /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                                           /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);       /* 004732dd (cdecl) */
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src,
                        int32_t mode);                                       /* 00463e08: eax, edx, ebx, ecx, stack */
    void (*pointer_poll)(void *ctx);                                         /* [004c6194], its answer unused */
    void (*pointer_mark)(void *ctx);                                         /* 00441858 */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                              /* 00473680 (cdecl) */
    void (*blit_own)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl) */
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
    void (*sound)(void *ctx, int32_t sound);                                 /* 0046492c: eax */
    void (*lock)(void *ctx);                                                 /* 00473578 */
    int32_t (*ticks)(void *ctx);                                             /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);                            /* 00464088 */
    int32_t (*key)(void *ctx);                                               /* 00464bd0 */
    void (*pointer_click)(void *ctx);                                        /* 00441814 */
    void (*pointer_off)(void *ctx);                                          /* 00464060 */
    void (*music)(void *ctx);                                                /* 0046417c */
    void (*net_sync)(void *ctx);                                             /* 004104d0 (race_net_sync) */
    int32_t (*deadline_passed)(void *ctx);                                   /* 0047336d */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);               /* 00463ef0: eax, edx */
    void (*free_inverse)(void *ctx);                                         /* 00474558 */
    void (*free)(void *ctx, int32_t bank);                                   /* 00475260 (cdecl) */
    void *ctx;
    /* port: for graphics_plus.c, all NULL unless the state has a `plus` */
    void (*plus_text)(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);  /* style 0 cream, 1 yellow */
    int32_t (*plus_text_width)(void *ctx, const char *text);
    void (*plus_panel)(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       int32_t framed);  /* the rectangle darkened (rgb: the screen's palette), a red frame if framed */
    void (*plus_save)(void *ctx, const PortSettings *settings);
} GraphicsMenuOps;

/* port: the settings the port's additions edit, and what they need to know */
typedef struct GraphicsPlus {
    PortSettings *port;
    ScreenFormat fullscreen_format;   /* the desktop's nearest format, taken when full screen is turned on */
} GraphicsPlus;

/* the globals the screen reads, through pointers as the original reads memory */
typedef struct GraphicsMenuState {
    uint8_t *config;              /* 00524df9: GRAPHICS_KEPT bytes of config.dat's image, the shorts unaligned */
    int16_t *showing;             /* the short 004f9e8c: 1 while the screen is up */
    const int16_t *net_off;       /* the short 0052b302: check/graphics.fsh's art, and 004104d0 not called */
    const int16_t *pointer_active;  /* the short 00524790 */
    int16_t *buttons;             /* the short 00524792 */
    const int32_t *screen;        /* 00524834: the screen's picture */
    int32_t *inverse;             /* 004c5ff4: 00466c00's table */
    const int16_t *quit;          /* the short 0050a5b6: 1 leaves */
    GraphicsPlus *plus;           /* port: NULL is the original screen exactly */
} GraphicsMenuState;

/* 00423d28 (eax mode: not 0 from the race, without the resolution item; all registers kept but eax): option/
 * graphics.qfs's (check/graphics.fsh's while 0052b302) eight items over the pointer's bands: the resolution, the
 * window, the image's detail, the view distance, the cars' detail and the horizon (each the next with Space, Enter or
 * the left button, F1..F4 the window, the image, the view and the horizon at once; 320 keeps the image at 1 or 0),
 * Done and Cancel (Left / Right between them). Cancel and Escape put config.dat's image back as it was on entry; the
 * 0x2ee0-tick deadline, the right button or 0050a5b6 == 1 leave too. Returns 1. */
int32_t graphics_menu(const GraphicsMenuState *st, int32_t mode, const GraphicsMenuOps *ops);

#endif

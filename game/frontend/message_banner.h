/* The menus' centred message banner (0046667c). */
#ifndef TNFS_MESSAGE_BANNER_H
#define TNFS_MESSAGE_BANNER_H

#include <stdint.h>

enum { MESSAGE_BANNER_SHADE = 0x100 };

/* The calls 0046667c makes; pictures are EA's (0047310c's), passed as words. */
typedef struct MessageBannerOps {
    int32_t (*text_width)(void *ctx, const char *text);                         /* 004770f4 (cdecl) */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);     /* 0047310c (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                       /* the picture's dword +0x2c */
    /* 0048263c (cdecl): table[i] = the colour of pal nearest i's scaled by r, g, b (16.16) */
    void (*shade_table)(void *ctx, int32_t r, int32_t g, int32_t b, const uint8_t *pal, uint8_t *table);
    void (*select)(void *ctx, int32_t picture);                    /* 00473680 (cdecl): the draw surface */
    void (*select_default)(void *ctx);                             /* 00473604: 00473680([0052d2f4]) */
    void (*grab)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 00476e28 (cdecl): the screen into shape */
    void (*shade_set)(void *ctx, uint8_t *table);                  /* 004826f0 (cdecl) */
    void (*shade_polygon)(void *ctx, int32_t count, const int32_t *xy);  /* 004827cf (cdecl): count, then x, y */
    int32_t (*colour)(void *ctx, int32_t rgb);                     /* 00473140 (cdecl) */
    void (*frame)(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t colour);  /* 004827f0 */
    void (*text_styles)(void *ctx);                                /* 0046376c */
    void (*text_select)(void *ctx, int32_t style);                 /* 004638c0: eax the style */
    void (*text_centred)(void *ctx, const char *text, int32_t cx, int32_t y);  /* 00463c60: eax, edx, ebx */
    void (*blit)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 004737d0 (cdecl) */
    void (*blit_own)(void *ctx, int32_t shape);                    /* 004737ec (cdecl): at its own place */
    void (*flip)(void *ctx);                                       /* 004733ec */
    void (*picture_free)(void *ctx, int32_t picture);              /* 004746a0 (cdecl) */
    void *ctx;
} MessageBannerOps;

typedef struct MessageBannerInputs {
    int32_t screen;          /* 00524834: the screen's picture */
    const uint8_t *palette;  /* 00520dd8: the screen's palette */
} MessageBannerInputs;

/* 0046667c: a 0x3c-high picture as wide as the text plus 0x14, centred on
 * 0x140 at y 0xd2: the screen under it grabbed, darkened (0x9000) through a
 * shade table, two red frames, the text centred in style 2 at y 0x12; then
 * blitted, the whole screen picture blitted at its own place, flipped, and
 * the picture freed. */
void message_banner_show(const char *text, const MessageBannerInputs *in, const MessageBannerOps *ops);

/* The calls 00467214 makes past 0046667c's. */
typedef struct MessageBannerWaitOps {
    void (*music_service)(void *ctx);                                        /* 0046417c */
    int32_t (*picture_new)(void *ctx, int32_t w, int32_t h, int32_t flags);  /* 0047310c (cdecl) */
    int32_t (*picture_shape)(void *ctx, int32_t picture);                    /* the picture's dword +0x2c */
    void (*select)(void *ctx, int32_t picture);                    /* 00473680 (cdecl) */
    void (*grab)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 00476e28 (cdecl) */
    void (*banner)(void *ctx, const char *text);                   /* 0046667c (message_banner_show) */
    void (*deadline_set)(void *ctx, int32_t time);                 /* 0047334f (cdecl) */
    void (*blit)(void *ctx, int32_t shape, int32_t x, int32_t y);  /* 004737d0 (cdecl) */
    void (*picture_free)(void *ctx, int32_t picture);              /* 004746a0 (cdecl) */
    void (*pointer_show)(void *ctx);                               /* 00441798 */
    int32_t (*deadline_passed)(void *ctx);                         /* 0047336d */
    int32_t (*key)(void *ctx);                                     /* 00464bd0 */
    int32_t (*pointer_buttons)(void *ctx);                         /* 004417f8 */
    void (*select_default)(void *ctx);                             /* 00473604 */
    void (*blit_own)(void *ctx, int32_t shape);                    /* 004737ec (cdecl) */
    void (*flip)(void *ctx);                                       /* 004733ec */
    void *ctx;
} MessageBannerWaitOps;

/* 00467214: the banner over the screen picture, the screen under it put
 * back on the picture at once (0x1b8 x 0x3c at 0x64, 0xd2). With a time
 * other than -1 it stays shown until the time passes, a key, or a button
 * of the active pointer (the short 00524792, cleared first), then 10 more,
 * and the screen picture is shown again; the music is serviced all along. */
void message_banner_wait(const char *text, int32_t time, int32_t screen, int *pointer_active,
                         int *pointer_buttons, const MessageBannerWaitOps *ops);

#endif

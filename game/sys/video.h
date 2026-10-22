/* EA's display layer as used by the front end: a 640x480 8-bit back buffer
 * that is flipped to the screen, and a hardware-style palette that changes
 * what is on screen immediately (DirectDraw 8-bit mode in the original). */
#ifndef TNFS_VIDEO_H
#define TNFS_VIDEO_H

#include "formats/fsh.h"

#include <stdint.h>

enum { VIDEO_WIDTH = 640, VIDEO_HEIGHT = 480 };

int video_init(void);
void video_shutdown(void);

uint8_t *video_back_buffer(void);
/* Where drawing (the blits, the rects, the text) goes: a w x h block of pixels, NULL for the back buffer (the
 * default). The front end's pictures (0047310c / 00473680) are such blocks. */
void video_select_target(uint8_t *pixels, int w, int h);
uint8_t *video_target(int *w, int *h);
/* 0047383c: the blits kept to left <= x < right, top <= y < bottom (cut to the target), until the next
 * video_select_target, which opens the box to the whole target again (00473680 loads the picture's own). */
void video_set_clip(int left, int right, int top, int bottom);
void video_clear_back(void);                                          /* PTR_004c618c */
void video_flip(void);                                                /* 004733ec */
void video_wait_vblank(void);                                         /* 004735bc */

void video_set_palette(int first, int count, const uint8_t *rgb);     /* 004763cc */
void video_get_palette(uint8_t rgb[768]);                             /* 00476648 */
void video_fade_in(int first, int count, const uint8_t *target);      /* 00463e7c */
void video_fade_out(int first, int count);                            /* 00463ef0 */

/* port: whether the display is already dark -- every colour on it is black, so a screen that fades its caller's
 * picture out before putting its own page up has nothing left to fade and can skip those seventeen vblanks. It is
 * the video layer's own answer, not a caller's flag: a fade_out over the whole palette leaves it entirely zero
 * (palette_fade_out_step's last step is 0), and anything that puts light on the display clears it again. */
int video_display_is_dark(void);

/* Sprite composited over the displayed frame with 0xFF transparent: the
 * mouse pointer, which the original drew straight onto the front surface
 * with save-under (004416cc). NULL hides it. Re-presents the screen. */
void video_set_overlay(const FshEntry *shape, int x, int y);

/* Dialog helpers, all on the back buffer. */
void video_copy_front_to_back(void);
/* Copies a w x h block between the back buffer and a caller buffer (the
 * original saved the area under a dialog in an offscreen surface, 00476e28). */
void video_read_rect(int x, int y, int w, int h, uint8_t *out);
void video_write_rect(int x, int y, int w, int h, const uint8_t *in);
/* Replaces every pixel p in the block with table[p] (darkened dialog boxes). */
void video_shade_rect(int x, int y, int w, int h, const uint8_t table[256]);
/* One-pixel outline from (x0, y0) to (x1, y1) inclusive. */
void video_frame_rect(int x0, int y0, int x1, int y1, uint8_t color);

/* Draws a bitmap entry into the back buffer at (x, y), clipped. */
void video_blit(const FshEntry *shape, int x, int y);                 /* 004737d0 */
/* Same at the shape's own screen position (misc[2], misc[3]). */
void video_blit_at_own_position(const FshEntry *shape);               /* 004737ec */
/* Same with 0xFF transparent (highlight frames and arrows). */
void video_blit_transparent_at_own_position(const FshEntry *shape);   /* 00473ab4 */
void video_blit_transparent(const FshEntry *shape, int x, int y);      /* 00473a98 */

#endif

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
void video_clear_back(void);                                          /* PTR_004c618c */
void video_flip(void);                                                /* 004733ec */
void video_wait_vblank(void);                                         /* 004735bc */

void video_set_palette(int first, int count, const uint8_t *rgb);     /* 004763cc */
void video_get_palette(uint8_t rgb[768]);                             /* 00476648 */
void video_fade_in(int first, int count, const uint8_t *target);      /* 00463e7c */
void video_fade_out(int first, int count);                            /* 00463ef0 */

/* Draws a bitmap entry into the back buffer at (x, y), clipped. */
void video_blit(const FshEntry *shape, int x, int y);                 /* 004737d0 */
/* Same at the shape's own screen position (misc[2], misc[3]). */
void video_blit_at_own_position(const FshEntry *shape);               /* 004737ec */

#endif

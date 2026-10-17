/* EA's picture and shape library as the checked menus call it (int32 banks and pictures, raw shape entries), over
 * game/sys: banks are resource_load_art's blocks, pictures are blocks of pixels drawing can be pointed at
 * (video_select_target), shapes are the raw 16-byte entries (shape_bank_find's) or a picture's own. */
#ifndef TNFS_EA_LIB_H
#define TNFS_EA_LIB_H

#include <stdint.h>

int32_t ea_bank_load(const char *name);                       /* 00466c34: 0 when it cannot be read */
void ea_bank_free(int32_t bank);                              /* 00475260 on a bank */
const uint8_t *ea_shape(int32_t bank, const char *tag);       /* 004732dd: NULL when it has none */

int32_t ea_picture_new(int32_t w, int32_t h);                 /* 0047310c: 0 when there is no room */
void ea_picture_free(int32_t picture);                        /* 004746a0 */
int32_t ea_picture_shape(int32_t picture);                    /* the picture's +0x2c: its shape */
int32_t ea_screen(void);                                      /* 00524834: the screen's 640 x 480 picture */

void ea_select(int32_t picture);                              /* 00473680: drawing goes to it */
void ea_select_default(void);                                 /* 00473604: drawing goes to the back buffer */
void ea_blit_own(const uint8_t *shape);                       /* 004737ec: at the shape's own place */
void ea_blit_masked(const uint8_t *shape);                    /* 00473ab4: the same, 0xff left out */
void ea_restore(int32_t shape, int32_t x, int32_t y);         /* 004737d0: a shape at (x, y) */
void ea_save(int32_t shape, int32_t x, int32_t y);            /* 00476e28: (x, y) of the drawing copied into it */

#endif

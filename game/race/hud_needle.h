/* The tachometer's needle (0042221c, from 00422598 in the in-car views). */
#ifndef TNFS_HUD_NEEDLE_H
#define TNFS_HUD_NEEDLE_H

#include "game/gfx/quad.h"
#include "game/race/dash_art.h"

#include <stdint.h>

/* 004b9558..004b9564: the needle last drawn. */
typedef struct {
    int32_t tip_x, tip_y;       /* 004b9558, 004b955c */
    int32_t inner_x, inner_y;   /* 004b9560, 004b9564 */
} HudNeedle;

/* 0042221c (eax = the car): angle = min(rpm (+0x3c1), redline + 0x3d4) x 600
 * / redline + 0x1a9 (1024 a turn; redline the viewed car's specs +0xac);
 * tip = the tach's middle + fx_mul_round(radius, fx_cos / fx_sin), inner =
 * (middle + (middle + tip) / 2) / 2. The old needle is erased by 'tach'
 * (00473ab4) clipped to the boxes old tip..midpoint and midpoint..old inner,
 * 2 pixels out; then the clip is the screen (screen_w x screen_h) and at high
 * resolution the needle is three lines (00484254: as is, one right, one
 * down) in `colour` (0050b790). Each clip goes through 0047383c (kept inside
 * the surface, surface_w x surface_h) and is left in t. TODO(0048383b): the
 * low resolution needle is not drawn. Where the original divides by zero
 * (redline 0) or overflows the division the port draws nothing. */
void hud_draw_needle(QuadTarget *t, HudNeedle *needle, const DashArt *art, int32_t rpm, int32_t redline,
                     int32_t surface_w, int32_t surface_h, int32_t screen_w, int32_t screen_h, int32_t high_res,
                     uint32_t colour);

#endif

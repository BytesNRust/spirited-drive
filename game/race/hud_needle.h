/* The tachometer's needle (0042221c, from 00422598 in the in-car views). */
#ifndef TNFS_HUD_NEEDLE_H
#define TNFS_HUD_NEEDLE_H

#include "game/gfx/needle.h"
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
 * down) in `colour` (0050b790), at low resolution the anti-aliased needle
 * (0048383b, needle_draw) from inner to tip, one pixel wide at both ends,
 * in 00482a36's colour (`fade`, 004f9744's block) through `line` (its clip
 * 0041f818's). Each clip goes through 0047383c (kept inside the surface,
 * surface_w x surface_h) and is left in t. Where the original divides by
 * zero (redline 0) or overflows the division the port draws nothing; with
 * no line or block the low resolution needle is not drawn (port guard). */
void hud_draw_needle(QuadTarget *t, HudNeedle *needle, const DashArt *art, int32_t rpm, int32_t redline,
                     int32_t surface_w, int32_t surface_h, int32_t screen_w, int32_t screen_h, int32_t high_res,
                     uint32_t colour, NeedleLine *line, const uint8_t *fade);

#endif

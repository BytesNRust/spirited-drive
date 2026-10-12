/* The view's mask (00421c18, from 00422598): the 3D view is drawn into an
 * offscreen surface (004c2d90) and copied to the screen through the run
 * lists of the .mmb (or the .b%d%d parts), which leave out the overlay's
 * pixels. The surfaces are the caller's, reached through HudMaskOps. */
#ifndef TNFS_HUD_MASK_H
#define TNFS_HUD_MASK_H

#include "game/race/dash_art.h"
#include "game/race/hud.h"

#include <stdint.h>

typedef enum {
    HUD_SURFACE_VIEW = 0,       /* 004c2d90: the offscreen view */
    HUD_SURFACE_SCREEN = 1,     /* 004c2da0, when 004b95b4 is set */
    HUD_SURFACE_DEFAULT = 2,    /* 004735dc's (0052d2f0) otherwise */
    HUD_SURFACE_BACK = 3,       /* 004c2d98: 0x71 x 0x8c high, 0x39 x 0x34 low (0044ca3c); its shape at +0x2c */
    HUD_SURFACE_MAP = 4         /* 004c2d9c: the track map's (00422ce0) */
} HudSurface;

typedef struct {
    void *ctx;
    void (*select)(void *ctx, HudSurface s);    /* 00473680 / 004735dc: the draw surface */
    /* 0047383c: the clip rectangle (left, right, top, bottom) */
    void (*clip)(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom);
    void (*fill)(void *ctx, uint32_t colour);   /* 00482980: the clip rectangle filled */
    void (*draw)(void *ctx, uint8_t *shape);    /* 00473ab4: a shape at its own place */
    void (*view_clip)(void *ctx);               /* 0041e3f4 */
    void (*present)(void *ctx);                 /* 004737d0: the whole view surface at (0, 0) */
    void (*runs)(void *ctx, const uint8_t *runs); /* 00483fa0: the view surface through a run list */
} HudMaskOps;

typedef struct {
    int32_t view;           /* 00511fe8's +0xc: the camera's view */
    int32_t window_size;    /* 00511ff0 */
    uint8_t screen_kind;    /* 004fb789: 3 copies the two last columns */
    int32_t use_parts;      /* 004f9b3c: the .b%d%d parts instead of the .mmb */
    int32_t back_buffer;    /* 004b95b4 */
    int32_t width, height;  /* 0051201c, 00512020 */
    uint32_t strip_colour;  /* 0050b7b4 */
} HudMaskInputs;

/* 00421c18. first (eax) != 0: 'tach' from the bank drawn into the view
 * surface, the screen selected, hud 004f9a28 cleared, 0041e3f4, the view
 * surface presented. Otherwise row = view - 1 (0 below), and with
 * screen_kind 3: when *clear_view (004fb78a) the view surface cleared in the
 * strip colour and copied through column 2, *clear_view 0, then column 3;
 * else column window_size. The run list is the .mmb record at
 * art->mmb + mmb_words[row][column] (the original's words are pointers, the
 * loader 004511a4 having relocated them) or parts[row][column]. Port guards:
 * no 'tach' (the original aborts), a row and column whose flat index leaves
 * the 12 entries (the original reads the fields after them): nothing more. */
void hud_mask_draw(const HudMaskOps *ops, const DashArt *art, HudState *hud, uint8_t *clear_view,
                   const HudMaskInputs *in, int32_t first);

#endif

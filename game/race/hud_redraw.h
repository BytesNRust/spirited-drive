/* The race screen's full redraw (004218d0, from 00422598 when 004b9568 is
 * set): the background tiles, the top row's labels, the frame round the
 * view and the tach. */
#ifndef TNFS_HUD_REDRAW_H
#define TNFS_HUD_REDRAW_H

#include "game/gfx/quad.h"
#include "game/race/dash_art.h"
#include "game/race/hud.h"
#include "game/race/race_state.h"

#include <stdint.h>

/* The view row's frame (004b75d0 row +0x24..+0x30, ViewRow.unknown_14[4..7]). */
typedef struct {
    int32_t left, top, width, height;
} HudFrame;

/* What 004218d0 reads besides the HUD's own. */
typedef struct {
    HudFrame frame;         /* 00511fe8's view row */
    int32_t high_screen;    /* 00512018: the tiles start at y 0x20, else 0xd */
    int32_t width, height;  /* 0051201c, 00512020 */
    int32_t gap, gap_ticks; /* for 00420370 */
    int32_t high_res;       /* 004fa0c0 */
    uint32_t strip_colour;  /* 0050b7b4 */
} HudRedrawInputs;

/* 004218d0: the tiles '0000'..'0003' (004b94e0: 30 entries, [(column & 1)
 * + 2 row]) at (column x w, start + row x h) for width / w columns and
 * height / h rows, w x h the size of '0000' (004737d0); 00420370; the frame
 * with 0047380c: 'b00t' at (x, top) and 'b00b' at (x, bottom) for x from
 * left while below right, by the width of 'b00t'; 'b00l' at (left, y) and
 * 'b00r' at (right, y) for y from top while below bottom - h ('b00l''s
 * height), by h; 'b00l' and 'b00r' again at bottom + 1 - h plus their own
 * +0xa; the corners 'b000' (left, top), 'b001' (right, top), 'b002' (right,
 * bottom), 'b003' (left, bottom), right = left + width - 1, bottom = top +
 * height - 1; 'tach' (00473ab4); 0041e230. Port guards (the original
 * faults or runs on): a name the bank lacks, a tile of width or height 0, a
 * row past the table's 30 entries, a frame piece of width or height 0 or
 * less with an edge to draw: it stops there. */
void hud_redraw(const QuadTarget *t, HudState *hud, const DashArt *art, const RaceState *race,
                const HudRedrawInputs *in);

/* What 00420918 reads. */
typedef struct {
    HudFrame frame;             /* 00511fe8's view row */
    int32_t high_screen;        /* 00512018 */
    int32_t width, height;      /* 0051201c, 00512020 */
    int32_t high_res;           /* 004fa0c0 */
    int32_t surface_w, surface_h;  /* 004c78b4, 004c78b8: 0047383c keeps the clips inside */
} HudPartsInputs;

/* 00420918 (from 00420710 when the parts mode 004f9b3c is switched off with
 * a camera or 004f9b7c set): the lower right quarter of the tiles (columns
 * from width / w / 2, rows from height / h / 2, as 004218d0 places them)
 * drawn twice, clipped (0044987c) first to the box below the view (high
 * resolution (0x206, top + height + 2, 0x74, 0x1bd - top - height), low
 * (0xfe, top + height + 1, 0x50, 0xb3 - top - height)) then to the one right
 * of it (high (left + width + 2, 0x130, 0x74, 0x84), low (left + width + 1,
 * 0x75, 0x50, 0x3f)); then the clip is the screen (high, 0044985c) or the
 * low box again; then the frame's lower right: 'b00b' at (x, bottom) for x
 * from left + width - 5 w ('b00t''s width) while below right, 'b00r' at
 * (right, y + 1) for y from top + height - 4 h - 1 ('b00l''s height) while
 * at most bottom - h, 'b002' at (right, bottom), all with 0047380c. The
 * clip is left in t. Port guards as hud_redraw. */
void hud_parts_redraw(QuadTarget *t, const DashArt *art, const HudPartsInputs *in);

#endif

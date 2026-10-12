/* `--drive`'s heads-up display: the mask bank and its .mmb read as 00450838
 * names them (high resolution, English), set up and drawn with the ported
 * HUD (hud.c) over each frame. Glue, no exe function of its own. */
#ifndef TNFS_DRIVE_HUD_H
#define TNFS_DRIVE_HUD_H

#include "game/race/dash_art.h"
#include "game/race/hud.h"
#include "game/race/hud_needle.h"
#include "game/race/race_palette.h"
#include "game/race/race_state.h"

#include <stddef.h>
#include <stdint.h>

enum {
    DRIVE_HUD_W = 640, DRIVE_HUD_H = 32,    /* the top strip ('stat', 004829b0's fill) */
    DRIVE_VIEW_W = 640, DRIVE_VIEW_H = 480, /* the view surface (004c2d90) */
    DRIVE_VIEW_HEADER = 0x10                /* drawn from as an 8-bit shape (type 0x7b) */
};

typedef struct {
    uint8_t strip[DRIVE_HUD_W * DRIVE_HUD_H];  /* what the HUD drew: it redraws only what changed */
    uint8_t *bank, *mmb;        /* the files, the caller's to free with drive_hud_free */
    DashArt art;
    ReplayArt replay;
    HudState hud;
    int ready;                  /* set up: drawn each frame */
    uint32_t strip_colour;      /* 0050b7b4 */
    uint8_t view[DRIVE_VIEW_HEADER + DRIVE_VIEW_W * DRIVE_VIEW_H];  /* 004c2d90's shape: the 3D view goes here */
    int changed;                /* 00511fec: the camera took a view; the full redraw and 00421c18(1) are due */
    int32_t shown_view;         /* the view drawn for (-1 none yet) */
    HudNeedle needle;           /* 004b9558..: the needle last drawn */
    uint32_t needle_colour;     /* 0050b790 */
} DriveHud;

/* Reads data_root/SIMDATA/MISC/maskhi.fsh and MASKHI.mmb (an upper-case
 * retry for case-sensitive file systems). 0 when either is missing. */
int drive_hud_read(DriveHud *h, const char *data_root);

/* Inside the race palette's build, before the queue is applied: 00450838's
 * part after the reads (dash_load_setup: the bank queued, the colours
 * 0xf0.. queued, the shapes found). */
void drive_hud_setup(DriveHud *h, RacePalette *p);

/* After the palette is applied: the bank mapped onto it with its own
 * '!pal' (what 00447098 does with the queued banks), the strip colour
 * (00446880(0, 0, 0): the entry nearest black) and the HUD's start state
 * (0041f9a0: layout 0, redraw on; 0041e230). */
void drive_hud_start(DriveHud *h, RacePalette *p);

/* Before the 3D view is drawn, as 0044cc74 does: when the camera's view
 * changed (00511fec), 004218d0 into the view surface (the tiles, the labels,
 * the frame round the view, the tach). Returns where the 3D view is drawn:
 * the view surface's pixels when the HUD is set up (00421c18 copies them to
 * the screen), else the frame itself. */
uint8_t *drive_hud_view(DriveHud *h, RaceState *race, uint8_t *pixels);

/* 00422598's order: 00421c18 (after a view change with 1: 'tach' into the
 * view surface, all of it to the frame; otherwise the view surface through
 * the run list of the camera's view and window size 0), 00420710's first part (the
 * labels 10 ticks after the start lights were last drawn) and the top row
 * (00421e40) for the viewed car into the strip, which then covers the
 * frame's top rows, then 0042221c (the tach's needle at the viewed car's
 * revs, `redline` its specs' +0xac). `pixels` is the persistent frame (the
 * original redraws only what changed). TODO: 00420710's keys, 0041e3f4.
 * pixels NULL: the strip only (a frame not shown). */
void drive_hud_draw(DriveHud *h, RaceState *race, int32_t redline, uint8_t *pixels, int width, int height);

void drive_hud_free(DriveHud *h);

#endif

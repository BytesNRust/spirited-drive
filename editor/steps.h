/* --steps: a user's edits played through the window, for the flow tests (editor_e2e_track, editor_e2e_car). A comma list, in order:
 *
 *   edit           a track's point added, or the open mask's first texel painted another class
 *   undo, redo, save
 *   title          the window's title printed ("title: ...")
 *   print          a track's points printed ("points: COUNT x,z x,z ...")
 *   key:NAME       a key pressed and let go (SDL's key names: N, I, Delete, X ...), as the keyboard sends it
 *   click:I        the left button pressed and let go on the track's point I, in the Viewport
 *   drag:I,DX,DY   point I taken and dragged DX, DY screen pixels, in four moves, then let go
 *   width:I,M      a road key's width on point I set to M metres, as the Points window sets it
 *   bank:I,DEG     a road key's bank on point I set to DEG degrees, as the Road profile's bank cell sets it
 *   race           a car's Test in race, as its button: the game's command line printed ("race: ...") and its TNFS_ environment ("race env: ...")
 *   race-log       waits for the game's log to name the race's car ("race log: race car: slot 0 is ..."); fails when the game ends first
 *   race-stop      the race stopped as the Stop button stops it, waited for, its end printed ("race end: ...")
 *
 * A number after a step is that step's (drag:2,0,-5). The keys and the mouse go in as SDL events, so the window takes them as it takes a
 * person's; each such step waits a frame after its last event, so the edit is done (and is a step to undo) before the next. */
#ifndef TNFS_TRACK_EDITOR_STEPS_H
#define TNFS_TRACK_EDITOR_STEPS_H

#include "app.h"

#include <SDL3/SDL.h>

enum { STEPS_MAX = 64, STEPS_ARGS = 3 };

typedef struct {
    char name[STEPS_MAX][16];
    double arg[STEPS_MAX][STEPS_ARGS];
    int args[STEPS_MAX];
    int count, at, phase;
    int frames;   /* drawn since the steps began */
    float from_x, from_y;   /* where the step's point was in the window when it began (click, drag) */
    int failed;   /* a step that is not one or did nothing: the steps after it are not done */
} Steps;

/* `text` read into `s`; 0 with a word on stderr when a step is not one */
int steps_read(Steps *s, const char *text);
/* 1 while steps are left */
int steps_running(const Steps *s);
/* This frame's part, after the window was drawn: the steps that need no frame are done, an input step sends its next events (taken by the
 * next frame). `win` gets the events. */
void steps_frame(Steps *s, App *a, SDL_Window *win);

#endif

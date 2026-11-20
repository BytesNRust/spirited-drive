/* --steps: a user's edits played through the window, for the flow tests (editor_e2e_track, editor_e2e_car). A comma list, in order:
 *
 *   edit           a track's point added, or the open mask's first texel painted another class
 *   undo, redo, save
 *   title          the window's title printed ("title: ...")
 *   print          a track's points printed ("points: COUNT x,z x,z ...")
 *   key:NAME       a key pressed and let go (SDL's key names: N, I, Delete, X ...), as the keyboard sends it; Shift+ or Cmd+ before
 *                  the name holds that modifier (key:Shift+])
 *   click:I        the left button pressed and let go on the track's point I, in the Viewport
 *   shift-click:I, cmd-click:I   the same with Shift (a range) or Cmd (one more or one less) held (dep87)
 *   box:X0,Z0,X1,Z1   a left drag on the Viewport from track place (X0, Z0) to (X1, Z1): the points inside it selected
 *   row:I, shift-row:I, cmd-row:I   point I's row of the Points table clicked (plain, Shift, Cmd), as the table does
 *   rotate:DEG     the selected points rotated DEG degrees about the first, as Rotate points does (an undo step of its own)
 *   print-sel      the selected points printed ("selected: 3-7", "selected: none")
 *   print-profile  the Road profile of the selected points as the panel shows them, each row Left|Right ("profile: Off-road|Asphalt 3.00|Mixed ...")
 *   status         the status line printed ("status: 3 points removed")
 *   copy, paste    Cmd+C and Cmd+V on the selected run of points (app_points.h); paste goes after the last selected point
 *   mirror-lr, mirror-fb, reverse   the Points window's Mirror L/R, Mirror F/B and Reverse on the selected run
 *   drag:I,DX,DY   point I taken and dragged DX, DY screen pixels, in four moves, then let go
 *   drag-near:I,J,M  point I dragged as drag does, to M metres before point J on the line from J to I (a place in metres, whatever the zoom)
 *   width:I,M      a road key's width on point I set to M metres, as the Points window sets it
 *   bank:I,DEG     a road key's bank on point I set to DEG degrees, as the Road profile's bank cell sets it
 *   problems       what Problems shows first: "problems: refusal TEXT" (Build's words), "problems: red node N" or "problems: none"
 *   build          Build, as its button: what it says printed ("build: built ID.zip" or "build: build refused: ...")
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

enum { STEPS_MAX = 64, STEPS_ARGS = 4 };

typedef struct {
    char name[STEPS_MAX][16];
    double arg[STEPS_MAX][STEPS_ARGS];
    int args[STEPS_MAX];
    int count, at, phase;
    int frames;   /* drawn since the steps began */
    float from_x, from_y;   /* where the step's point was in the window when it began (click, drag) */
    float to_x, to_y;       /* where a drag lets it go */
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

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
 *   curve:DIR,DEG,R,K  Add curve's fields (DIR 1 right, 0 left; DEG degrees, R metres, K points, 0 for the default) and Accept
 *   straight:L,K   Add straight's (L metres, K points or 0) and Accept
 *   curve-block    prints "curve block: open" or "curve block: folded": the Points panel's Add curve / straight block as last drawn
 *   curve-fold:N   the block unfolded (1) or folded (0) as a click on its header would; it keeps that whatever the room
 *   colour-by:N    the Viewport's Colour by mode N (TraceColourBy: 0 Limits, 1 Curve, 2 Bank, 3 Width, 4 Lanes, 5 Shoulder, 6 Fence, 7 Grade)
 *   print-classes  the road's node classes in that mode as runs, "classes 1: 0@0-36 2@37-81 ..." (class@first-last)
 *   dblclick-road:X,Z  a double click on the road at track place X, Z in metres: it selects that stretch (not in Limits mode)
 *   strip-dblclick:I   a double click on point I's mark in the Distance strip's profile: the top view centres on that place and zooms in to at least 1 px/m
 *   print-view         prints "view: CX CZ SCALE", the top view's centre and pixels per metre
 *   sheet          takes the Race window's sheet (Day and Night of the car) and waits for it; fails when it cannot start
 *   print-sheet    the cells and how they ended, "sheet 2: Day=done Night=failed"
 *   sheet-save     Save sheet: writes the PNG in the project's sheets/ folder, prints "sheet saved: PATH"
 *   preview:KIND   the Viewport shows what Add curve (1) or Add straight (2) would make with their fields, until preview:0 (for a picture)
 *   drag:I,DX,DY   point I taken and dragged DX, DY screen pixels, in four moves, then let go
 *   drag-near:I,J,M  point I dragged as drag does, to M metres before point J on the line from J to I (a place in metres, whatever the zoom)
 *   width:I,M      a road key's width on point I set to M metres, as the Points window sets it
 *   bank:I,DEG     a road key's bank on point I set to DEG degrees, as the Road profile's bank cell sets it
 *   height:I,M     point I's height set to M metres, as the Points window's h cell and the profile in the Distance strip set it (dep95)
 *   strip          the Distance strip's tab brought to the front (a hidden tab draws nothing)
 *   profile-drag:I,DY   point I's mark in the strip's profile pressed, moved DY pixels (down is lower) in four moves, let go: the height it means (needs strip first)
 *   profile-fine:I,DY  the same with Shift held during the drag: the height moves a tenth as far
 *   profile-click:I,DX a click on the profile DX pixels right of point I's mark: it remembers that place (30 is between two points)
 *   insert-here        the Insert point here button: a point at the remembered place
 *   slope:G,IN,OUT,KEEP      the Slope block over the selected run: grade G %, eased IN m and OUT m, KEEP 1 the road after it kept; then Accept (slope-rise:R,.. by rise R m)
 *   slope-preview:N    the profile draws what Accept would make (N 1; 0 stops), held, for a picture
 *   strip-split:DY     the splitter between the Viewport and the Distance strip dragged DY pixels (down: a lower strip); strip-size:H drags it to H pixels high (whatever it was); print-strip prints "strip: H", its window height in pixels
 *   print-confirm  what waits for an answer and what is to run, "confirm: quit|none|other" and "go: quit|none|other" (Escape never quits; a quit is asked every time)
 *   print-height:I      point I's height printed ("height: 2 12.3")
 *   night-light:N, night-tint:R,G,B (0..1), night-reset:K (0 the light to 100 %, 1 the tint to the grey), lights-open (the Preview's Lights header unfolded), print-night: the car Preview's Night light and tint, as its slider, picker and Reset buttons set them
 *   problems       what Problems shows first: "problems: refusal TEXT" (Build's words), "problems: red node N" or "problems: none"
 *   build          Build, as its button: what it says printed ("build: built ID.zip" or "build: build refused: ...")
 *   race           a car's Test in race, as its button: the game's command line printed ("race: ...") and its TNFS_ environment ("race env: ...")
 *   race-log       waits for the game's log to name the race's car ("race log: race car: slot 0 is ..."); fails when the game ends first
 *   poll-wait:S   the window runs S seconds, so a file changed meanwhile is seen as when it is open: "poll: watching" first, then "poll: STATUS" when the photos are due again
 *   new-car-open   File > New car with a workspace: the window up, for a picture
 *   new-car:T      File > New car: the name "Car <word>" typed, car T (0..7: the eight you drive) chosen, Create pressed; the kit is made and opened
 *   physics-from:T the open car kit's Physics from: car T (0..8: the eight you drive, then the Warrior) copied into its specs.tsv and pdn.tsv
 *   physics-open:T the Physics from question up with car T named, for a picture
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
    char name[STEPS_MAX][48];
    double arg[STEPS_MAX][STEPS_ARGS];
    int args[STEPS_MAX];
    int count, at, phase;
    int frames;   /* drawn since the steps began */
    float from_x, from_y;   /* where the step's point was in the window when it began (click, drag) */
    float to_x, to_y;       /* where a drag lets it go */
    double wait_from;   /* poll-wait: the clock when it began */
    long wait_serial;   /* poll-wait: the photo edit serial it last saw */
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

# Track editor (phase 1a)

`tnfs-track-editor` is a separate program for making a track's centre line. It links the core and SDL, never the game's code. It works on
the source of a track (a project folder), builds the package the game installs, and installs it with the game's own installer. The track
is tested in the game, not in the editor. Design: `notes/dep59-track-editor-design.md`.

The build uses no CD data: the road is turned into the TRI of `track_template_generic` (core/formats/track_template.h), the INFO is the
editor's own generic one, and the stage's art is a FAM of your choice (`art=` in project.ini, e.g. the CC0 straight in
`art/tnfs-tracks/generic-straight/GENERIC.FAM`).

## Run

```sh
nix develop -c sh -c 'cmake -S . -B build/native -G Ninja && cmake --build build/native --target tnfs-track-editor'
./build/native/tnfs-track-editor art/tnfs-tracks/my-road
```

The project folder is created with `project.ini` and `trace.json` (a straight of six points, 200 m) when they are missing.

| File | What it holds |
|------|---------------|
| `project.ini` | `id`, `name`, `author`, `stem` (1..7 of A-Z 0-9, not AL/CL/CY/TR first), `art` (a FAM: relative to the project folder, or absolute), `redistributable` (the author's word on the art's licence) |
| `trace.json` | `width` (the road's width byte, 8..120; the template's is 40, see Road width) and `points`, the control points as `[x, z]` in metres (one track unit is one metre), and optional `version` (2) and `keys`, the road keys (see Road keys). A file with no `version` or `1` has no keys; a newer one is refused |
| `build/<id>.zip` | written by Build; the only thing the game installs |

## Keys

| Input | Action |
|-------|--------|
| left drag on a point | move it (the selected point) |
| right or middle drag, wheel | pan, zoom (wheel zooms about the cursor) |
| arrows | nudge the selected point by 1 unit |
| `N` | add a point after the last one, along the last segment |
| `I` | insert a point after the selected one |
| `Del` or `X` | remove the selected point (a trace keeps 2) |
| `F` | fit the view to the trace |
| `S` | save `trace.json` |
| `B` | build `build/<id>.zip` |
| `U` | install the last built zip (runs the game: `--game`, `--user`) |
| `Esc` | quit |

## Limits

The road is drawn through the control points as a Catmull-Rom curve, resampled to the CD's node spacing (about 6 m). Up to 8 degrees a node
(a radius of 43 m) nothing is marked. Over that a node is drawn **amber**: the road is built, but the curve is tighter than almost all of the
CD's (the CD has some of 16 degrees: Alpine 3), so check the terrain in the game; the AI brakes there. A node is drawn **red** when it turns more
than 16.5 degrees from the node before (the CD's own most is 16.2), or the curve there is tighter than 20.9 m: Build refuses while any node is red. In a tight curve the
generator holds the terrain on the inside of the curve to 0.4 of its radius, as the CD does (`notes/dep59-editor-limits-vs-cd.md`); the outside is left as it is, except
where an S's easing runs out (the CD cuts both sides, but its scenery hides the outer hole and the generic template has none). The cut starts and ends as a slope, at most a node
length (6 m) of terrain edge from one node to the next, so an S bend has no step at its inflection. A
layout of `track new` does not turn the terrain and keeps its own limit of 8 degrees. The numbers are measured and provisional, not limits of the game.

How tight a curve is depends on how far apart its points are: two points close together make the curve turn within a short distance. Hover a red or amber
stretch to see it; the fix is to space the points out or to smooth them.

The **Limits** window (a tab beside Problems at the bottom) shows the node count against the 2397 a TRI holds (599 segments and the closing node), the
length and, in red or amber, a line for each rule some node breaks ("2 nodes turn more than 16.5° per node: the track can't be built", "26 nodes turn
more than 8° per node: they may not look right in the game"; the same for the radius). A rule no node breaks has no line. The **Problems** window lists
the nodes themselves, one row each, red first and then by node number: "node 80: turns 9.4° (more than 8°) and radius 38 m (under 43 m), may
not look right". A click on a row selects the nearest point and centres the Viewport on that node. With none it says "No problems". Both windows are
dockable like the others and their layout is kept.

## Start screen, cars, and projects: New, Open, recent, and unsaved changes

Studio has one project open, a track or a car. With nothing to reopen it shows a **start screen**: Open car..., Open track..., New track..., Export original
car... and **Recent** (cars and tracks together, up to eight; a folder that is gone is greyed). A normal run with no folder given
reopens the project it had last. The **File** menu has the same entries, Save (a track's) and Quit. Each kind keeps its own docked layout in the layout file.

Open track wants a folder that holds a project (a `project.ini`); New track wants one that holds none, and makes the project there (the window's folder dialog
can make the folder). The last eight project folders are kept in the editor's own folder (`recent.txt` beside its layout; a car's line starts with `car` and a tab,
and a file from the first Studio, all tracks, still reads). The view fits the project that was opened.

**Open car** takes a car kit's folder (the one that holds `coche/` and `modelo/`) or its `coche/`; given `coche/` with a `modelo/` beside it, the kit is opened.
What makes it a car: a `mod.ini` with `type=car` and a `stem=`, and the car's file `SIMDATA/CARFAMS/<STEM>.CFM` beside it. `car_masks/<STEM>/` may be missing (listed as
none). A folder that is not a car is refused with what was looked at and what is missing. The car window shows and edits the card (`mod.ini`): version, year, power, weight, make, model, country, category, author, description, ABS and traction
control (yes, no, or the default, which removes the line), sound from (the game's eight original cars) and the colors (add, remove, move up or down, name, and a
color picker; up to 16). Each field is checked by the game's own reader (`mod_ini_parse`, car_piece's words) and a field with a problem is red, with what is wrong
and its number ("Year: 17 characters; the card keeps up to 16"); **Save is blocked while any field has a problem**. Undo and Redo (buttons, the Edit menu,
Ctrl or Cmd+Z and Shift+Z) take back typing in a field as one step. **Save writes only `coche/mod.ini`**, through `mod_ini_set`: only the lines you changed,
every other byte (comments, order, line ends) stays, and a Save with no change writes nothing. A card with unsaved changes asks before it is left, as a track does.
The textures (`modelo/c1`, and how many far ones) and the mask files found are listed beside it. `--set KEY=VALUE` sets a field before the first frame (headless
runs). **Rehacer** (a button in the car window, and the Rehacer tab with its log) runs the kit's own `rehacer.command` with the kit as its folder, through `sh`, with no
input. Before it runs, Studio copies the whole kit (but `.studio-backup` and Finder's `.DS_Store`) into `.studio-backup/<YYYYMMDD-HHMMSS>/` and keeps the last
five; a kit over 512 MB is not backed up, so it does not run. The script's output (both streams) streams into the log, Stop ends it, and its exit code is shown.
When it ends the textures and masks lists and the card are read again. It is refused, with the reason in the status line, when the kit has no `rehacer.command`
(the button is disabled), when `mod.ini` has changes that are not saved (the button offers Save first), or while another run goes; while it runs the card is not
edited and nothing else is opened. `--rehacer` runs it before the first frame (a headless run waits for it and fails when its exit code is not 0).
The same folder on the command line opens as a track when it has a `project.ini`, as a car
when it holds a car's `mod.ini`, else as a new track project.

**Export original car** (File menu and the start screen) makes a project of your own from one of the game's cars, to adjust its masks. It reads the player's own
copy of the game's files (`gamedata/` in the player folder: `--user`, else the system's) for the car files, and the game's own `data/` (the app's
`Contents/Resources/data`, or `data/` beside a build tree's program) for the masks; `--cd DIR` and `--port-data DIR` name either. The window lists the cars; one
the game has no masks for (probe) or no files for is greyed, with the reason as its tooltip. After the car, a folder is asked for and the project is made in it,
in a folder named for the car (`p911-original`, `wagon-masks`). It is made beside as `<folder>.exporting` and renamed when whole, so a failure leaves
nothing, and a folder that is already there is refused (a second export of the same car into the same folder changes nothing).
- **The eight cars you drive** (a normal car project, as `tnfs-studio car new` makes one, but with the car's own stem): `mod.ini` (`redistributable=0`, the
  catalogue's colors as `paints=`, ABS and traction control from the PBS, a description that says the files are the game's), `SIMDATA/CARFAMS/<STEM>.CFM` and
  `.PDN`, `SIMDATA/CARSPECS/<STEM>.PBS`, `car_masks/<folder>/` and `dash_masks/<STEM>/`. It opens with the card editable, as any car does. A game stem is
  not a mod stem the game takes (`mod_validate` refuses it), so this project is for looking and for the masks, not for Build.
- **The twelve traffic cars and the police car** (a masks project; they have no PBS, so they cannot be driven): `project.ini` (`kind=masks`, `masks=<folder>`,
  `cfm=original/<X>.CFM`, `export_sizes=<mask>:<W>x<H> ...`, `redistributable=0`), `original/<X>.CFM` (a copy kept to look at) and `car_masks/<folder>/`. It
  opens with the Car window (read only: what it is) and the Masks window only; there is no card, no Save and no Rehacer.

The masks are the game's `data/car_masks/<folder>/` copied as they are (same names, same bytes, so the same 8-bit grey PNGs and sizes; the old `*.before-*_mask.png`
copies, which the game does not read, are left out). The Masks window's **Reveal masks folder** shows the folder in the Finder, and says what it maps to in the
game's source tree (`data/car_masks/<folder>/`): the way to change a game mask is to copy the files there. Everything made this way says "Made from the game's
files: for your own use, do not share" and has `redistributable=0`. `--export-car FOLDER --into PARENT` does it before the first frame (headless), `--ask export`
shows the window and `--pick FOLDER` chooses a car in it.

**Test in race and Race photo** (the Race window, for a project of one of the game's own cars: the eight from Export original car, and the traffic and police ones).
Both run the game (the one Studio found; Locate game if it is not there) on a scratch player folder made for the run (`studio-race-XXXXXX` in the temporary folder,
Restomod, no extra liveries, render scale 1, removed when the run ends): the player's own folder and the installed game are never touched. The project's masks are
in front of the game's own (`TNFS_CAR_MASKS=<project>/car_masks`, and `TNFS_DASH_MASKS` for a car you drive); the person's own `TNFS_*` variables are dropped.
**Day** or **Night** (Night is `light_time=night` and `light_effects=1`), and a track from the list (default Rusty Springs, the night reference; Lost Vegas has no
time of day). A car you drive is raced alone (`--car N --racers 1`; N is the car's model for a photo, and the car menu's number for the window, whose race is
Control Central's: the game turns that number into the model by its own table, 00426620). A traffic or police car is raced in a race with traffic (`--type 1`), which
runs only on the City, Coastal and Alpine stages (the circuits are greyed for it; default Coastal 1); the player drives the Toyota Supra Turbo, and the window says so
and where the tested car is: the window's traffic is the game's own draw, so a given traffic car is on the road in some races and not in others. **Test in race** opens the game's window; **Stop** ends it. **Race photo** runs
the game headless and shows one frame in the window next to the masks (200 ticks in for a car you drive, 800 for a traffic car, 250 for the police car). For a
traffic or police car the camera has to follow the slot the car has in the race (`TNFS_FOLLOW_CAR`): the traffic's models are drawn by the race's seed, so a
probe run (1 tick) per seed reads the game's `race car: slot N is STEM` lines and Studio tries the game's own seed and then 24 more (`TNFS_DRIVE_SEED`) until the car
is in the draw; if none has it there is no photo, and the window says so. A traffic car waits parked high above the track until the player comes near, so the shot asks
the game to wait until the car has been on the road 64 ticks (`TNFS_FOLLOW_SETTLE`, past the 800); if it stays parked in a draw the next draw is tried (4 such draws at most), and the
summary says the tick of the frame. The police car is put 15 nodes behind the player, chasing (`TNFS_POLICE_NEAR`), so its camera
has it at any tick. Studio cannot see whether the car is on screen: look at the picture. A game older than this Studio does not log the slots: the photo says so, and
Test in race still runs. The game's log (its output and its own `spirited-drive.log`) is in the window, errors red, the masks it took green; the game looks for every
car's mask in the override first and says "cannot read" for the cars the project has none for: those lines are dim, they are expected. `--race-photo` takes the
photo before the first frame (with `--night`, `--track STEM`, `--race-png FILE`), headless runs wait for it.

Closing the window, Escape and Quit with changes that are not in `trace.json` (a point moved, added or removed, or the road width changed since the last
save or open) ask "Save the changes to <project> before quitting?" with **Save** (Enter), **Discard** and **Cancel** (Escape). New, Open and Open recent ask
the same before they go on (the folder is asked for after the answer). A save that fails keeps the question open with the reason. Build and Install
use what is on screen, not the file, as before.

## Metres and the Node window

One track unit is one metre (`TRACE_METRES_PER_UNIT` in `core/formats/track_trace.h` says where that comes from), and the editor writes every distance in metres.
The Points table has a `width (m)` column (see Road keys) and two read-only columns beside x and z: `dist m`, the distance along the curve from the start to the point, and `turn`, the turn at the
node nearest the point ("R 5.3°" to the right, "L 3.1°" to the left). Hovering a node in the Viewport shows its number, its distance from the start and its
turn. The **Node** window (under Points) shows the selected node, picked by a click in the Viewport, in Points or in Problems: its number, x and z, the
distance from the start, the turn, the distance to the node before and to the next one, and the limits it breaks. With none it says "Select a node".
The distance between nodes is not the same on every road: the editor spaces them evenly along the curve and fits a whole number of segments (4 nodes
each) to its length, so it is about 6.03 m on a long road and can be a few per cent more or less on a short one (the CD's is about 6.0 m).
Height and slope are not shown: the editor has no height yet.

## Road width: the Track window

The Track window (above Project in the first layout; a tab beside Project in a layout kept from before it) has the road's width in metres, tarmac edge to edge: 2 to 30 m, one pixel of drag is 0.25 m, 10 m by default. Past
the tarmac the surface changes and the walls start further out. In `trace.json` the width is still the node byte `width` (track_gen's byte 0 of every
node): the distance of each edge from the centre in eighths of a metre (VERGE_UNIT 0x2000 in 16.16, game/race/car_walls.c), so metres = byte / 4
and the template's 40 is 10 m. The terrain rows move by the same amount, 16 raw per byte (0.125 m).

## Road keys: the width per point

A control point can set the road's width from there on: in the Points table, drag its `width (m)` cell (2 to 30 m, one pixel is 0.25 m). That makes a **road key** on the
point. A point with no key shows the width it inherits, dimmed: from the Track window's width before the first key, and between two keys a smoothstep of the
distance along the curve (3t²-2t³, so the road widens gently at both ends; its steepest slope is 1.5 times the average), and after the last key the last key's
width holds. Right-click a key's cell and choose "Inherit again" to clear it. Inserting a point keeps the keys on their points, removing one drops its key. The
Node window shows the node's effective width.

Problems marks amber a node whose width changes by more than 1.25 m (5 bytes) from the node before: D metres over L metres of road on 6.25 m nodes (a 300 m road) steps 1.5 x D x 6.25 / L,
so an 8 to 14 m ramp is right at the limit at 45 m of road (5.00 bytes), clean from about 45 m on and amber under it (at 40 m). A width clause on a node that is red for another reason is covered by "can't be built". In `trace.json` the keys are written only when there are some, with `"version": 2`:

```json
{ "version": 2, "width": 32, "points": [[0,0],[60,0],[120,0],[180,0],[240,0],[300,0]],
  "keys": [ {"point": 1, "width_m": 8.0}, {"point": 4, "width_m": 14.0} ] }
```

A key whose point does not exist, twice on a point, or with a width outside 2..30 m refuses the file with a message naming it. Unknown fields are ignored. Banks, lanes and
surface per key are not read yet; their place is reserved in the key (`TRACE_KEY_*` bits in `core/formats/track_trace.h`).

## Preview

The **Preview** window (a tab beside Textures and Race) shows each texture of the car file as the race has it: the game's own code (the fixed black of the masks, then the paint) runs on a copy of the car file with this project's masks in front of the game's, and an unsaved mask edit counts as if it were saved. Pick the paint from the card's `paints=` (a car you drive; a traffic or police car has none: the race does not paint it). It updates by itself when a mask is edited or saved, a paint or its colour changes, or Rehacer ends (not while a pencil stroke is going).

The bank's palette is taken to 6 bits a channel, as the race loads it. Not applied: the track's light and the screen palette's nearest-colour snapping. Instead Preview counts the distinct colours of the body and says how many the car's entries in the race palette (23 of its own, 72 shared when several cars race) cannot hold: the rest will look like their nearest. Texels that name one of the race's screen entries (0xfa-0xfe) are hatched, and 0xff is a chequer. Headless: `--paint N` shows the card's N-th paint.

## Changes from the game's masks

At the top of the **Masks** window of a project made from one of the game's cars (Export original car), **Changes from the game's masks** compares each `*_mask.png` of `car_masks/<folder>/` with the game's own copy, `data/car_masks/<folder>/`, the one the export started from: *unchanged*, *changed* (how many texels, and which classes gained and lost them), *new* (only the project has it) or *missing* (only the game has it). Masks are compared as classes, so a file written again by another picture editor with the same texels is unchanged; backups with an extra dot in the name (`frnt.before-1650_mask.png`) are left out, as the export leaves them. **Refresh** reads the files again; **Copy the changed list** puts plain text on the clipboard (the folder, then a line for each changed file with its texel count and classes, each new and each missing file, and how many are unchanged) for pasting into a hand-over. Read only. Headless: `--changes` prints the same text.

## Edit mask

The **Edit mask** window (a tab beside Textures and Race) paints a car's masks. Pick a texture of the car file (the ones with a mask in `car_masks/<car>/`; the low detail bank's are marked "low"): the mask is drawn over the texture, each class in its legend colour, with an Overlay switch, an Opacity slider and a switch per class. The class chosen in the legend is the one the tools paint with.

- **Pencil**: a square of 1 to 8 texels; drag to draw a line (one step to undo). **Fill**: the connected texels of the same mask class, or of the same texture colour. **Eyedropper**: takes the class under the click.
- **Undo / Redo** (Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y while the window has the keyboard), **Save mask** (Ctrl+S too), **Discard**.
- Save writes the mask as the 8-bit grey PNG of `data/car_masks`: the same name and size, every texel outside the edit as it was; a mask with no change is not rewritten. Changing texture, closing the project or running Rehacer with unsaved mask changes asks first.
- A mask whose size is not its texture's says both sizes: the game scales the mask to the texture (nearest neighbour), so the Studio shows it scaled the same way.
- **Race photo** (or **Save, then Race photo** when there are changes) runs the game on the saved masks and shows the picture in the Race window.

Headless: `--mask NAME` (`side`, `frnt_lo`) opens a mask, `--mask-rect X,Y,W,H,CLASS` paints a rectangle of it (repeatable, one step each), `--mask-save` saves it; with `--shot` the picture is the window.

## Headless run

For a smoke run (no window events, no sound):

```sh
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_RENDER_DRIVER=software timeout -k 10 60 ./build/native/tnfs-track-editor PROJECT \
    --frames 3 --save --build --shot out.png
```

`--install --game ./build/native/tnfs --user SCRATCH` runs the game's installer into a scratch player folder. `--select N` selects control point N first
(for a picture of the Node window). `--edit` adds a point first (something unsaved) and `--ask quit|new|open` asks for that action, to show the question
of the unsaved changes.

## What is missing

- The INFO is generic: lines 9 and 10 (the sky colours) are a plain blue and the other lines zero. Not checked against the game's sky.
- Heights and banks are not read from `trace.json` yet (every node is level and unbanked); the width is per road key (see Road keys).
- The install was not run end to end in this version: it invokes the game's `--install`, which needs a built game.
- The 3D free-flight view, lighting and scenery editing are later phases.

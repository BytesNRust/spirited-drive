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
./build/native/tnfs-track-editor <workspace>/tracks/my-road
```

A workspace is one folder with `cars/` and `tracks/` (your own mods; the game's originals live in `tweaks/<id>/studio/project/`) in it (File > Workspace, or `--workspace DIR`; the choice is kept between runs): Open and New start in its
`cars/` or `tracks/`, and the start screen lists what is in them. It is optional: with none, the folder dialogs start where the system puts them.

The project folder is created with `project.ini` and `trace.json` when they are missing: a straight as long as the shortest stage the game races (1308 m, 54
segments of 4 nodes: `track_trace_stage_min_length()`, from the game's own `RACE_STAGE_END` and `RACE_GRID_FRONT`), 15 points 93.4 m apart, so a new project
has nothing red.

| File | What it holds |
|------|---------------|
| `project.ini` | `id`, `name`, `author`, `stem` (1..7 of A-Z 0-9, not AL/CL/CY/TR first), `art` (a FAM: relative to the project folder, or absolute), `redistributable` (the author's word on the art's licence) |
| `trace.json` | `width` (the road's width byte, 8..120; the template's is 40, see Road width) and `points`, the control points as `[x, z]` in metres (one track unit is one metre), and optional `version` (2; 3 when a key has a bank or a side value) and `keys`, the road keys (see Road keys). A file with no `version` or `1` has no keys; a newer one is refused |
| `build/<id>.zip` | written by Build; the only thing the game installs |

## Keys

| Input | Action |
|-------|--------|
| left click on a point | select it alone (a click on empty space clears the selection) |
| Shift+click, Cmd+click (Ctrl elsewhere) | select a range from the last clicked point; add or take away one point (the Points rows take the same clicks) |
| left drag on empty space | box: select the points inside it (with Shift, add them) |
| left drag on a point | move it; on a selected point, move all the selected points (one undo step) |
| right or middle drag, wheel | pan, zoom (wheel zooms about the cursor) |
| arrows | nudge the selected points by 1 unit |
| `[` `]` | rotate the selected points 1° about the first of them (Shift: 5°) |
| Cmd+C, Cmd+V (Ctrl elsewhere) | copy the selected run of points with its road profile; paste it after the last selected point |
| `N` | add a point after the last one, along the last segment |
| `I` | insert a point after the selected one |
| `Del` or `X` | remove the selected points (a trace keeps 2: removing more is refused, the status line says why) |
| `F` | fit the view to the trace |
| `S` | save `trace.json` |
| `B` | build `build/<id>.zip` |
| `U` | install the last built zip (runs the game: `--game`, `--user`) |
| `Esc` | clear the selection; with nothing selected, quit |

### Several points selected

With more than one point selected, the Points panel has a row «Rotate points [ 0.0° ] about point N»: drag or type an angle and the selected
points turn about the first of them, clockwise as the view shows it (the view does not turn); the field goes back to 0 after each edit, and each
edit is one undo step. The Node window shows how many are selected and how long the stretch is along the road. The Road profile shows each value
the selected points share, or «Mixed», and is read-only until one point is selected. A group move that breaks a limit shows in Problems at once,
as a single point's move does. The X and Z cells of the Points table still edit only their own row.

A selected run of points (no gaps, two or more) has three more operations in the Points panel, and Copy and Paste on the keys; each is one undo step:

- **Copy / Paste** (Cmd+C, Cmd+V): Paste puts the copied points in after the last selected point (the road's last when none is selected). The
  block's first point lands on that point, the block turns to go on along the road's heading there, and the points after it follow it rigidly.
  The road profile comes with it, and the road after the block keeps its own. The pasted run, that point included, becomes the selection.
- **Mirror L/R**: reflects the run about the road's heading at its first point. **Mirror F/B**: about the line square to it there (the run folds
  back). Both swap the profile's sides: shoulders, barriers, fences, surfaces, lanes 1+2 and 2+1, the bank's sign.
- **Reverse**: the run is travelled the other way. With all points selected, the road is driven the other way and nothing moves. A stretch in
  the middle is turned round and put back so its new first point sits where the old one was, going on the same way: no jump.

The points after a run that moved follow it, keeping the turn they had where they joined it. The profile values just outside the run keep
theirs; only a ramp running into the run from a key further out takes a new shape.

**Half a circuit**: build the first half from its start to the far side, both ends square to the axis the circuit is symmetric about (the first
and last segments run straight across it). Select it all, Cmd+C, Cmd+V, then Mirror L/R and Reverse on the pasted run: the second half is the
first's mirror image and ends exactly on the start. With ends that turn, it misses. Dragged within a few pixels of the start, the last point
lands on it; the Node window (last point selected) and the drag's readout say how far the end is from the start, and near it what a circuit
would need: the game takes 512 or 1024 nodes for one. The editor builds stages only: a closed circuit is not built yet.

While a point is dragged, a readout by the cursor gives its distance from the point before, the turn there and the radius of that turn.

### Add curve and Add straight

The rows sit in a folding block, **Add curve / straight**: it starts folded when the Points table would be left under 4 rows (the default layout) and open when there is room; a click on its header keeps what you chose.

The Points panel's **Add curve** row takes the direction (R or L, as the view shows it), the angle it turns in all (up to 180°), the radius in
metres and how many points it has (the default keeps each step under 40 m and 15°); **Add straight** takes a length and a point count. While a
row's fields are hovered or edited, the Viewport draws what Accept would make: the new stretch in grey (amber under 43 m of radius, red under
20.9 m, where Build refuses it) and the road after it where it would go, and the line under the rows says the length, the point count, the point
it goes after and, under a limit, the limit in Build's words. **Accept** puts it in after the last selected point (the road's last when none is
selected), going on along the road's heading there: every point lies on the arc, the points after it follow rigidly, the stretch takes the road
profile the road has at that point and the road after keeps its own. The new points become the selection; one undo takes it all back. To change
a curve, select its stretch, remove it and add the new one.

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
In the first layout (and after Window > Reset layout) the right column is as wide as all the Points columns need and the Road profile as wide as its labels and
both sides' choices need, while the Viewport keeps the larger part (at 960x600 Limits gets the least). The Project window keeps each path on one line: a long one
shows its start and its end with "..." between, the whole path in its tooltip.

Both also say in red, above the rest, when the road is too short for a stage: a stage's race ends 181 nodes (about 1.1 km) before the road's last
node and the grid's front row starts at node 32, so a stage needs at least 214 nodes (about 1.29 km) of road for its finish to lie past the front row;
under that the race ends at its first tick with the car standing. The line gives the road's length and what it needs ("too short for a stage: 1.13 km
of road (188 nodes); a stage needs 1.29 km (214 nodes): ..."). The numbers come from the game's own constants (`core/formats/race_stage.h`, which the
race reads too). A new project is born just long enough (see above): shortening it brings the line back.

## Start screen, cars, and projects: New, Open, recent, and unsaved changes

Studio is **two rooms behind one hub**: Cars and Tracks (the Track light window is in Tracks). It has one project open, a car in the Cars room or a track in
the Tracks room. With nothing to reopen it shows the **main menu**, the hub, with three doors side by side: **Edit car mod** (New car..., Open car...), **Edit track mod** (New track..., Open track..., New track from an original...)
and **Edit tweak** (Edit tweak...: the game's own cars and tracks, see below). Under each door are the projects of the workspace of that kind (if one is set) and **Recent** of that
kind (up to eight in all; a folder that is gone is greyed). A normal run with no folder given reopens the project it had last. Each kind keeps its own docked layout
in the layout file.

Inside a room, the **File** menu holds that room's actions only, New before Open: in Tracks New track..., Open track..., New track from an original..., Open recent (tracks);
in Cars New car..., Open car..., Open recent (cars); **Settings...** in both (the game, the workspace and the game repo's folder); in both Save, **Main menu** (Cmd+W on a Mac, Ctrl+W elsewhere) and Quit. Main menu
closes the project and goes back to the hub, asking once about unsaved changes; the recent list and the workspace stay. The workspace is chosen on the hub only (its File menu and the row at the top, which shows the whole path, cut in the middle when it is long, with all of it on hover). **Window** lists the room's panels, each a
toggle: ticked when the panel is open, a click closes it or opens it again where it was (kept between runs; Reset layout opens them all).

With a workspace set, New track asks only for a **name**: the id is made from it (lower case, a-z, 0-9 and '-'; shown, and you may type over it), the project is
made in `<workspace>/tracks/<id>` and opened, and a folder that already exists is refused in words with nothing written. With no workspace the window's folder dialog
is used as before.

Open track wants a folder that holds a project (a `project.ini`); New track wants one that holds none, and makes the project there (the window's folder dialog
can make the folder). The last eight project folders are kept in the editor's own folder (`recent.txt` beside its layout; a car's line starts with `car` and a tab,
and a file from the first Studio, all tracks, still reads). The view fits the project that was opened.

**Open car** takes a car kit's folder (the one that holds `coche/` and `modelo/`) or its `coche/`; given `coche/` with a `modelo/` beside it, the kit is opened.
What makes it a car: a `mod.ini` with `type=car` and a `stem=`, and the car's file `SIMDATA/CARFAMS/<STEM>.CFM` beside it. `car_masks/<STEM>/` may be missing (listed as
none). A folder that is not a car is refused with what was looked at and what is missing. The car window shows and edits the card (`mod.ini`): version, year, power, weight, make, model, country, category, author, description, ABS and traction
control (yes, no, or the default, which removes the line), sound from (the game's eight original cars) and the colors (add, remove, move up or down, name, and a
color picker; up to 16). Each field is checked by the game's own reader (`mod_ini_parse`, car_piece's words) and a field with a problem is red, with what is wrong
and its number ("Year: 17 characters; the card keeps up to 16"); **Save is blocked while any field has a problem**. Undo and Redo (the Edit menu,
Cmd+Z and Cmd+Shift+Z on a Mac, Ctrl+Z and Ctrl+Shift+Z elsewhere; every shortcut takes the platform's key) take back typing in a field as one step. **Save writes only `coche/mod.ini`**, through `mod_ini_set`: only the lines you changed,
every other byte (comments, order, line ends) stays, and a Save with no change writes nothing. A card with unsaved changes asks before it is left, as a track does.
The textures (`modelo/c1`, and how many far ones) and their mask files are listed in the Masks tab. `--set KEY=VALUE` sets a field before the first frame (headless
runs). The car window has no Save, Undo or Redo buttons: File > Save and Edit > Undo / Redo (and Cmd/Ctrl+S, Z) do them. Build (a kit) makes the car again from the kit's
files; the old Rehacer, which ran a kit's own `rehacer.command`, is gone. When an Install ends and the game says the files changed but the version did not, the status
line says to raise Version in the Car tab (and to which).
The same folder on the command line opens as a track when it has a `project.ini`, as a car
when it holds a car's `mod.ini`, else as a new track project.

**Edit tweak** > **Car** > **Open** (and the headless `--export-car`) makes a project from one of the game's cars, to adjust its masks, and its tweak folder (see `originals-workflow.md`). It reads the player's own
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
  opens with the Car window (read only: what it is) and the Masks window only; there is no card and no Save.

The masks are the game's `data/car_masks/<folder>/` copied as they are (same names, same bytes, so the same 8-bit grey PNGs and sizes; the old `*.before-*_mask.png`
copies, which the game does not read, are left out). The Masks window's **Show PSD folder** shows the folder in the Finder, and says what it maps to in the
game's source tree (`data/car_masks/<folder>/`): the way to change a game mask is to copy the files there. Everything made this way says "Made from the game's
files: for your own use, do not share" and has `redistributable=0`. `--export-car FOLDER --into PARENT` does it before the first frame (headless), `--ask export`
shows the window and `--pick FOLDER` chooses a car in it.

**Test drive and In-game photo** (the Drive window, for a project of one of the game's own cars: the eight from Edit tweak, and the traffic and police ones).
Both run the game (the one Studio found; Locate game if it is not there) on a scratch player folder made for the run (`studio-race-XXXXXX` in the temporary folder,
Restomod whole, its look keys written out, no extra liveries, removed when the run ends): the player's own folder and the installed game are never touched. The project's masks are
in front of the game's own (`TNFS_CAR_MASKS=<project>/car_masks`, and `TNFS_DASH_MASKS` for a car you drive); the person's own `TNFS_*` variables are dropped.
**Day** or **Night** (Night is `light_time=night` and `light_effects=1`), and a track from the list (default Rusty Springs, the night reference; Lost Vegas has no
time of day). **Look**, under the buttons and open (it folds by hand), has Reflections, Car colours, Soft shadow and Texture filter (all on: Restomod's) and the resolution, 1280 x 960
(render scale 2, the default) or 640 x 480; they are the session's, never saved in the project, and the header's title and the run's summary say what is off
("Look (Reflections off, 640x480)"). The window's explanations are behind the (?) beside the buttons. A car you drive is raced alone, a time trial (`--car N --racers 1`
for a photo, `--car N --type 0` for the window; N is the car's model for a photo, and the car menu's number for the window, whose race is
Control Central's: the game turns that number into the model by its own table, 00426620). A traffic or police car is raced in a race with traffic (`--type 1`), which
runs only on the City, Coastal and Alpine stages (the circuits are greyed for it; default Coastal 1); the player drives the Toyota Supra Turbo, and the window says so
and where the tested car is: the window's traffic is the game's own draw, so a given traffic car is on the road in some races and not in others. **Test drive** opens the game's window; **Stop** ends it. **In-game photo** runs
the game headless and shows one frame in the window next to the masks (200 ticks in for a car you drive, 800 for a traffic car, 250 for the police car). For a
traffic or police car the camera has to follow the slot the car has in the race (`TNFS_FOLLOW_CAR`): the traffic's models are drawn by the race's seed, so a
probe run (1 tick) per seed reads the game's `race car: slot N is STEM` lines and Studio tries the game's own seed and then 24 more (`TNFS_DRIVE_SEED`) until the car
is in the draw; if none has it there is no photo, and the window says so. A traffic car waits parked high above the track until the player comes near, so the shot asks
the game to wait until the car has been on the road 64 ticks (`TNFS_FOLLOW_SETTLE`, past the 800); if it stays parked in a draw the next draw is tried (4 such draws at most), and the
summary says the tick of the frame. The police car is put 15 nodes behind the player, chasing (`TNFS_POLICE_NEAR`), so its camera
has it at any tick. Studio cannot see whether the car is on screen: look at the picture. A game older than this Studio does not log the slots: the photo says so, and
Test drive still runs. The game's log (its output and its own `spirited-drive.log`) is in the window, errors red, the masks it took green; the game looks for every
car's mask in the override first and says "cannot read" for the cars the project has none for: those lines are dim, they are expected. `--race-photo` takes the
photo before the first frame (with `--night`, `--track STEM`, `--race-png FILE`), headless runs wait for it.

Closing the window, Escape and Quit with changes that are not in `trace.json` (a point moved, added or removed, or the road width changed since the last
save or open) ask "Save the changes to <project> before quitting?" with **Save** (Enter), **Discard** and **Cancel** (Escape). New, Open and Open recent ask
the same before they go on (the folder is asked for after the answer). A save that fails keeps the question open with the reason. Build and Install
use what is on screen, not the file, as before.

## Original tracks: light and reflective masks

The game's 16 tracks are projects of the workspace, put there when their tweak is first opened (**Edit tweak...** > **Track** > **Open**, from the game's `data/`, found through File > Settings... or `--port-data DIR`, and `--art-dir DIR` for the owner's
mask PSDs; headless, `--steps add-originals` or `tweak-open-row:<TRACK>`; an older workspace's `tracks/<TRACK>/` is copied into the tweak by Open, once, and left as it was; what you edited is never touched). Each is `<workspace>/tweaks/<track>-tweak/studio/project/` (never in a zip): `original.ini` (what marks it), `light/` (the track's light files, the layout the Track light window works on), `art-source/masks/` (the
owner's mask PSDs, copied, never moved; backups with a second dot in the name are skipped) and `art-generated/track_masks/`. Nothing that is already there is overwritten. The **Track** tab of **Edit tweak** lists all 16.

An original track opens in the Track light window on its own `light/` folder with the track fixed (its time of day still changes), and a **Track masks** panel beside the comparison: a row for each texture with a mask, its
size, the texels it marks and its state, and the picked mask over the game's texture (read from the game's files; without them the mask is shown alone). Above the list: **Show PSD folder** and **Show masks folder** (in the Finder), **Update masks from PSD** (makes again the PNGs of the PSDs newer than them; a PNG edited by hand is asked about first: Replace / Keep / Cancel) and **Re-scan** (reads the folders again). Below it, **Changes from the game's masks** lists the PNGs that differ from the game repo's `data/track_masks` (when the repo's data/ is set), with **Copy list of changed files to clipboard**. The PSDs make the PNGs the game reads, in C (the rule of
`tools/track_mask_from_psd.py`: the layer named `mask`, alpha 128 or more and not black is reflective, yellow is a lamp's mark; grey PNG without lamps, RGBA with), when the track opens and when a PSD is saved
again. States: **ok**, **new**, **stale**, **edited by hand** (the PNG differs from the PSD's and is newer: kept as it is until *Update this mask from its PSD* (Replace); the row counts the PNG's texels, what the game uses),
**PNG is the source** (a PNG with no PSD), **no mask painted in the PSD** (an empty mask layer is never a source: a PNG next to it is kept, and none is made from it), **unreadable PSD**.

Textures whose PSD has no mask painted and no PNG (all of TR2, TR5 and TR7 at first) are not listed with the rest: they sit in one closed line at the bottom of the table, **Not painted yet (N)**, open on click. `tools/track_starter_psds.py <track_mask_draft> <original> <workspace tracks dir>` makes the starting PSD of every texture of TR2, TR5 and TR7 (the texture, an empty `mask` layer and a hidden `draft` guess); it never overwrites a PSD that is there, and needs Pillow.

The **photo** of an original track shows its workspace's masks: the PNGs of `art-generated/track_masks` go in front of the game's own (`TNFS_TRACK_MASKS`, a copy in the photo's scratch folder), so a retouched PSD shows in the
next photo (and in the comparison pictures).

**Apply to game repo...** (the **Tweak** panel of an original track or car) needs the game repo's `data/`: File > Settings... (**Game repo** > Choose...; kept as `repo data<TAB><path to the repo>/data` in `workspace.txt`, beside the recent
list), or `--port-data DIR` for one run; the first Apply asks for it. It lists what would change in that `data/` (each light file by lines, each mask PNG by texels gained and lost, compared as marks, each file marked same, changed or new) and, on **Apply**, writes the changed and new ones into
`data/track_light` and `data/track_masks/<TRACK>/` (for a car, `car_masks`, `dash_masks` and the colors into `core/formats/car_paint.c`). A file the repo has changed by itself is listed apart: **Replace**, **Skip** or **Cancel**. It is all or nothing, and it never commits or pushes. `tools/track_mask_fill_psd.py <workspace>/tracks data/track_masks` is the one-time migration step for PSDs whose mask
layer is empty: it fills the layer of the workspace copy from the PNG the game uses today (other layers byte for byte); `--steps check-originals` compares every original's generated PNGs with `data/track_masks`. Steps: `add-originals`, `tweak-list`, `tweak-open-row:NAME`, `tweak-make`, `tweak-status`, `tweak-check`, `tweak-apply:ID`, `settings-open`,
`open-original:I`, `masks-print`, `masks-poll`, `mask-select:I`, `mask-regen`, `repo-ask`, `repo-copy`, `print-repo`, `print-original`, `check-originals`.

## Metres and the Node window

One track unit is one metre (`TRACE_METRES_PER_UNIT` in `core/formats/track_trace.h` says where that comes from), and the editor writes every distance in metres.
The Points table has a `width (m)` column (see Road keys) and two read-only columns beside x and z: `dist m`, the distance along the curve from the start to the point, and `turn`, the turn at the
node nearest the point ("R 5.3°" to the right, "L 3.1°" to the left). Hovering a node in the Viewport shows its number, its distance from the start and its
turn. The **Node** window (under Points) shows the selected node, picked by a click in the Viewport, in Points or in Problems: its number, x and z, the
distance from the start, the turn, the distance to the node before and to the next one, and the limits it breaks. With none it says "Select a node".
The distance between nodes is not the same on every road: the editor spaces them evenly along the curve and fits a whole number of segments (4 nodes
each) to its length, so it is about 6.03 m on a long road and can be a few per cent more or less on a short one (the CD's is about 6.0 m).
Height and slope are not shown: the editor has no height yet. The bank is (see Road keys: the bank).

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

### Shoulder surface

The **Road profile** window (beside Limits and Problems, so Node stays in sight; its header says "Road profile at point N") sets, for the selected point
and each side (Left | Right), the shoulder's surface from there on: **Asphalt** (grips as the road does) or **Off-road** (the Track window's Verge material:
the player's car loses grip; the rivals feel neither). A value the point inherits shows dimmed; right-click a set one and choose "Inherit again". (In
`trace.json` and in the code Asphalt is `tarmac` and Off-road `verge`.) An asphalt shoulder is drawn with asphalt in the game (bank 4 of Studio's
`GENERIC.FAM`, the TRI's shoulder band of each segment) and, in the Viewport, as an asphalt-grey line beside the road at its edge; a node's tip says
"shoulder: asphalt left/right", and the Node window "shoulder surface: left asphalt, right off-road". **A side changes at a segment's start** (every 4 nodes, 25 m): the game's texture is per segment, so the grip (the node's
surface) changes at the first node of the segment where the key's point falls, not at the point itself, and picture and grip always agree.

When the project's art is not Studio's current generic copy (`art=` of another name, or a `GENERIC.FAM` the author edited) Problems says that an asphalt
shoulder draws with that art's bank 4 and should be checked in the game; the art is not touched. An untouched copy from an older Studio is replaced
by the current one when the project opens (`editor/art/README.md`). In `trace.json` a key carries `"shoulder_l_surface": "tarmac"` and/or `"shoulder_r_surface": "verge"`,
and the file is `"version": 3` when any key sets a surface (2 otherwise).

A key whose point does not exist, twice on a point, or with a width outside 2..30 m refuses the file with a message naming it. Unknown fields are ignored. Lanes
per key are not read yet; their place is reserved in the key (`TRACE_KEY_*` bits in `core/formats/track_trace.h`).

## Road keys: the bank

The Points table's `bank (°)` column banks the road at a point, the same way as the width: drag or type a value (-60..60, a tenth of a degree; one pixel is
0.25°) and the point has a key; a point with none shows, dimmed, the bank it inherits: level before the first key, a smoothstep of the distance along the
curve between two keys, the last key's after it. Right-click "Inherit again (clear this bank)" clears it. Positive banks the road for a right-hand curve
(the outside edge up), as the CD's tracks do; the Node window says "bank: R 30.0°" or "L ...", or "level".

The limits (notes/dep59-banking-spike.md), in Problems, the Limits summary, the Viewport and the column's colour:

- **amber over 25°**: built. The CD banks over 25° only in short stretches of three tracks, and the game measures the road's edge in plan, so at 25° the edge is
  about 10 % off (13 % at 30°, 18 % at 35°): on a banked curve the car can run over the inside line. Drive it.
- **red over 35°**: the track is not built, as with a red curve; Build refuses it naming the node. 35° is the CD's own most (TR4 banks 34.5°), and 35 itself is
  amber. The generator takes up to 35° on the editor's nodes (`TRACK_GEN_BANK_MAX_DEGREES`); a `track new` layout still 20°.

In `trace.json` the bank is `bank_deg` on a key, a field of version 3: a file with a bank is written as version 3, and a version 2 file's `bank_deg` is
not read. A value that is not a number, or outside -60..60, refuses the file naming the point:

```json
{ "version": 3, "width": 40, "points": [[0,0],[0,60],[0,120],[0,180],[0,240]],
  "keys": [ {"point": 1, "bank_deg": 0}, {"point": 2, "bank_deg": 30.0}, {"point": 4, "bank_deg": 0} ] }
```

## Test drive: the Drive window

The **Drive** window (a tab beside Limits and Problems) runs the game on the open track. **Test drive** builds the piece (as Build does), installs it into a
scratch player folder of its own (never yours: your player folder and its `mods/` are not touched), and starts the game in its window on the piece as a time
trial. **Stop**, or closing the game's window, ends it; the scratch folder goes with it.

- **Car**: the game's car menu, in its order; the first is the default, and the last one picked is kept in the editor's settings (`track-editor.ini`,
  `[StudioSettings][Editor] DriveCar=N`, the menu's number).
- **Look**: the same switches and resolution as the car's Drive window (`race_look_controls`, `editor/race_look_ui.cpp`), all on by default.
- A track with a red problem, or too short for a stage, is not run: the window says the Problems' text. No game found says what to do, as Install does.
- The install runs in the background: the state line says «Installing...» and the editor goes on; then the game starts. **Install (U)** in the Project
  window does the same (an «Installing...» line with a **Stop**); while either installs, Build waits (the installer reads the zip) and nothing else installs.
- The state line says what the game's own log says of the piece while it runs (`track piece: <id> is track N`, or why it refused it). The game's log is
  under a folded header.
- Not in this version: a in-game photo at the selected point.

Headless: `--test-drive` does the same and waits (60 s at most) for the game's line on the piece, prints it, then stops the game. `--race-off` and
`--race-scale` set the look, as for `--race-photo`; `--cd DIR` names the game's files.

## Preview

The **Preview** window (a tab beside Masks and Drive) shows each texture of the car file as the race has it: the game's own code (the fixed black of the masks, then the paint) runs on a copy of the car file with this project's masks in front of the game's, and an unsaved mask edit counts as if it were saved. Pick the paint from the card's `paints=` (a car you drive; a traffic or police car has none: the race does not paint it). It updates by itself when a mask is edited or saved, a paint or its colour changes, (not while a pencil stroke is going).

The bank's palette is taken to 6 bits a channel, as the race loads it. Not applied: the track's light and the screen palette's nearest-colour snapping. Instead Preview counts the distinct colours of the body and says how many the car's entries in the race palette (23 of its own, 72 shared when several cars race) cannot hold: the rest will look like their nearest. Texels that name one of the race's screen entries (0xfa-0xfe) are hatched, and 0xff is a chequer. Headless: `--paint N` shows the card's N-th paint.

## Changes from the game's masks

At the top of the **Masks** window of a project made from one of the game's cars (Edit tweak > Car), **Changes from the game's masks** compares each `*_mask.png` of `car_masks/<folder>/` with the game's own copy, `data/car_masks/<folder>/`, the one the export started from: *unchanged*, *changed* (how many texels, and which classes gained and lost them), *new* (only the project has it) or *missing* (only the game has it). Masks are compared as classes, so a file written again by another picture editor with the same texels is unchanged; backups with an extra dot in the name (`frnt.before-1650_mask.png`) are left out, as the export leaves them. **Refresh** reads the files again; **Copy the changed list** puts plain text on the clipboard (the folder, then a line for each changed file with its texel count and classes, each new and each missing file, and how many are unchanged) for pasting into a hand-over. Read only. Headless: `--changes` prints the same text.

## Masks window: the list and the mask view

The **Masks** window (a tab beside Drive and Preview) is the one place for a car's masks, and it never paints them: masks are made in an external editor (the PSD, or the PNG of a texture that has no PSD), and the window shows the result and reloads when the file changes. From the top: the project's changes from the game's masks and Update masks from PSD / Check dash (below), then **a row for each texture of the car file** with its size and the state of its mask, then the picked mask over its texture.

- The state is *in sync* (the PNG is as new as its PSD), *PSD newer* (the PSD was painted after the PNG: Update masks from PSD makes it again), *PSD found, no mask yet*, *PNG only* (no PSD found to compare with, which today means any car that is not one of the game's: the PNG is the source) or *no mask* (a texture the car has no mask for: its row is greyed).
- **Update masks from PSD** reads the car's PSDs with Studio's own reader (nothing to install: no python, no psd-tools) and writes the mask PNGs, the dash's too when the folder has a `dash.psd`; it replaces the masks only when every PSD was read, and names the PSD that could not be read otherwise. A `.psd` with a second dot in its name (a backup) is not a PSD of the car. The PSDs are never written.
- Click a row: the mask is drawn over the texture, each class in its legend colour, with an Overlay switch, an Opacity slider and a switch per class. Zoom with the slider, the keys + - 0 or the wheel; move with the right or middle button.
- A mask whose size is not its texture's says both sizes: the game scales the mask to the texture (nearest neighbour), so the Studio shows it scaled the same way.
- **In-game photo** runs the game on the masks and shows the picture in the Drive window.
- Studio never writes a mask file from this window.

Headless: `--mask NAME` (`side`, `frnt_lo`) opens a mask; with `--shot` the picture is the window. (`--mask-rect` and `--mask-save` are gone.)

## Headless run

For a smoke run (no window events, no sound):

```sh
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_RENDER_DRIVER=software timeout -k 10 60 ./build/native/tnfs-track-editor PROJECT \
    --frames 3 --save --build --shot out.png
```

`--install --game ./build/native/tnfs --user SCRATCH` runs the game's installer into a scratch player folder; `--test-drive --game ./build/native/tnfs --cd original`
runs Test drive (see Test drive). `--select N` selects control point N first
(for a picture of the Node window). `--edit` adds a point first (something unsaved) and `--ask quit|new|open` asks for that action, to show the question
of the unsaved changes.

## What is missing

- The INFO is generic: lines 9 and 10 (the sky colours) are a plain blue and the other lines zero. Not checked against the game's sky.
- Heights are not read from `trace.json` yet (every node is level); the width and the bank are per road key (see Road keys).
- The install was not run end to end in this version: it invokes the game's `--install`, which needs a built game.
- The 3D free-flight view, lighting and scenery editing are later phases.

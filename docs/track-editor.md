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

The project folder is created with `project.ini` and `trace.json` (a straight of six points, 200 units) when they are missing.

| File | What it holds |
|------|---------------|
| `project.ini` | `id`, `name`, `author`, `stem` (1..7 of A-Z 0-9, not AL/CL/CY/TR first), `art` (a FAM: relative to the project folder, or absolute), `redistributable` (the author's word on the art's licence) |
| `trace.json` | `width` (the verge byte, 8..120; the template's is 40) and `points`, the control points as `[x, z]` in units |
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

The road is drawn through the control points as a Catmull-Rom curve, resampled to the CD's node spacing (about 6 units). Up to 8 degrees a node
(a radius of 43 units) nothing is marked. Over that a node is drawn **amber**: the road is built, but the curve is tighter than almost all of the
CD's (the CD has some of 16 degrees: Alpine 3), so check the terrain in the game; the AI brakes there. A node is drawn **red** when it turns more
than 16.5 degrees from the node before (the CD's own most is 16.2), or the curve there is tighter than 20.9 units: Build refuses while any node is red. In a tight curve the
generator holds the terrain on the inside of the curve to 0.4 of its radius, as the CD does (`notes/dep59-editor-limits-vs-cd.md`); the outside is left as it is
(the CD cuts both sides, but its scenery hides the outer hole and the generic template has none). A
layout of `track new` does not turn the terrain and keeps its own limit of 8 degrees. The numbers are measured and provisional, not limits of the game.

How tight a curve is depends on how far apart its points are: two points close together make the curve turn within a short distance. Hover a red or amber
stretch to see it; the fix is to space the points out or to smooth them.

The **Limits** window (a tab beside Problems at the bottom) shows the node count against the 2397 a TRI holds (599 segments and the closing node), the
length and, in red or amber, a line for each rule some node breaks ("2 nodes turn more than 16.5° per node: the track can't be built", "26 nodes turn
more than 8° per node: they may not look right in the game"; the same for the radius). A rule no node breaks has no line. The **Problems** window lists
the nodes themselves, one row each, red first and then by node number: "node 80: turns 9.4° (more than 8°) and radius 38 units (under 43 units), may
not look right". A click on a row selects the nearest point and centres the Viewport on that node. With none it says "No problems". Both windows are
dockable like the others and their layout is kept.

## Headless run

For a smoke run (no window events, no sound):

```sh
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_RENDER_DRIVER=software timeout -k 10 60 ./build/native/tnfs-track-editor PROJECT \
    --frames 3 --save --build --shot out.png
```

`--install --game ./build/native/tnfs --user SCRATCH` runs the game's installer into a scratch player folder.

## What is missing

- The INFO is generic: lines 9 and 10 (the sky colours) are a plain blue and the other lines zero. Not checked against the game's sky.
- Heights, banks and per-point widths are not read from `trace.json` yet (every node is level, unbanked, of the same width).
- The install was not run end to end in this version: it invokes the game's `--install`, which needs a built game.
- The 3D free-flight view, lighting and scenery editing are later phases.

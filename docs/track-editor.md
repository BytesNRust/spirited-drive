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

The road is drawn through the control points as a Catmull-Rom curve, resampled to the CD's node spacing (about 6 units). A node drawn red breaks
a limit: it turns more than 8 degrees from the node before, or the curve there is tighter than 43 units. Nothing under those is marked. Build
refuses while any node is red, and the generator (`track new`) refuses the same 8 degrees. The limit is where the generic template's terrain starts
to break in the game (at 10 degrees the terrain of the inner side crosses itself, at 16 the road's texture smears; measured by racing generated
pieces), not a limit of the game: the CD has curves of 16 degrees.

How tight a curve is depends on how far apart its points are: two points close together make the curve turn within a short distance. Hover a red
stretch to see it; the fix is to space the points out or to smooth them. The Limits panel shows the node count against the 2397 a TRI holds (599
segments and the closing node), the length and the first limit broken.

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

# Modding guide

How to change the game's data without touching the CD: where files live, what a mod can replace, and the file formats of the things the port added (track light, car masks, dash masks, track masks, sounds). The settings themselves are in [settings.md](settings.md); the track light format has its own page, [track-light.md](track-light.md).

## The three folders

| Folder | What it is | Written by the game? |
|--------|------------|----------------------|
| **Game data** (`--data DIR`, or the player's copy `<player folder>/gamedata/`, or `./original` in a development build) | The original CD's files: `SIMDATA/`, `GAMEDATA/`, `FRONTEND/`, the `NFS_WIN.EXE`. Read only. | Never |
| **The port's data** (`data/` beside the program; in a source tree `data/` at the repository root) | What the port ships: `car_masks/`, `dash_masks/`, `track_masks/`, `track/`, `track_light/`, `sounds/`, the car environment map. Looked up as `<mod>/...` first (see below), then `<program folder>/data/...`, then `./data/...`, then (development builds) the source tree's `data/...`. | Never |
| **The player's folder** | `port.cfg`, `stock.cfg`, `restored.cfg`, `restomod.cfg`, `controls.cfg`, `extras.cfg`, `records.dat`, `config.dat`, `Replay.rpl`, saved games, `spirited-drive.log`, `gamedata/`, `mods/`. | Yes: all of it |
| **A mod** (`--mods DIR`, else `<player folder>/mods/`) | A folder of files that replace the game's. One overlay. | Never |

The player's folder is `~/Library/Application Support/Spirited Drive/` on macOS, `%APPDATA%\Spirited Drive\` on Windows and `~/.local/share/Spirited Drive/` on Linux. `--user DIR` puts it at `DIR` for that run (a portable install, a test: `tnfs --user ./user`); `TNFS_PREF_DIR=DIR` does the same and is what the test suite uses. `--mods` has no say in where the player's files go. The EXTRAS page's *Open folder* shows the player's folder; `spirited-drive.log` in it is what to send with a bug report.

If you used an older build: what `TNFS Restomod/` or `tnfs/tnfs/` (the old folder names, the newer first) or `<mods>/user/` held moves to the new folder the first time, one line in `spirited-drive.log` says so, and a file that already exists in the new place is kept (the old copy stays where it was).

## How a mod replaces a file

Run with `--mods DIR` (or leave the files in `<player folder>/mods/`). When the game asks for a file of the game data, say `SIMDATA/MISC/TR1.TRI`, it looks in the mod first, at the same path (`DIR/SIMDATA/MISC/TR1.TRI`, or the upper-case form of it), and then in the game data. A file the mod does not have comes from the game data, so a mod holds only what it changes.

* The mod's file is checked against what the original executable accepts (`tnfs-studio` has the same checks). A file that fails is **not** given to the game and the run says which file and why (`platform_mods_error`, also in `spirited-drive.log`).
* Names that leave the mods folder (`..`, absolute paths, symbolic links out of it) are refused.
* The mod is never written: saves, replays and settings go to the player's folder.
* The mods folder must exist when it is given with `--mods` (a missing one is an error); the default `<player folder>/mods/` is used only when it is there.

Example, a changed track:

    mymod/SIMDATA/MISC/TR1.TRI
    tnfs --mods mymod --track TR1 --node 120 --screenshot out.ppm

[modding/track-layout.md](modding/track-layout.md) and [modding/limitations.md](modding/limitations.md) cover building tracks and what the engine cannot do; `tnfs-studio slot replace` puts a track or a car into a mod's folder with the checks.

The port's own files (track light, car masks, dash masks, track masks, sounds, the car environment map) go through the same overlay: **a mod's `<mod>/<path>` is tried first, where `<path>` is the path under `data/`** (`track_light/CY1N.txt`, `track/CY1.txt`, `car_masks/<CAR>/<tex>_mask.png`, `dash_masks/<CAR>/dash_mask.png`, `track_masks/<TRACK>/<name>.png`, `sounds/thunder.wav`, `car_envmap.png`), then the port's own `data/`. The `TNFS_CAR_MASKS` and `TNFS_DASH_MASKS` variables, for trying a folder, still win over both.

## Track light

`data/track_light/<TRACK><A|B|C|N>.txt`, one file for each track and time of day (A dawn, B day, C dusk, N night). In Restomod '26, with no PS1 disc, the light of a track is this file, whole. A mod can carry its own copy: **`<mod>/track_light/<file>` is tried first**, then `data/track_light/<file>`. A file is a few lines per run of nodes (`rows` the light rows, tunnel and lamp lines, `brightness N N P` to dim or lift a stretch, `car_tone` the car's tone). Stock '96 and Restored '96 never read these files.

    # a stretch of the night track at 70 percent of its light, then the car warmer there
    brightness 120 180 70
    car_tone 120 180 ff8020:60

The **live mode** `light_edit=1` in `port.cfg` (or `TNFS_LIGHT_EDIT=1` for one run) shows in a race the node the car is at, which file it reads and the values there, and the texture under the mouse pointer (`aim (+): road texture slot_38`: the mask to paint for a lamp you point at). **F10** holds the race where it is, **F11** copies a `brightness N N 100` line to the clipboard, and saving the light file or the track's places file reloads the light (and where it rains) within a second without leaving the race; `snow` lines want a restart.

The keywords are English: `track time nodes flags` (the header), `rows`, `present`, `pool car_pool`, `tunnel` (a zone: `tunnel a b [light] [colour | lit]`), `tunnel_tone <light> [colour]`, `brightness`, `tone`, `car_tone`; [track-light.md](track-light.md) is the reference. Files written with the first, Spanish words (`pista hora nodos banderas filas tunel pozo brillo tono_coche`...) still load: the port reads each as its English word. The generator is `tools/export_track_light.sh`: **the export writes only missing files; `FORCE=1` rewrites them and loses your edits** (an existing file is kept, with a "kept <file> (exists; FORCE=1 to overwrite)" line). A retouch can live in the shipped file or in a mod's folder (the mod's copy goes first).

### Track places

`data/track/<TRACK>.txt` holds the physical facts of a track, the same at every time of day, and is the home for other per-track data later: a header `track AL1`, then `tunnel a b`, `canyon a b` and `car_dim a b` lines (and `present <key>` for an array that is there but empty), `snow 1` (the track's rain is snow and its rally is on snow) and `snow_map FROM TO` (the rally's road art: texture slot FROM is drawn with slot TO's bitmaps; see track-light.md's Snow); a word the format does not know is a bad line. It goes through the same overlay as the light files: **`<mod>/track/<TRACK>.txt` is tried first**, then `data/track/<TRACK>.txt`. The time's light file (`track_light/<TRACK><A|B|C|N>.txt`) adds to it: its `tunnel a b lit` says a stretch is a hall, and its `tunnel_tone` the tunnels' light and colour at that time. A track with no places file has no tunnels, canyon or `car_dim` nodes for its light; the log says so.

## Car masks

A car's texture is repainted when the player picks a colour. A **mask** says which texels are paint, which are glass, which are lamps. One PNG per texture and detail level: `data/car_masks/<CAR>/<tex>_mask.png` (high detail) and `<tex>_lo_mask.png` (the low detail bank, whose textures have other sizes). A texture with no mask is repainted by family rules; one with a mask only by the mask. The mask can be any size (scaled by nearest neighbour).

The masks are made from layered PSDs: `tools/car_mask_psd.py` writes a PSD per texture (the texture on a locked layer, an empty `mask` layer over it at 50% opacity) and `tools/car_mask_from_psd.py <dir of CAR/<tex>.psd>` reads the **`mask` layer** back into the PNGs. Paint the mask layer with these colours (flat, no anti-aliasing at the edges):

| Colour in the `mask` layer | The texel |
|----------------------------|-----------|
| transparent | **paint**: recoloured by the paint, and shines |
| blue `0000ff` | keeps its colour, shines as glass (windows, lamp lenses) |
| green `00ff00` | keeps its colour, shines as the body (a two-tone car's roof and pillars) |
| magenta `ff00ff` | the **fixed black**: repainted with the car's catalogue black whatever the paint |
| red `ff0000` | brake light (glows in low light) |
| orange `ff8000` | position (tail) light, dimmer red |
| yellow `ffff00` | headlight, warm white in low light |
| cyan `00ffff` | reversing light, lit only in reverse gear, in any light |
| white (anything else) | keeps its colour, no shine: plastics, trim, plate, badges, vents |

The layer's own opacity is ignored. A low detail texture's PSD is named `<tex>_lo.psd`. Needs `pip install psd-tools Pillow`. A mod puts its masks in `<mod>/car_masks/<CAR>/`. To try a folder of masks without installing them: `TNFS_CAR_MASKS=<dir of <CAR>/<tex>_mask.png>`; the log's `car mask:` lines say which file each texture used.

## Dash masks

The dash (cockpit view) lights its gauges at night. `data/dash_masks/<CAR>/dash_mask.png` says which parts glow; an optional `dash_color.png` gives them colours (for the *gauge light: from the car* setting). They come from a PSD of the dash at 640 x 480:

* `tools/dash_mask_psd.py` writes the draft PSD from a dump the game makes with `TNFS_DASH_DUMP=<dir>` (a night race, in the car's cockpit): the **`dash`** layer (the dash without the steering wheel, locked), **`volante`** (the straight-ahead wheel, hidden, a guide), **`nunca visible`** (never visible: the pixels no steering pose shows, so what is painted there does not matter), and the empty **`mask`** layer. `--update <dump> <PSD dir> <output dir>` rewrites only the `dash` and `volante` layers of a PSD you already painted and copies every other layer byte for byte; the output dir must not be your PSD dir.
* `tools/dash_mask_from_psd.py <dir of CAR/dash.psd>` reads the **`mask`** layer (and the optional **`color`** layer) into the PNGs.

Colours of the `mask` layer:

| Colour | Meaning |
|--------|---------|
| transparent or black | not marked: the game's own gauge ellipses decide |
| green `00ff00` | inside a dial: its bright marks (numerals, ticks) are lit, its dark face is not |
| yellow `ffff00` (white also works) | always lit, whatever its brightness (a warning lamp) |
| blue `0000ff` | the dash's art: never lit (the speedo's rim) |

The **`color`** layer: paint over the lit parts in the colour each is to glow (the strongest channel sets the hue; the dash's art gives the brightness). Transparent or black means no colour: it glows orange. The game folds the colours into twelve flat ones, so use a few. `TNFS_DASH_MASKS=<dir of <CAR>/dash_mask.png>` tries a folder without installing it.

You can also write `dash_color.png` by hand, with no PSD, at `<mod>/dash_masks/<CAR>/dash_color.png`:

* **Format:** a PNG up to 4096 pixels a side, in the dash's 640 x 480 frame (any other size is scaled to it, nearest pixel). A pixel with alpha under 128, or black (red, green and blue all under 24), has no colour; any other pixel is the colour its light is to have. An indexed PNG has no alpha: its palette colours count, black ones are skipped.
* **Which pixels count:** only the lit ones, whether the mask or the game's own gauge ellipses lit them. A lit pixel with no colour glows orange (the `gauge_light` 3 look); a colour painted on a pixel that is not lit does nothing. The file needs no `dash_mask.png` beside it.
* **Folding:** the colours become at most twelve. A colour within a distance of 48 (the sum of the three channels' differences) of an earlier one shares its slot, a thirteenth different colour takes the nearest of the twelve. Paint a few flat colours, not a gradient.
* **When:** only with `gauge_light` 5 (*from the car*), in the cockpit while the gauges are lit (the night; also dusk and rain with `gauge_exterior`); the other values keep their own colours.
* **Lookup:** the mod's file, then `data/`, as for the mask; a car whose folder name is in lower case is also tried in upper case. `spirited-drive.log` has a line `dash dash_color.png: <path>` for the file it read; no line means no file was found. A file that is found but does not read as a PNG logs the same line and then colours nothing: the lit parts are orange.

## Track masks

At night, or in a dark tunnel, what is **reflective** (road signs, posts, the cat's eyes painted on the asphalt of some roads) is to shine when it is in the cone of the car's headlights. A **mask** says which texels of a texture are reflective. One PNG per texture: `data/track_masks/<TRACK>/<name>.png`, with `<TRACK>` the track's name in capitals (`CL1`) and `<name>`:

* `slot_<NN>` a road texture slot (the road, its shoulder; `NN` is the slot number, `id * 3 + letter`, as the places file's `snow_map` lines number them), the base-size bitmap of the slot: the cat's eyes are in these;
* `scenery_<NN>` a scenery board (signs, posts, panels; `NN` is the board number).

The PNG has the size of the texture, or any other size (scaled by nearest neighbour). **White is reflective** (red 128 or more, and not transparent); black and transparent are not. A texture with no mask is not reflective. The masks hold no colour and none of the game's pixels. The loader gives the drawing code the mask of a texture (`track_mask_for`: one byte per texel, 1 reflective), the same for a road texture and a board.

**Yellow is a lamp.** In a road texture's mask (`slot_<NN>.png`) a **yellow** texel (red 128 or more, green 128 or more, blue below 128; `255, 255, 0` is the colour to paint) marks the lamp of a tunnel or a cavern. A yellow texel is not reflective (red and white stay so). From the yellow the game
takes, for each slot that has any: the **texture** (the slot), the **place** of the lamp in it (the bounding box of the yellow texels: the lamp's glow and flare are drawn there) and the **colours** that stay bright under the night's and the tunnels' darkening (the palette entries the yellow texels show). The tunnel
lamps' light pools (`tunnel_lamps`, `docs/track-light.md`) hang where those textures are on the track. A track whose masks have no yellow keeps the built-in table (`game/race/tunnel_lamp_textures.c`: CY1, CY2, CY3, TR3, TR4), so a track nobody has painted and a mod without marks are as they were; once a track's slot masks have any yellow, the
marks are the track's lamps and the table is not read for it. A rally has the same lamps (a rally's road keeps no reflective mask, but its lamps are the track's). Yellow in a scenery board's mask (`scenery_<NN>.png`) is read as not reflective and used for nothing yet: the street lamps' boards are still the code's table.
The first yellow layer of the five tracks with a table was drawn from it by `tools/track_lamp_marks <data dir> data/track_masks` (the texels of the table's rectangle whose colour the table lets stay bright; an existing mask is kept and the yellow laid over it): correct it in the PSD, in the layer the other marks are in.

### Lamps for a track, step by step

For a track the built-in table does not know (or one you changed), the lamps are yellow marks in the road textures' masks:

1. Get the textures as PNGs and then PSDs, as for any track mask (`tools/track_mask_draft`, then `tools/track_mask_psd.py`), or open the PSDs you already have.
2. In the **`mask`** layer of the texture a lamp is drawn on (`slot_<NN>`), paint the lamp's texels yellow, `255, 255, 0`: only the lamp, not the wall round it. Paint nothing yellow in textures that are not lamps. Red and white in the same layer stay reflective.
3. Read the layers back with `tools/track_mask_from_psd.py`; it writes `slot_<NN>.png` for each texture. Put them at `<mod>/track_masks/<TRACK>/slot_<NN>.png`.
4. Load the track with `--mods <mod> --verbose` (or `TNFS_VERBOSE=1`: these lines are details, left out of a plain run's log) and look in `spirited-drive.log` for one line per marked slot, `lamp marks: slot <NN>, <k> colours, rect x0,y0 to x1,y1`. No line means the game did not read that mask (check the name and the folder); `track mask: <path>` tells where each mask came from (a mod's mask is named in every run's log; the port's own only with `--verbose`).
5. Once any slot of a track has yellow, the built-in table is no longer read for that track: mark every lamp texture of it, not only the new one.

`tools/track_lamp_marks <data dir> <output dir> [track ...]` is a starting point for the five tracks with a table (CY1, CY2, CY3, TR3, TR4): `<data dir>` holds the game's `SIMDATA/ETRACKFM/<TRACK>_001.FAM`, and it writes the table's lamps as yellow into `<output dir>/<TRACK>/slot_<NN>.png`. A mask already there keeps its red and white and gets the yellow laid over it; for a track outside the table it writes no mask.

The masks are made from layered PSDs, as the car's are: `tools/track_mask_draft` (built with the game) writes each texture of the CD's tracks as a PNG, `tools/track_mask_psd.py <drafts> <output>` turns them into PSDs (the texture on a locked layer, an empty **`mask`** layer over it at 50% opacity), and `tools/track_mask_from_psd.py <dir of TRACK/<name>.psd>` reads the **`mask` layer** back into the PNGs: paint what is reflective in any colour but black (white is easiest); an unpainted or black texel is not. The layer's own opacity is ignored. Needs `pip install psd-tools Pillow`. A mod puts its masks in `<mod>/track_masks/<TRACK>/`. To try a folder of masks without installing them: `TNFS_TRACK_MASKS=<dir of <TRACK>/<name>.png>`; the log's `track mask:` lines say which file each texture used.

## Car environment map

With `body_shine` 4 the cars' paint and glass take their light from an environment map, a sphere map of the sky around the car. The port makes one at start-up (a sky gradient with soft clouds and a sun spot, a dark band under the horizon, flat dark ground, no EA or Sony pixels); `data/car_envmap.png`, or `<mod>/car_envmap.png`, replaces it.

* **File:** a PNG up to 4096 pixels a side, any size, taken as grey (an indexed PNG by its palette's red, any other by its red channel) and resampled to 128 x 128. Row 0 is the top.
* **Layout:** a sphere map: a surface facing the camera reads the centre, one turned edge-on reads the rim. Its axes are the world's, so the sky stays up however the camera tilts or rolls, and the light does not turn with the car.
* **Values:** 128 leaves the paint as it is, above it lightens (255 is the strongest highlight), below it darkens.
* **Lookup:** the mod's file, then the program's `data/`, then the working directory's `data/`, then a development build's source tree. A file that is missing, not a PNG, or too large is skipped without a log line and the next one is tried, then the port's own map. A development run can point `TNFS_CAR_ENVMAP=<file>` at a file outside these folders.
* Stock and Restored keep the exe's frames (`body_shine` is 0 there), so the map shows only in Restomod or with `body_shine=4` set by hand.

## Sounds

`data/sounds/` holds the sounds the port added, three 16-bit mono WAVs at the mixer's rate (22050 Hz); [sounds.md](sounds.md) says when each plays and which flag turns it on.

| File | What | Plays |
|------|------|-------|
| `thunder.wav` | the storm's thunder | `weather=storm` with `rain_v2` |
| `rain_tyre_roll.wav` | a looping wet-tyre roll | rain or storm, `rain_v2` and `rain_tyre_roll`, the followed car on open asphalt |
| `snow_crunch.wav` | a looping snow crunch | the snowy rally (Alpine 3, `snow_weather`), the followed car moving |

A mod's `<mod>/sounds/<file>` is used instead of the one in `data/`, under the same name. The WAV must be 16-bit mono PCM at 22050 Hz; a file that is not (another rate, stereo, 8 or 24 bit) is skipped, `spirited-drive.log` says so once (`wet_roll: <path>: not a 16-bit mono WAV at the mixer's rate`, `thunder: ...` for the thunder) and the next candidate is tried, then the sound stays silent (`wet_roll: no sounds/<file>: the roll is silent`). Nothing else breaks. The two rolls loop, so make the file's end meet its start (a click at the seam is heard every few seconds). The sounds play only in Restomod: Stock and Restored keep the exe's.

Only CC0 or public-domain samples go in the repository, each with its source, author and licence written in [`data/sounds/LICENSES.md`](../data/sounds/LICENSES.md): add the entry there in the same change as the file (what it is made from, what was changed, the URL, the author, the licence and the date it was checked on the page). A mod of your own is free to use any sound it has the right to.

## The mask tools

The masks are painted in PSDs and read back by scripts in `tools/`. The Python ones need Pillow; the `from_psd` ones also `psd-tools` (`pip install psd-tools`). The two programs are built with the game (`track_mask_draft`, `track_lamp_marks`).

**`track_mask_draft <data dir> <output dir> [track ...]`**: `<data dir>` holds the game's `SIMDATA/ETRACKFM/<TRACK>_001.FAM` (the midday art). For each track named (all sixteen when none is) it writes, in `<output dir>/<TRACK>/`, `slot_<NN>.rgb.png` for every road and shoulder texture (NN the slot, variant 0) and `scenery_<NN>.rgb.png` for every board (signs, posts, panels), in the track's own colours, a transparent texel transparent. **These files hold the game's pixels: keep them out of any repository.** A track whose art cannot be read is reported and skipped, and the exit code is 1.

**`tools/track_mask_psd.py <drafts dir> <output dir> [--png <dir>] [--all]`** turns those drafts into one PSD per texture, `<output dir>/<TRACK>/<name>.psd`, at the texture's size: a locked `texture` layer, an empty **`mask`** layer at 50% (yours: paint what is reflective in any colour but black; yellow `255, 255, 0` is a lamp), and a hidden `draft` layer with a first guess (`tools/track_mask_auto.py`: the cat's eyes of a road, the letters of a green sign, the symbol and border of a sign, the stripe of a post) to copy into `mask` or touch up. The game never reads `draft`. By default a track with a list of candidates gets PSDs only for the textures worth painting, `--all` writes every texture. `--png <dir>` also writes each guess as `<dir>/<TRACK>/<name>.png`, white reflective and black not, which is the format the game reads: drop them in a mod's `track_masks/` to try the guesses without painting.

**`tools/track_mask_from_psd.py <dir of TRACK/<name>.psd> [<output dir>]`** reads each `mask` layer into `<output dir>/<TRACK>/<name>.png` (default: `data/track_masks` next to `tools/`; give your mod's `track_masks/` instead). A texel is reflective when it is opaque enough (alpha 128 or more) and not black (brightest channel 64 or more); a yellow texel (red and green 192 or more, blue 64 or less) is a lamp instead; the layer's own opacity does not matter. A mask with no lamp is a grey PNG, 255 reflective, 0 not; one with lamps is RGBA, white reflective, yellow `255, 255, 0` the lamps, transparent the rest.

**`tools/track_lamp_psd.py <textures dir> <masks dir> <output dir> TRACK/slot_NN ...`** makes the PSD of a mask that already exists only as a PNG (the lamp masks `tools/track_lamp_marks` drew): the texture from `tools/track_mask_draft`'s output under a `mask` layer holding the PNG's yellow and white, so it can be edited as the others; `tools/track_mask_from_psd.py` reads it back into the same PNG.

**`tools/dash_mask_psd.py <dump dir> <output dir>`** writes the dash PSDs. The dump is made by the game: run a night race in the car's cockpit with `TNFS_DASH_DUMP=<dump dir>`; it writes, once per dash, `<STEM>.ppm` (the dash without the wheel), `<STEM>.mask.png` (a draft of the two gauge ellipses as dials) and the steering frames the dash is rebuilt from. `<output dir>/<CAR>/dash.psd` has the layers `dash` (locked), `volante` (the straight-ahead wheel, hidden, a guide), `nunca visible` (what no steering pose shows: paint there does nothing) and the empty `mask` at 50%. **An existing file is never touched**: a new draft goes beside it as `dash_draft.psd`, `dash_draft_2.psd`, and so on. After a new dump, `--update <dump dir> <PSD dir> <output dir>` rewrites only the `dash`, `volante` and `nunca visible` layers of the PSDs you painted and copies every other layer (`mask`, `color`, `limpia`) byte for byte, into the output dir, which must not be your PSD dir.

**`tools/dash_mask_from_psd.py <dir of CAR/dash.psd> [<output dir>]`** reads the `mask` layer into `dash_mask.png` (colours in the Dash masks table above; a pixel with alpha under 128 or black is not marked, yellow, white or light grey is lit, blue is the dash's art, green a dial, red a wiper blade the dash draws) and, if the PSD has a **`color`** layer, `dash_color.png` too (a PSD without one gets none, and an old `dash_color.png` there is deleted). A layer named `limpia` (or `wipers`) holds the wiper blades: whatever is painted on it, in any colour but black, is a blade.

## Settings and launchers

Every key of `port.cfg` and the three mode files, its default, range, menu and `TNFS_*` variable is in [settings.md](settings.md); copy nothing from there into a mod, point to it. The development variables (`TNFS_START_NODE`, `TNFS_FRAMES`, `TNFS_SHOTS`, `TNFS_VIEW`...) drive headless runs, with `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`, `timeout -k` and a scratch `--user` folder so the player's files stay out.

*Colour palettes (.aco files, as used for the paint colours) are not in the repository; only their use is described where a tool takes them.*

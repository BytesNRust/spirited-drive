# Modding guide

How to change the game's data without touching the CD: where files live, what a mod can replace, and the file formats of the things the port added (track light, car masks, dash masks, sounds). The settings themselves are in [settings.md](settings.md); the track light format has its own page, [track-light.md](track-light.md).

## The three folders

| Folder | What it is | Written by the game? |
|--------|------------|----------------------|
| **Game data** (`--data DIR`, or the player's copy `<player folder>/gamedata/`, or `./original` in a development build) | The original CD's files: `SIMDATA/`, `GAMEDATA/`, `FRONTEND/`, the `NFS_WIN.EXE`. Read only. | Never |
| **The port's data** (`data/` beside the program; in a source tree `data/` at the repository root) | What the port ships: `car_masks/`, `dash_masks/`, `track/`, `track_light/`, `sounds/`, the car environment map. Looked up as `<mod>/...` first (see below), then `<program folder>/data/...`, then `./data/...`, then (development builds) the source tree's `data/...`. | Never |
| **The player's folder** | `port.cfg`, `stock.cfg`, `restored.cfg`, `restomod.cfg`, `controls.cfg`, `extras.cfg`, `records.dat`, `config.dat`, `Replay.rpl`, saved games, `tnfs.log`, `gamedata/`, `mods/`. | Yes: all of it |
| **A mod** (`--mods DIR`, else `<player folder>/mods/`) | A folder of files that replace the game's. One overlay. | Never |

The player's folder is `~/Library/Application Support/TNFS Restomod/` on macOS, `%APPDATA%\TNFS Restomod\` on Windows and `~/.local/share/TNFS Restomod/` on Linux. `--user DIR` puts it at `DIR` for that run (a portable install, a test: `tnfs --user ./user`); `TNFS_PREF_DIR=DIR` does the same and is what the test suite uses. `--mods` has no say in where the player's files go. The EXTRAS page's *Open folder* shows the player's folder; `tnfs.log` in it is what to send with a bug report.

If you used an older build: what `tnfs/tnfs/` (the old folder name) or `<mods>/user/` held moves to the new folder the first time, one line in `tnfs.log` says so, and a file that already exists in the new place is kept (the old copy stays where it was).

## How a mod replaces a file

Run with `--mods DIR` (or leave the files in `<player folder>/mods/`). When the game asks for a file of the game data, say `SIMDATA/MISC/TR1.TRI`, it looks in the mod first, at the same path (`DIR/SIMDATA/MISC/TR1.TRI`, or the upper-case form of it), and then in the game data. A file the mod does not have comes from the game data, so a mod holds only what it changes.

* The mod's file is checked against what the original executable accepts (`tnfs-studio` has the same checks). A file that fails is **not** given to the game and the run says which file and why (`platform_mods_error`, also in `tnfs.log`).
* Names that leave the mods folder (`..`, absolute paths, symbolic links out of it) are refused.
* The mod is never written: saves, replays and settings go to the player's folder.
* The mods folder must exist when it is given with `--mods` (a missing one is an error); the default `<player folder>/mods/` is used only when it is there.

Example, a changed track:

    mymod/SIMDATA/MISC/TR1.TRI
    tnfs --mods mymod --track TR1 --node 120 --screenshot out.ppm

[modding/track-layout.md](modding/track-layout.md) and [modding/limitations.md](modding/limitations.md) cover building tracks and what the engine cannot do; `tnfs-studio slot replace` puts a track or a car into a mod's folder with the checks.

The port's own files (track light, car masks, dash masks, sounds, the car environment map) go through the same overlay: **a mod's `<mod>/<path>` is tried first, where `<path>` is the path under `data/`** (`track_light/CY1N.txt`, `track/CY1.txt`, `car_masks/<CAR>/<tex>_mask.png`, `dash_masks/<CAR>/dash_mask.png`, `sounds/thunder.wav`, `car_envmap.png`), then the port's own `data/`. The `TNFS_CAR_MASKS` and `TNFS_DASH_MASKS` variables, for trying a folder, still win over both.

## Track light

`data/track_light/<TRACK><A|B|C|N>.txt`, one file for each track and time of day (A dawn, B day, C dusk, N night). In Restomod '26, with no PS1 disc, the light of a track is this file, whole. A mod can carry its own copy: **`<mod>/track_light/<file>` is tried first**, then `data/track_light/<file>`. A file is a few lines per run of nodes (`rows` the light rows, tunnel and lamp lines, `brightness N N P` to dim or lift a stretch, `car_tone` the car's tone). Stock '96 and Restored '96 never read these files.

    # a stretch of the night track at 70 percent of its light, then the car warmer there
    brightness 120 180 70
    car_tone 120 180 ff8020:60

The **live mode** `light_edit=1` in `port.cfg` (or `TNFS_LIGHT_EDIT=1` for one run) shows in a race the node the car is at, which file it reads and the values there. **F10** holds the race where it is, **F11** copies a `brightness N N 100` line to the clipboard, and saving the file reloads the light within a second without leaving the race.

The keywords are English: `track time nodes flags` (the header), `rows`, `present`, `pool car_pool`, `car_disc`, `tunnel` (a zone: `tunnel a b [light] [colour | lit]`), `tunnel_tone <light> [colour]`, `brightness`, `tone`, `car_tone`; [track-light.md](track-light.md) is the reference. Files written with the first, Spanish words (`pista hora nodos banderas filas tunel pozo brillo tono_coche`...) still load: the port reads each as its English word. The generator is `tools/export_track_light.sh`: **the export writes only missing files; `FORCE=1` rewrites them and loses your edits** (an existing file is kept, with a "kept <file> (exists; FORCE=1 to overwrite)" line). A retouch can live in the shipped file or in a mod's folder (the mod's copy goes first).

### Track places

`data/track/<TRACK>.txt` holds the physical facts of a track, the same at every time of day, and is the home for other per-track data later: a header `track AL1`, then `tunnel a b`, `canyon a b` and `car_dim a b` lines (and `present <key>` for an array that is there but empty); a word the format does not know is a bad line. It goes through the same overlay as the light files: **`<mod>/track/<TRACK>.txt` is tried first**, then `data/track/<TRACK>.txt`. The time's light file (`track_light/<TRACK><A|B|C|N>.txt`) adds to it: its `tunnel a b lit` says a stretch is a hall, and its `tunnel_tone` the tunnels' light and colour at that time. A track with no places file has no tunnels, canyon or `car_dim` nodes for its light; the log says so.

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

## Sounds

`data/sounds/` holds the sounds the port added (so far the storm's `thunder.wav`, mono 16-bit PCM). A mod's `<mod>/sounds/thunder.wav` is used instead of it. Only CC0 or public-domain samples go in the repository, each with its source, author and licence written in [`data/sounds/LICENSES.md`](../data/sounds/LICENSES.md): add the entry there in the same change as the file (what it is made from, what was changed, the URL, the author, the licence and the date it was checked on the page). A mod of your own is free to use any sound it has the right to.

## Settings and launchers

Every key of `port.cfg` and the three mode files, its default, range, menu and `TNFS_*` variable is in [settings.md](settings.md); copy nothing from there into a mod, point to it. The development variables (`TNFS_START_NODE`, `TNFS_FRAMES`, `TNFS_SHOTS`, `TNFS_VIEW`...) drive headless runs, with `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`, `timeout -k` and a scratch `--user` folder so the player's files stay out.

*Colour palettes (.aco files, as used for the paint colours) are not in the repository; only their use is described where a tool takes them.*

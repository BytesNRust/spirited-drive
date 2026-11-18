# Modding guide

How to change the game's data without touching the CD: where files live, what a mod can replace, and the file formats of the things the port added (track light, car masks, dash masks, track masks, sounds). The settings themselves are in [settings.md](settings.md); the track light format has its own page, [track-light.md](track-light.md).

## The three folders

| Folder | What it is | Written by the game? |
|--------|------------|----------------------|
| **Game data** (`--data DIR`, or the player's copy `<player folder>/gamedata/`, or `./original` in a development build) | The original CD's files: `SIMDATA/`, `GAMEDATA/`, `FRONTEND/`, the `NFS_WIN.EXE`. Read only. | Never |
| **The port's data** (`data/` beside the program; in a source tree `data/` at the repository root) | What the port ships: `car_masks/`, `dash_masks/`, `track_masks/`, `track/`, `track_light/`, `sounds/`, the car environment map. Looked up as `<mod>/...` first (see below), then `<program folder>/data/...`, then `./data/...`, then (development builds) the source tree's `data/...`. | Never |
| **The player's folder** | `port.cfg`, `stock.cfg`, `restored.cfg`, `restomod.cfg`, `controls.cfg`, `extras.cfg`, `race_records.dat`, `config.dat`, `Replay.rpl`, saved games, `spirited-drive.log`, `gamedata/`, `mods/`. | Yes: all of it |
| **A mod** (`--mods DIR`, else `<player folder>/mods/<id>/`) | A folder of files that replace the game's. One overlay. | Never while it runs; `--install` and `--uninstall` put a mod in `mods/` and take it out (*Installing a mod*) |

The player's folder is `~/Library/Application Support/Spirited Drive/` on macOS, `%APPDATA%\Spirited Drive\` on Windows and `~/.local/share/Spirited Drive/` on Linux. `--user DIR` puts it at `DIR` for that run (a portable install, a test: `tnfs --user ./user`); `TNFS_PREF_DIR=DIR` does the same and is what the test suite uses. `--mods` has no say in where the player's files go. The EXTRAS page's *Open folder* shows the player's folder; `spirited-drive.log` in it is what to send with a bug report.

If you used an older build: what `TNFS Restomod/` or `tnfs/tnfs/` (the old folder names, the newer first) or `<mods>/user/` held moves to the new folder the first time, one line in `spirited-drive.log` says so, and a file that already exists in the new place is kept (the old copy stays where it was).

## Installing a mod

A mod made for others comes as a zip with a `mod.ini` at its root (`tnfs-studio mod pack FOLDER OUT.zip` makes one). Three commands
handle the installed ones. None of them opens a window or needs the game data, and each exits with 0, or with 1 and `tnfs: <reason>`:

    tnfs --install FILE.zip     installs it, or updates the installed one with the same id: "installed <id> <version>",
                                "updated <id> <old> -> <new>"
    tnfs --uninstall ID         moves it to the backups: "uninstalled <id> (backup kept)"
    tnfs --mods-list            one line per installed mod: id, version, type, on (the edition in use names it) or off, name
                                (tab-separated)

`--user DIR` before them works on that player's folder. What happens:

* **Everything is checked before anything is written** (the names, the kinds of file, each file's content with the game's own readers,
  and what the mod's kind may carry, below). A refused zip leaves the player's folder as it was.
* The mod is unpacked into `<player folder>/mods/.staging/<id>/`, then renamed to `mods/<id>/`. A staging folder left by a run that
  stopped is cleaned the next time. A folder `mods/<id>/` with no `mod.ini` was put there by hand and is never replaced.
* **An update** moves the installed folder to `<player folder>/mods_backup/<id>-<version>-<date>/` first. Uninstalling moves it there
  too: nothing is deleted except backups beyond the last 3 of each id.
* Nothing is written outside `mods/` and `mods_backup/`, and no symbolic link is followed.

What a mod may carry depends on its `type` in `mod.ini`:

| Type | May carry |
|------|-----------|
| `tweak` (or no `type`) | how the game looks and sounds: art, fonts, music, sounds, dashboards, car models (CFM), track scenery (FAM), the sky (`<TRACK>INFO`), `track_light/`, the masks, `car_envmap.png`. **Never** what changes a race: a track's `.TRI` and the rest of `SIMDATA/MISC` (but its INFO), `SIMDATA/CARSPECS/`, a car's `.PDN`, `track/<TRACK>.txt`. [modding/inventory.md](modding/inventory.md) §12 says why for each |
| `car` | its own three files (its stem's CFM, PDN and PBS) and `car_masks/<STEM>/`, `dash_masks/<STEM>/`, and nothing else (see *Adding a car*) |
| `track` | its stages' files: for each stage `<STEM>1`..`<STEM>3` its `.TRI`, `<STEM><n>INFO` and the day art `ETRACKFM/` and `GTRACKFM/` `<STEM><n>_001.FAM` (optional: `NTRACKFM/` `_T01`, `_M01`, `_R01`, and the port's `track/`, `track_light/` text and `track_masks/` PNG named by the same root), and nothing else. A stem may not begin with AL, CL, CY or TR. This version adds A-to-B tracks only (`kind=stage`); it checks, installs and runs them in Restomod (*Try your piece*, below) |

Every type may also carry `mod.ini`, `preview.png` and a README or LICENSE. `GAMEDATA/` (the player's config, replays and saves) is never
part of a mod. A folder in `mods/` that holds a file its type may not carry, put there by hand, still runs: that file is not given to the
game, and the log says so once (`mods: <id>: <file> is left out: <why>`).

**Installing only makes a mod available.** Which installed mods the game uses is the edition's to say (*Editions*, below): the
standard edition, today's game, uses none of them. Uninstalling changes no edition; one that names the mod skips it.

### Editions

In Restomod an edition is a set of cars, tracks and tweaks, `<player folder>/editions/<id>.ini`, and `edition=<id>` in `restomod.cfg`
says which one is in use (docs/settings.md). Without it the game is the standard edition. The file is `key=value` lines as `mod.ini`'s:

    name=Europa 80
    cars=f40,testarossa-koenig,alpine-a110,rx7
    tracks=city,coastal,rusty
    tweaks=coastal-night-light,nicer-smoke

* `name`: what the game shows, 1 to 12 printable ASCII characters (not `Customize game`).
* `cars` and `tracks`, in the menus' order; `tweaks`, the first winning over the ones after it. Ids between commas: a mod's `id`, or
  one of the game's own: the cars `rx7 nsx supra 911 zr1 viper 512tr diablo warrior`, the tracks `city coastal alpine rusty autumn burnt
  vertigo transtropolis vegas` (`rusty` brings Lunar Springs with it; `hidden`, the cut moon track, is kept and never listed). Up to 64
  cars, 32 tracks and 32 tweaks; `original` is no id.
* A piece that is not installed stays in the file and is left out, with a line in the log. An edition that keeps no car or no track
  then, or whose file is missing or damaged, is not used: the standard edition is, and the log says why.
* The folders the game reads are `--mods DIR`, then the edition's tweaks in their order, then its added cars.

Control Central's car arrows and its head to head rival's step through the edition's `cars` in its order; the Warrior is there only when the edition lists it, and a car it does not list (one picked before) becomes its first car. The standard edition's arrows are the original's: the eight race cars, and the Warrior when it is on offer. What an edition changes in the track menus and its records comes with the rest of dep59 step 4.

An edition unlocks everything itself, whatever the unlock level and the cheat codes say: the mirror, the rally and the way round, Lost
Vegas, the Warrior when `cars` lists it, and Lunar Springs when `tracks` lists `rusty`. Its unlock level is 2 and is never written, and
Customize Game shows the rows it decides dimmed, with the reason. The codes that unlock (SKCART, EARALLYE, CASINOVILLE, CONCEPTCAR,
ROCKYROAD) do nothing in it; NITRO, EXPLODE and TURBO work as always.

* Tournament needs the stock cars and tracks: an edition whose `cars` and `tracks` are the eight race cars and the eight tracks (in any
  order, the Warrior, `vegas` and `hidden` allowed too) has the tournament; any other, and Control Central's race type arrows pass it.
* Multiplayer needs the standard edition: in an edition Game Mode shows it dimmed, and choosing it says so.
* A saved game is a tournament: in an edition without the tournament, Load Game says so and loads nothing.

Game Mode's selector changes the edition at once, with no restart: the mod folders in effect, Control Central's cars, tracks and unlocks,
and Customize Game's rows are the new edition's from the next screen on, and nothing typed or kept in the edition before (a cheat code, a
Warrior) carries over. The log says `mark:edition=<id>` (`-` for the standard edition).

**`--mods DIR` serves no file that changes a race.** A changed track or car is a piece of its own: make one with `tnfs-studio track new
--piece` or `tnfs-studio car new`. A loose TRI, PBS, PDN or `track/*.txt` in `--mods` is left out, and the log says once
`mods: <id>: <file> is left out: a changed track is a track piece now: tnfs-studio track new --piece` (a car's: `a changed car is a car piece
now: tnfs-studio car new`). What does not change a race (the look: pictures, `track_light/`, a track's day art) is still served. One exception is the live light editor
(`light_edit=1`): it works on a track's places file, `track/<TRACK>.txt`, in the `--mods` folder, so with the editor on that file is served,
the log says `mods: --mods serves <file> to the light editor: this run keeps no records and no replays`, and the run keeps none. Do not
play a network game from such a session: the other players do not have your places file.

## How a mod replaces a file

Run with `--mods DIR` (or leave the files in `<player folder>/mods/`). When the game asks for a file of the game data, say `SIMDATA/MISC/TR1.TRI`, it looks in the mod first, at the same path (`DIR/SIMDATA/MISC/TR1.TRI`), then its upper-case form, then its lower-case form (`DIR/simdata/misc/tr1.tri`), always in that order, and then in the game data. On Linux, where names differ by case, the mod's files can be named in either case; keep one copy of each file. A file the mod does not have comes from the game data, so a mod holds only what it changes.

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

The race repaints the body textures by their names (`topv`, `rsid`, `bott`, `wing`, `frnt`, `rear`, and `side`, `back` in the low bank). An added car's own texture (`car texture add`, a name of its own such as `tras`) is repainted too when the car's own mask for it (`<mod>/car_masks/<STEM>/<tex>_mask.png`, or a skin's) marks a texel as paint; without a mask, or with one that paints nothing (a lamps' mask), it keeps its colours. An original car's masks keep their names' classes.

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
| red `ff0000` | a wiper blade the dash's art draws: with the wipers on (rain or snow) it is not drawn; on the glass the view shows through, on the nose (cyan) the nose is made again under it from the nose round it |
| cyan `00ffff` | the car's nose seen through the windscreen: with the wipers on the sweeping blades are drawn over it (the 3D, then the nose, then the blades, then the rest of the dash); never lit |

Do not leave stray cyan pixels inside the red: each red pixel takes the side, glass or nose, of the nearest glass or nose pixel, so a lone cyan pixel left inside a blade (a leftover of the wiper's pivot, say) claims all the red round it, and a disc of filled nose in its colour grows there over the glass. Paint the cyan only where the nose really shows, up to the blade's edge, and keep the blade's red solid.

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

`data/sounds/` holds the sounds the port added, 16-bit mono WAVs at the mixer's rate (22050 Hz); [sounds.md](sounds.md) says when each plays and which flag turns it on.

| File | What | Plays |
|------|------|-------|
| `thunder.wav` | the storm's thunder | `weather=storm` with `rain_v2` |
| `rain_stopped_out.wav` | the rain round a stopped car, a loop | rain or storm, `rain_v2` and `rain_tyre_roll`, the followed car stopped, a view outside |
| `rain_stopped_in.wav` | the rain on the roof, a loop | the same, the dash view |
| `rain_moving_out.wav` | the rain round a moving car, a loop | the same, the followed car moving, a view outside |
| `rain_moving_in.wav` | the rain on a moving car with its tyres' hiss, a loop | the same, the dash view |
| `rain_spray.wav` | a car's tyre spray, a loop | the same or snow falling, every car moving on asphalt, where the car is (lower under a rally's gravel roll) |
| `rain_puddle_1.wav` .. | puddle splashes, played once each | the same cars at random, more the faster; `rain_puddle_<n>.wav` from 1 up to the first missing (at most 8), so a mod can add more |
| `snow_roll.wav` | the roll on snow, a loop | the snowy rally (Alpine 3, `snow_weather`) only, the followed car moving |

A mod's `<mod>/sounds/<file>` is used instead of the one in `data/`, under the same name. The WAV must be 16-bit mono PCM at 22050 Hz; a file that is not (another rate, stereo, 8 or 24 bit) is skipped, `spirited-drive.log` says so once (`wet_roll: <path>: not a 16-bit mono WAV at the mixer's rate`, `thunder: ...` for the thunder) and the next candidate is tried, then the sound stays silent (`wet_roll: no sounds/<file>: the roll is silent`). Nothing else breaks. The rolls loop, so make the file's end meet its start (a click at the seam is heard every few seconds). The sounds play only in Restomod: Stock and Restored keep the exe's.

Only CC0 or public-domain samples go in the repository, each with its source, author and licence written in [`data/sounds/LICENSES.md`](../data/sounds/LICENSES.md): add the entry there in the same change as the file (what it is made from, what was changed, the URL, the author, the licence and the date it was checked on the page). A mod of your own is free to use any sound it has the right to.

## Adding a track (development)

A `track` piece (`type=track` in its `mod.ini`, made with `tnfs-studio track new --piece`) is an A-to-B track like Coastal and Alpine. Its `mod.ini` keys beside `id`, `name`, `version`, `author` and `description`:

| Key | What it is | Rule |
| --- | --- | --- |
| `stem` | the name its stages' files go by (`<STEM>1`..`<STEM>3`) | 1 to 8 of `A-Z 0-9`; not a stem of the CD's tracks |
| `kind` | the kind of track | `stage` (the default); `circuit` is refused: this version adds A-to-B tracks only |
| `segments` | how many stages it has | 1, 2 or 3 (1 when not given) |
| `road`, `edge` | its road and what lies beside it | `asphalt` or `dirt`; `grass`, `gravel`, `sand`, `snow`, `dirt` or `water` |
| `length` | the length its page shows, as written (`14.2 km`) | optional, up to 16 bytes |
| `times` | the times of day it takes | optional; some of `dawn`, `day`, `dusk`, `night`, each once |
| `showcase_tint` | the tone of its page's background: the CD's sepia pages turned to this colour, each shade keeping its brightness | optional; `#RRGGBB` (hex digits, either case, nothing else); without it the page is sepia. `tnfs-studio mod pack` and the game refuse a value that is not `#RRGGBB` (`PIECE.TINT`) |

## Try your piece

A track or a car piece runs only in Restomod. Stock and Restored load no mods at all, so a piece installed for them is silently
unused. Three lines make it run, in the player's folder (the `--user` folder while you test):

* `port.cfg`: `profile=restomod`
* `restomod.cfg`: `edition=<id>`, the edition in use
* `editions/<id>.ini`: `name=...`, and the piece's id in `tracks=` (a track) or `cars=` (a car)

Copy-paste, with a scratch folder `MYUSER` for the test (a track; a car is the same with `cars=` and `--car`):

    tnfs-studio track new loop.trk --template AL1 --piece --id novato-loop --stem NOVATO --name "Novato Loop" --author "Your name" --out build/piece --data original
    tnfs-studio mod pack build/piece novato-loop.zip
    tnfs --user MYUSER --install novato-loop.zip
    printf 'profile=restomod\n' >> MYUSER/port.cfg
    printf 'edition=novato\n' > MYUSER/restomod.cfg
    mkdir -p MYUSER/editions
    printf 'name=Novato\ntracks=novato-loop\n' > MYUSER/editions/novato.ini
    tnfs --user MYUSER --track NOVATO1 --node 120 --screenshot novato.ppm

The track's race is its stem and 1: `NOVATO1`. A car is raced with `tnfs --user MYUSER --drive TR1 --car <id>` once its edition lists it.
Without the three lines, `--track NOVATO1` fails with rc 1 and no line on the standard output; the reason is in `spirited-drive.log`.

Reinstalling: bump `version=` in `mod.ini` before each `--install`. The install replaces the folder whatever the version, but a message
that says `updated novato-loop 0.1 -> 0.1` tells nothing about what changed.

## Adding a car (development)

In Restomod a mod can add a car beside the twelve of the original game: a folder whose `mod.ini` says `type=car`. Stock and Restored never load one. It is picked on Control Central when the edition in use lists it (Editions, above: the arrows step through the edition's cars, the player's and the head to head rival's), shown in its own gallery, and raced from the menus or the command line.

    mycar/mod.ini
    mycar/SIMDATA/CARFAMS/MYCAR.CFM        the 3D model and its textures
    mycar/SIMDATA/CARFAMS/MYCAR.PDN        its performance data
    mycar/SIMDATA/CARSPECS/MYCAR.PBS       its specs
    mycar/card.png                         its card on Control Central (required)
    mycar/rival.png, mycar/gallery.png     its rival card and gallery picture (optional)
    mycar/skins/<name>/...                 its skins (optional, see Pictures, skins and paints)
    mycar/car_masks/MYCAR/...              its car masks (optional, see Car masks)

    tnfs --mods mycar --drive TR1 --car my-car

`mod.ini`:

| Key | What | Rule |
|-----|------|------|
| `id`, `name`, `version` | as every mod's | `id` is what `--car` takes |
| `author`, `description` | the maker's name and what the car is, shown in its gallery | optional; up to 48 and 400 characters of text, one line; `tnfs-studio car new --author --description` writes them |
| `type` | `car` | required for a car |
| `stem` | the name its three files go by (`MYCAR` above) | 1 to 8 of `A-Z 0-9`; not the stem of a car, traffic or police car of the CD, nor `M` and one (the CD's smaller rival models) |
| `short_name` | what the results show | up to 8 characters of printable ASCII; the stem when not given |
| `year`, `power`, `weight` | the figures its gallery shows, as written (`1992`, `400 hp`, `1500 kg`) | optional, up to 16 bytes each, drawn in the menus' small face (see The gallery) |
| `make`, `model`, `country` | the car's make, model and country, as written (`Porsche`, `911 Carrera`, `Germany`) | optional, up to 32 bytes each; read and kept, not shown yet (`year` is the key above) |
| `paints` | its gallery's Color choices: `Name:rrggbb,Name:rrggbb` | optional; up to 16, a name of 1 to 24 printable ASCII characters without `:` or `,`, six hex digits, no two names alike in any case |
| `sound_from` | the original car whose engine it plays | required: `supra`, `diablo`, `911`, `zr1`, `512tr`, `viper`, `nsx` or `rx7` |
| `ai_level` | how fast a computer driver takes it: `slow`, `medium`, `fast` or `top` | optional; without it the pace comes from the car's own PDN (its top gear's limit), kept between `slow` and `top` |
| `card_name` | whether the game draws the car's name over `card.png` and `rival.png` | optional; `on` (the default) or `off`, for pictures that show their own name |
| `category` | the car's category, shown in its showcase page's General box as written | optional; up to 16 characters (e.g. `B`) |
| `photos` | the big pictures of the car's showcase page, in order (the first when the page opens; arrows inside the picture when there are several) | optional; up to 8 names of PNG files in the piece's root, comma separated: `a-z 0-9 - _ .`, ending `.png`, up to 24 characters, none of `card.png`, `rival.png`, `gallery.png`, `preview.png`, `car_envmap.png`; each from 100x60 to 1280x960 (the game crops it to fill its tile: 450x254, twice the tile's 225x127, is a good size) |
| `photo_small` | the small picture of the showcase page | optional; one such name (278x228 is twice the tile's 139x114) |
| `dash_from` | the original car whose cockpit it shows | optional, the same words; without it the car has no cockpit view, and the view key and the race's start go to the first view behind the car |

`ai_from` is gone: a car with it is refused, with a line saying to use `ai_level` instead. An added car's brake lamps are always the standard red, whatever its `sound_from`.

The car is checked before it may race, with the same checks `tnfs-studio` uses: the three files are there (in any case), each passes the original executable's limits, and the specs can run a race (the PBS checksum, a power above 0, `unknown_320` and `force_scale` not 0, and 1 to 58 torque points). One line in `spirited-drive.log` says what happened to each car piece: `car piece: my-car is car 16`, or `car piece: my-car refused: <why>`. A refused car, or one asked for in Stock or Restored, stops `--drive` with `--drive: no car my-car`; nothing else changes.

The original game has a few habits that belong to one car only (one car's throttle and gear changes, two cars' tyre frames); an added car has none of them and drives as its PBS says.

Several cars can be added, up to 64, one folder each, in the mods in effect (`--mods`, then Restomod's list). Two folders with the same `id` or `stem`: the first one counts and the other is refused. A car's number (16 and up) holds for that run only; changing the list keeps the number of every car that stays.

**Records and replays.** A race with an added car never writes to the original game's files: `race_records.dat`, `config.dat` and `Replay.rpl` stay as they were. Its records go to `race_records_cars.dat`, the records by car, with a table per car named by its `id`, so each added car keeps its own best times; uninstalling or turning off the car never deletes them. Its replay is kept as `Replay.rpx` (the original's blocks, then a last one naming each added car by its `id` and a fingerprint of its PBS, its PDN and its computer driver's pace), and plays back with `tnfs --mods mycar --replay Replay.rpx`. It does not play, and says why, when the car's mod is not loaded (`replay: needs the mod my-car`) or when its PBS, its PDN or its `ai_level` changed since the race (`replay: the mod my-car has changed since this replay was made, or the game's rules for added cars have`): a replay re-runs the race from the players' inputs, so different physics or a different computer driver would drive a different race. A replay kept before a version of the game that changed how added cars drive is refused the same way. Changing the CFM or the masks keeps the replay. The replay keeps the skin and colour the car wore, and plays them back whatever is picked now. In the menus (Restomod, outside a link game) the Load/Save list shows `.RPX` replays beside the original's: a row that cannot play says why (`RACE1 - needs my-car`, `RACE1 - my-car changed`) and is not loaded, and the save box keeps a race with an added car as `<name>.RPX`.

### Pictures, skins and paints

The game never scales a picture: each is a PNG of its exact size, named in lower case as below.

| File | Size | What |
|------|------|------|
| `card.png` | 309 x 166 | the car's card on Control Central. Required: a piece without it is refused (`PIECE.CARD`) |
| `rival.png` | 284 x 117 | the head to head rival's card; the middle 284 x 117 of `card.png` when there is none |
| `gallery.png` | 320 x 200 | the gallery's big photo when `photos=` names none; `card.png` when there is none |

The cards are fitted to Control Central's palette by nearest colour, the gallery's picture to the gallery's own. The game draws the car's `name` over `card.png` and `rival.png` as the original cards carry theirs: in its own red letters with a black shadow, from the left edge, along the bottom (a name too long for the card ends in "..."). It is drawn as the card is shown, never written into the picture. For a picture that already shows its name, put `card_name=off` in `mod.ini` (`on` is the default). `tnfs-studio car card DIR --generic` makes a plain `card.png` (a plain ground; the game draws the name on it); a piece made before the pictures came has no card and stops loading until it has one (`car piece: my-car refused: ...`, `PIECE.CARD`): run that command on it.

**The gallery.** A click on the added car's card opens its gallery, laid out as the CD's Car Showcase pages: Gallery is the big photo (`photos=`; with several, arrows inside it, Left and Right or a click on its sides go round them, and after the last comes the first), the small tile has the card's picture and no label (`photo_small=`), General the figures (Year, Power, Weight, Category) and History the text. The big photo falls back to `gallery.png` and then `card.png`, the small one to `card.png`, and a tile with no picture at all shows a placeholder in the page's tone; the game crops each to fill its tile (225x127 and 139x108, so 450x254 and 278x216 are good sizes). History starts with the head line "Name (year) - by Author - vX" and goes on with `description=`; it scrolls by itself (3 s at the top, a line every 2 s, 3 s at the end), stops while the pointer or the keys are on it, and the mouse wheel moves it. The Mechanical tile stays empty. The tile at the top left is SETUP: Enter or a click opens a panel (the CD's stone, opening as its lists do, with their sound) with the rows Color, Skin (when the car has skins), Transmission, ABS and Traction; Up and Down choose a row, Left and Right (or a click on its arrows) change it, the panel is a modal: Escape or a click outside closes it (with the shutter and its sound). Color and Skin show a small preview box; Transmission, ABS and Traction control are the Options screen's own settings, with its words, changed by Enter or a click on the row and kept at once; a car with no ABS or traction control shows n/a. The same SETUP tile and panel are on the CD's eight showcase pages (there Color is the car's livery choices). The page's sepia background is tinted with the car's first `paints=` colour (fixed, whatever the panel picks); without paints it stays sepia. Leaving the gallery (Done, Escape, or Previous and Next to another car) keeps the pair in `restomod.cfg` (`look.my-car=stripes,Blue`, docs/settings.md) and the next race wears it. The author, description and figures are drawn in the menus' small face, which has no comma, no long dash and no accents: commas and any byte outside ASCII are left out there. The mod's `name`, `short_name` and the paint names must be printable ASCII (the big face draws them as they are); a mod with anything else is refused.

**Skins.** A skin is a folder `skins/<name>/` (1 to 16 of `a-z 0-9 -`; up to 15 skins) holding textures to lay over the CFM's: `<tex>.png` over the high-detail bank's texture `<tex>`, `<tex>_lo.png` over the low-detail one's, each the size of the texture it replaces. An indexed PNG is taken as it is: its indexes are read as the bank's, whatever palette the file carries (so make it with the bank's, as `tnfs-studio unpack` writes it; the race's special colours `0xFA`..`0xFF` come only this way). An RGB one is fitted to the bank's palette by nearest colour, never to the special colours. Skin is "As it comes" (the CFM's own textures) or one of these folders.

**Paints.** Color is "Own colour" or one of `paints=`. A paint shows only where the skin has masks: `<tex>_mask.png` (and `<tex>_lo_mask.png`) in the skin's folder, as Car masks describes; the skin as it comes takes the masks in `car_masks/<STEM>/`. With no masks the Color choice is passed over. With the Car Colours switch off, and for an added car racing as a rival, the car wears its skin as it comes and its own colour.

`validate` and the game check the skins as they check the rest (`PIECE.SKIN`, `PIECE.PICTURE`): every file a texture the bank has, of its size; a mask a PNG no larger than 1024 a side; up to 256 files a skin.

**What it does not do yet:** the added cars are not in the grid of a single race or a tournament, and Best Times does not show their tables; a car brings no engine sound, cockpit or dash paint of its own (`dash_from` borrows a stock car's cockpit); the original gallery's Previous and Next do not reach them.

### Making a car with tnfs-studio

Three commands take you from "a car based on the Viper" to a zip the game installs. The car starts as an exact copy of its template; you change it afterwards.

    tnfs-studio car new --template viper --id my-car --stem MYCAR --out mycar --name "My Car"
    tnfs-studio validate mycar
    tnfs-studio mod pack mycar my-car.zip

- `car new` takes one of the eight race cars, by its word (`viper`) or its stem on the CD (`DVIPER`), from the CD in `original` (`--data` for another folder). It writes `mod.ini` (`type=car`, the stem, and `sound_from` and `dash_from` set to the template's word; no `ai_level`, so the car is driven at its own pace), the template's CFM, PDN and PBS under the new stem, and the port's car and dash masks of the template from `data` (`--port-data` for another folder). It also writes `card.png`, the car's picture for Control Central (309 x 166): a plain ground, the game drawing the name on it; `tnfs-studio car card DIR --generic` makes it again (`--force` over an existing one). `tnfs-studio car card DIR --tnfs path/to/tnfs` takes the car's pictures from the game instead: it races the car alone, from behind, and crops the frame to `card.png` (309 x 166), `rival.png` (284 x 117) and `gallery.png` (320 x 200); `--track`, `--node` and `--time` (`day`, `dawn`, `dusk`, `night`) choose the background. It replaces the generic card freely, its own pictures only with `--force`. A stem of the CD's, a bad id, or an output folder that already holds the piece stop it before anything is written; `--force` replaces the piece's files.
- `validate` on a folder with a `mod.ini` reads the card (a card it cannot read is an error) and checks a car piece exactly as the game does before it lets the car race, plus what the race looks up in the model and the original executable's limits for the rest. On a `.zip` it makes the installation's own check first. `--port-only` turns errors into warnings, as for any file.
- `mod pack` checks the folder as `validate` does and writes the zip only if it passes: `mod.ini` first, the rest by name, nothing compressed, so the same folder always gives the same zip. A file the game does not install (a `.docx`, an `.exe`) stops it and is named; what macOS and Windows add on their own (`.DS_Store`, `Thumbs.db`) is left out. An existing zip is kept unless `--force`.

To race it before packing: `tnfs --mods mycar --drive TR1 --car my-car` in Restomod.

To change the car, edit its files with `tnfs-studio unpack` and `tnfs-studio pack` (the textures as PNG, the specs and the PDN as text), then `validate` again. `pack` keeps the template's vertex, uv and polygon counts; a model of your own goes in with `car import` (Importing a model, below).

What the game asks of a car's model (the checks `validate` makes, `PIECE.*` and `CFM.*` in its output):

| What | Limit | Kind |
|------|-------|------|
| Vertices, polygons, uvs | no cap but memory (the CD's cars reach 132 vertices and 86 polygons) | soft |
| Polygons in one draw list | past 1000 the rest are drawn unsorted | soft |
| Sides of a polygon | 3 or 4 | hard |
| Textures in one mesh | 256 (one byte a polygon); every index inside its table (`PIECE.MODEL_INDEX`) | hard |
| Texture size | any, not a power of two; a side under 32768 (the CD's run from 4x1 to 334x102) | soft |
| Texture repeat | none: coordinates are clamped at the edge | hard |
| Texture format | 8-bit with a palette | hard |
| Colours | up to 250 indexes, fitted on screen to the race's palette: about 23 of the car's own for the player's car (8 for a rival), 72 shared by the cars, and the track's | soft: it loses colour, never fails |
| Polygon groups | every polygon of the high-detail mesh in `inside`, `surface`, `outside` or `convex`, or it is not drawn close up (`PIECE.MODEL_GROUPS`, a warning) | hard |
| Named parts and points | tyres (`tire1`..`tire4`, `frntires`, `rertires`), `left_turn`, `right_turn`, `inside` and `surface` (at an index under 8), the exhaust point `smok`, the four lights `lt_frnt` `rt_frnt` `lt_rear` `rt_rear` and their points `fl0` `fl1` `fr0` `fr1` `rl0` `rl1` `rr0` `rr1` (`CFM.REQUIRED_NAME`, `PIECE.MODEL_NAME`) | hard: the race hangs without them |
| Meshes | a high-detail and a low-detail one (the CD's low ones: 6 faces, 3 textures, seen from afar) | hard |
| Collision body (PDN) | its three half sizes match the model's (measured as the race does, x 512) within a tenth, or are 0 (`PIECE.BODY_SIZE`, a warning) | should match |


#### Importing a model

`car import` puts a model made in any 3D editor into a piece made with `car new`: an OBJ, its MTL and its PNG textures become the piece's CFM (both meshes and both texture banks), and the body's size goes into its PDN.

    tnfs-studio car new --template viper --id my-car --stem MYCAR --out mycar --name "My Car"
    tnfs-studio car import mycar --obj mycar.obj --scale 130
    tnfs-studio validate mycar
    tnfs --mods mycar --drive TR1 --car my-car
    tnfs-studio mod pack mycar my-car.zip

**Name the objects.** The OBJ's objects (`o` or `g`, any case; a Blender number after a dot is ignored, so `wheel_fl.001` is a second `wheel_fl`) say what they are:

| Object | What it becomes |
|--------|-----------------|
| `wheel_fl`, `wheel_fr`, `wheel_rl`, `wheel_rr` | the wheels, each exactly once. The front two turn with the steering (`left_turn`, `right_turn`); every face of a wheel shows the race's tyre frames, the whole bitmap on each face, so model a wheel as the game's cars do: one flat face on each side |
| `inside...` | polygons drawn inside the car (the seats, the dash seen through the glass) |
| `outside...` | polygons drawn last, outside the body (spoilers, mirrors) |
| anything else | the body (`surface`) |
| `smok`, `fl0` `fl1` `fr0` `fr1` `rl0` `rl1` `rr0` `rr1` | a point: an object with no face, its first vertex the place. `smok` is the exhaust |
| `lt_frnt`, `rt_frnt`, `lt_rear`, `rt_rear` | a light: its first face is the lamp's polygon |

A point or a light the OBJ does not name is made, and the import says so (`made the point smok ...`): `fl0`..`rr1` at each wheel's bottom front and back corners, `smok` at the back of the body near the ground, each light on the body's polygon nearest its corner. A wheel missing or named twice stops the import: the race hangs without both front wheels.

**Axes and size.** Export from Blender with its defaults (Y up, -Z forward): the car's front towards -Z, its right towards +X, the wheels' bottom at the ground. The importer turns that into the game's frame (it negates z). The game's units are about 130 to a metre (the Viper's body is 578 long for its 4.45 m); the CD's race cars measure 229 to 258 wide, 140 to 166 high and 539 to 601 long. `--scale` multiplies the OBJ's coordinates: `--scale 130` for a model in metres. The import prints the body's size in units.

**Textures.** Each material's `map_Kd` names a PNG, found beside the MTL or below `--textures DIR`. Its colours are fitted to the template's palette (the import says how many pixels moved); a pixel with alpha under 128 becomes transparent (the race draws through it: the import marks the polygons that use that bitmap). A PNG with the bank's own palette keeps its indexes, so the race's special colours (`0xFA`..`0xFE`, docs/modding/car-model.md) come only that way. The material's first four letters name the bitmap, so the car's masks (Car masks, above) can name it too; the import removes the template's masks, which painted the template's textures. Texture coordinates are clamped at the bitmap's edge, never repeated.

**What else is made.** The polygons' flag bytes, the texture references' other words and the header's unknown words are copied from the template (the donor; `--donor` picks another of the eight race cars, by default the card's `sound_from`). The front wheels turn by the donor's angle (`--steer DEGREES` for another; the CD's turn 22 to 37). The tyre frames are the donor's. Faces of more than 4 corners are cut into triangles. Without `--low`, the far model is the same as the near one (the race draws it beyond about 6400 units: a rival far ahead; with `--raw`, the template's own far model stays instead); `--low FILE.obj` (with `--low-textures DIR`) gives one of your own, with the same names, its polygons drawn in one group. The PDN's three half sizes are measured from the model (`--keep-pdn` leaves the PDN as it was, and the import prints the sizes it would have written); the rest of the PDN stays the template's.

**Round trip.** `tnfs-studio unpack` writes a CFM's meshes as OBJ with their tables (`.tsv`) beside them; `car import --raw --obj d/c0/mesh.obj --low d/c2/mesh.obj` takes them back, everything the OBJ does not carry from the tables, so a car of the CD comes back with the same meshes and bitmaps. Raw mode retouches the template: its banks stay byte for byte (the bitmaps, their order and the tyre frames') and so do its masks, and without `--low` its far model stays as it is; an unchanged export of a car `car new` made comes back byte-identical. Edit the geometry in the OBJ (a face's row in `mesh.polygons.tsv` goes with it, in order).

**Through Blender.** Beside each `mesh.obj`, `unpack` writes `mesh.blender.obj`: the same mesh for Blender, in metres (130 units to a metre), Y up and −Z forward, every texture coordinate 0..1 of its bitmap, with `mesh.mtl`'s textures. To change a car's shape or its texture mapping:

    tnfs-studio car new --template viper --id my-car --stem MYCAR --out mycar --name "My Car"
    tnfs-studio unpack mycar/SIMDATA/CARFAMS/MYCAR.CFM d
    (Blender: File > Import > Wavefront OBJ, d/c0/mesh.blender.obj with the defaults; edit; File > Export > Wavefront OBJ, high.obj, with the defaults)
    tnfs-studio car import mycar --raw --obj high.obj --tables d/c0
    tnfs-studio validate mycar

The textures show in Blender's Material Preview (`mesh.blender.obj` has its own `mesh.blender.mtl`, for the preview only): the tyres show their first frame, `tyr1`, and a texture with the transparent key colour is shown from a copy beside the OBJ, `mesh.blender.<texture>.png`, where the key is see-through; the bank's PNGs stay as they are. Set each image's interpolation to Closest to see its texels. Move vertices and texture coordinates freely, and change a face's material to another of the car's (`t<number>_...`); Blender may write the faces in another order, which does not matter. Do not add, delete, split or flip faces or vertices, and leave the triangulate option off: the parts, groups, lights and points name vertices and polygons by their place, so the import refuses another count, a face on vertices that are no face of the car, and a flipped face (flip it back), pointing to the import without `--raw` for a new shape. Everything the OBJ does not carry comes from `d/c0`'s tables, and `d/c0/mesh.obj` must stay as `unpack` wrote it: each texture coordinate goes back on its own row there, so an unchanged mesh comes back the same. `--low low.obj --low-tables d/c2` does the far model the same way; without `--low` the car's far model, its bitmaps and its `_lo_mask.png` masks stay as they are. The body's half sizes are measured again as for any import; `--keep-pdn` keeps the PDN's.

**Overwriting.** The import replaces the piece's CFM only while it is the one `car new` copied; once you have imported, `--force` replaces it again. Everything is read and made before anything is written, so a refused import leaves the piece as it was.

## The mask tools

The masks are painted in PSDs and read back by scripts in `tools/`. The Python ones need Pillow; the `from_psd` ones also `psd-tools` (`pip install psd-tools`). The two programs are built with the game (`track_mask_draft`, `track_lamp_marks`).

**`track_mask_draft <data dir> <output dir> [track ...]`**: `<data dir>` holds the game's `SIMDATA/ETRACKFM/<TRACK>_001.FAM` (the midday art). For each track named (all sixteen when none is) it writes, in `<output dir>/<TRACK>/`, `slot_<NN>.rgb.png` for every road and shoulder texture (NN the slot, variant 0) and `scenery_<NN>.rgb.png` for every board (signs, posts, panels), in the track's own colours, a transparent texel transparent. **These files hold the game's pixels: keep them out of any repository.** A track whose art cannot be read is reported and skipped, and the exit code is 1.

**`tools/track_mask_psd.py <drafts dir> <output dir> [--png <dir>] [--all]`** turns those drafts into one PSD per texture, `<output dir>/<TRACK>/<name>.psd`, at the texture's size: a locked `texture` layer, an empty **`mask`** layer at 50% (yours: paint what is reflective in any colour but black; yellow `255, 255, 0` is a lamp), and a hidden `draft` layer with a first guess (`tools/track_mask_auto.py`: the cat's eyes of a road, the letters of a green sign, the symbol and border of a sign, the stripe of a post) to copy into `mask` or touch up. The game never reads `draft`. By default a track with a list of candidates gets PSDs only for the textures worth painting, `--all` writes every texture. `--png <dir>` also writes each guess as `<dir>/<TRACK>/<name>.png`, white reflective and black not, which is the format the game reads: drop them in a mod's `track_masks/` to try the guesses without painting.

**`tools/track_mask_from_psd.py <dir of TRACK/<name>.psd> [<output dir>]`** reads each `mask` layer into `<output dir>/<TRACK>/<name>.png` (default: `data/track_masks` next to `tools/`; give your mod's `track_masks/` instead). A texel is reflective when it is opaque enough (alpha 128 or more) and not black (brightest channel 64 or more); a yellow texel (red and green 192 or more, blue 64 or less) is a lamp instead; the layer's own opacity does not matter. A mask with no lamp is a grey PNG, 255 reflective, 0 not; one with lamps is RGBA, white reflective, yellow `255, 255, 0` the lamps, transparent the rest.

**`tools/track_lamp_psd.py <textures dir> <masks dir> <output dir> TRACK/slot_NN ...`** makes the PSD of a mask that already exists only as a PNG (the lamp masks `tools/track_lamp_marks` drew): the texture from `tools/track_mask_draft`'s output under a `mask` layer holding the PNG's yellow and white, so it can be edited as the others; `tools/track_mask_from_psd.py` reads it back into the same PNG.

**`tools/dash_mask_psd.py <dump dir> <output dir>`** writes the dash PSDs. The dump is made by the game: run a night race in the car's cockpit with `TNFS_DASH_DUMP=<dump dir>`; it writes, once per dash, `<STEM>.ppm` (the dash without the wheel), `<STEM>.mask.png` (a draft of the two gauge ellipses as dials) and the steering frames the dash is rebuilt from. `<output dir>/<CAR>/dash.psd` has the layers `dash` (locked), `volante` (the straight-ahead wheel, hidden, a guide), `nunca visible` (what no steering pose shows: paint there does nothing) and the empty `mask` at 50%. **An existing file is never touched**: a new draft goes beside it as `dash_draft.psd`, `dash_draft_2.psd`, and so on. After a new dump, `--update <dump dir> <PSD dir> <output dir>` rewrites only the `dash`, `volante` and `nunca visible` layers of the PSDs you painted and copies every other layer (`mask`, `color`, `limpia`) byte for byte, into the output dir, which must not be your PSD dir.

**`tools/dash_mask_from_psd.py <dir of CAR/dash.psd> [<output dir>]`** reads the `mask` layer into `dash_mask.png` (colours in the Dash masks table above; a pixel with alpha under 128 or black is not marked, yellow, white or light grey is lit, blue is the dash's art, green a dial, red a wiper blade the dash draws, cyan the nose seen through the windscreen) and, if the PSD has a **`color`** layer, `dash_color.png` too (a PSD without one gets none, and an old `dash_color.png` there is deleted). A layer named `limpia` (or `wipers`) holds the wiper blades: whatever is painted on it, in any colour but black, is a blade.

## Settings and launchers

Every key of `port.cfg` and the three mode files, its default, range, menu and `TNFS_*` variable is in [settings.md](settings.md); copy nothing from there into a mod, point to it. The development variables (`TNFS_START_NODE`, `TNFS_FRAMES`, `TNFS_SHOTS`, `TNFS_VIEW`...) drive headless runs, with `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`, `timeout -k` and a scratch `--user` folder so the player's files stay out.

*Colour palettes (.aco files, as used for the paint colours) are not in the repository; only their use is described where a tool takes them.*

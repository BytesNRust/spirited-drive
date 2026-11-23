# Working on the game's own cars and tracks: from the CD's files to the game repo

How the owner edits the 20 original cars and 16 original tracks in Studio and gets the result into the game repo, as it works now. Detail of each window is in `track-editor.md` and `track-light.md`.

## What you have
- A **workspace** (File > Workspace): `<workspace>/cars/<kit>/` for each original car, `<workspace>/tracks/<TRACK>/` for each original track. Nothing in it is overwritten by Studio unless a step below says so.
- The **game repo's `data/`**, told to Studio once: File > **Game repo folder...** (kept in the workspace's `workspace.txt`), or `--port-data DIR` for one run. For a car the repo also needs `core/formats/car_paint.c` beside `data/` (the colours are written there).
- The owner's PSDs (`--art-dir`, read only; Studio copies them, never moves or writes them).

## 1. Put the originals in the workspace (once)
- **Cars:** File > **Export original car** makes the kit of one car (card, `art-source/`, `art-generated/car_masks`, `art-generated/dash_masks` for the cars you drive). Headless: `--export-car <FOLDER> --into <workspace>/cars --cd <CD> --port-data <data>`.
- **Tracks:** the **Add originals** button (main menu, Tracks column; also in File > Open original track...) puts the 16 in `<workspace>/tracks/` (it needs the workspace and the game's `data/`: `--port-data`; the PSDs come from `--art-dir`; headless: `--steps add-originals`): `original.ini`, `light/track_light/<TRACK>.txt` (the one light file, copied from `data/`), `art-source/masks/` (the PSDs, backups with a second dot skipped) and `art-generated/track_masks/`. Press it again later (it reads **Update originals** in the Open window once the tracks are there): what is there stays as it was, a file that is missing is added. On a workspace made before the one-file light format it copies the one file and **takes the old light files away** (`light/track/<TRACK>.txt`, `light/track_light/<TRACK>{A,B,C,N}.txt`): nothing reads them.

## 2. Edit
- **Track light:** File > Open original track... opens the Track light window on the track's own `light/` folder; every edit is saved into `light/track_light/<TRACK>.txt`. Test drive and the photo show the working copy.
- **Track masks:** paint the PSD in `art-source/masks/`; the PNGs the game reads are made when the track opens and when the PSD is saved. A PNG changed by hand is kept (state *edited by hand*) until you press *Update masks from PSD* and answer *Replace* in its question (*Keep* leaves it, *Cancel* does nothing).
- **Car masks and dash:** edit the PSDs of the kit (`art-source/`) and press **Update masks from PSD** in the car's Masks window: it reads the PSDs newer than their PNG and writes the PNGs in `art-generated/` (the PSDs are never written); a PNG edited by hand is asked about first (Replace / Keep / Cancel). **Update + dash photo** does the same and then takes the cockpit photo at night; **Show PSD folder** opens `art-source/masks`. The PNGs in `art-generated/` are made from them. Colours: the kit card's `paints=` line.

## 3. Copy to game repo
File > **Copy to game repo** (cars and tracks) lists what would change in `data/` and then copies only that:
- Tracks: `track_light/<TRACK>.txt` (by lines), `track_masks/<TRACK>/*.png` (by texels).
- Cars: `car_masks/<folder>` and, for the cars you drive, `dash_masks/<folder>`, compared **by picture** (a PNG saved with other bytes but the same texels is not a change); the colours go into `core/formats/car_paint.c`. A card with more colours than the game's table holds (up to 8) is not refused: the colours are left alone and the window says so.
- It never commits or pushes. Review the diff in the repo and commit as usual. Asked again, it says there is nothing left to copy.

## Checks
`editor_copy_originals` (all 20 cars and 16 tracks through Copy in a clean temp workspace), `editor_original_track_flow_a` and `_b` (tracks, including an old-format workspace), `editor_original_psd_newer` (a newer PSD, the Replace / Keep / Cancel question), `editor_e2e_car` (the 911 and LDiabl dash Copy). They need the CD's `original/` and skip without it.

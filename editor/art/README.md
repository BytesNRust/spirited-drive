# Studio's generic art

`GENERIC.FAM` (551,908 bytes) is the stage art a new Studio project copies into its own folder (as `GENERIC.FAM`, `art=GENERIC.FAM` in project.ini), so the
project builds on any Mac with nothing of the source tree beside it. In the app it lives in `Spirited Drive Studio.app/Contents/Resources/`
(tools/package_macos.sh); a build tree has it beside `tnfs-track-editor` (CMake copies it). The editor looks for it in `editor/art.c`.

It holds no byte of the game's CD. It was built with

    tnfs-studio fam build generic_stage.txt --out GENERIC.FAM

from `generic_stage.txt` (here, with the paths of the art folder it was built in) and these pictures:

| bank | picture | origin |
|---|---|---|
| terrain 1 gravel, 2 rock, 3 grass, 10 planks | Gravel001, Rock035, Grass005, Planks001 (512x512 colour maps) | ambientCG, CC0 1.0 |
| terrain 0 road | asphalt with our own yellow centre line and white edge lines (`make_road.py`) | Asphalt010 (ambientCG, CC0 1.0) and our own lines |
| terrain 4..9 fillers | flat white | ours |
| horizon | a flat vertical blue gradient, 256x64 | ours |

ambientCG's licence: "All assets are released under the Creative Commons CC0 license, making them free to use without attribution - even in
commercial circumstances." (checked 2026-11-18; the pages are https://ambientcg.com/view?id=Gravel001, Rock035, Grass005, Planks001, Asphalt010.)
The contract of the FAM (11 terrain banks, bank 10 with letter C only) is the one `track new --generic` checks (core/formats/track_template.c).

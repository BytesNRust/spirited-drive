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
| terrain 4 tarmac shoulder (phase 2 step 2b, slot 12) | Asphalt010 as it is: the road's asphalt without its lines | ambientCG, CC0 1.0 |
| terrain 5 and 6 lanes (phase 2 step 4, slots 15 and 18) | Asphalt010 with our own lines (`make_lanes.py`): 5 a dashed white line inside and the white edge line outside, 6 a dashed white line inside and the yellow centre line outside (a 1+2 or 2+1 stretch's three 5 m strips; the third is terrain 0) | Asphalt010 (ambientCG, CC0 1.0) and our own lines |
| terrain 7..9 fillers | flat white | ours |
| horizon | a flat vertical blue gradient, 256x64 | ours |

ambientCG's licence: "All assets are released under the Creative Commons CC0 license, making them free to use without attribution - even in
commercial circumstances." (checked 2026-11-18; the pages are https://ambientcg.com/view?id=Gravel001, Rock035, Grass005, Planks001, Asphalt010.)
The contract of the FAM (11 terrain banks, bank 10 with letter C only) is the one `track new --generic` checks (core/formats/track_template.c).

## The shades under a car

A car's shadow turns each road pixel into the palette's nearest entry to its colour darkened by 0.6 (game/sys/palette.c palette_shade_table). A palette cut
for the pictures alone has no dark yellow, so the yellow centre line went green under a car. Two things keep it yellow:

- `fam build` (core/formats/fam_image.c reserve_shades) keeps 8 entries back and fills them with the shade of whichever entry's shade is farthest from the
  palette, while it is farther than 450 (squared, 8-bit units: the CD's own AL1 has its yellow's shade at 450). They go in at entry 0x58: the race loads only
  the first 0x60 colours of a stage's palette (game/viewer/drive_palette.c TRACK_FIRST) and maps the rest onto them.
- the race hands an entry nothing draws with to the cars' paint, so the road's picture must use the dark yellow: `road_rim.py ROAD_LINES.png OUT.png` makes
  `terrain 0`'s picture with one row of the yellow band's outer edge in it (one row of 512: it averages away in the smaller sizes).

tests/test_generic_art_shades.c checks this FAM. A project's copy of an older GENERIC.FAM is replaced when it is byte for byte one Studio carried before
(editor/project.c PROJECT_OLD_ARTS: add the SHA-256 of the file you retire there).

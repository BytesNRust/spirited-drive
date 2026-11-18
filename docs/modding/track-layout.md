# Making a track from a text layout

`tnfs-studio track new LAYOUT --template SLOT --out DIR [--stem NAME]` builds a point-to-point track (a stage, like
Alpine, City and Coastal) from a few lines of text. The TRI is generated; the INFO and the FAMs (road textures and
scenery objects) are copied from the template slot, so every texture id stays valid. Add `--piece --id ID --stem STEM` to make a track piece the game can load (below); `--generic ART.FAM --piece` takes the port's own template and an art file instead of a CD slot ([../modding.md](../modding.md), *Adding a track*).

```
# comment
section SEGMENTS [turn=DEG] [grade=PERCENT] [bank=DEG] [width=VERGE]
```

- `SEGMENTS`: length in terrain segments (4 road nodes each, about 6 units per node). 8..599 in all.
- `turn`: total heading change over the section, positive to the right (at most 6 degrees per node).
- `grade`, `bank`, `width`: the value reached at the end of the section; the generator ramps linearly from the
  previous section (start: 0, 0 and the template's verge). A key left out keeps the previous value; `turn` does not carry over.
  Ranges: grade ±30 %, bank ±20 degrees, width 8..120 (the half road width in the node's verge byte, 16 terrain units each).
- Heights follow the grade, so a closed circuit is not made (the generator writes a stage: `circuit_segments = 0`).

What is written, from the node fields found on the CD (the formulas are checked against TR1 and AL1 in
`tests/test_track_gen.c`): position advances 395000 per node along (sin h, grade, cos h); `slope` is atan(grade) and
`slant_a` the bank, both 14-bit angles (0x4000 = full turn, negative values wrap); `orientation` is the heading;
`z_orientation`, `slant_b`, `x_orientation` are cos h·cos b, -sin b and -sin h·cos b times 32767. The cross-section of
each terrain strip is the template's strip 10, with `right[0].x`/`left[0].x` moved by 16 per unit of width change.
The scenery placements of the template are dropped (they name template nodes); add some in `scenery.tsv`
(`tnfs-studio unpack` / `pack`).

To play it, make it a piece: a changed TRI in a mod's slot is not given to the game (`--mods` serves no file that changes a race), and the piece is a track of its own. In Restomod, in a scratch `MYUSER` folder:

```
tnfs-studio track new loop.trk --template AL1 --piece --id my-loop --stem MYSTAGE --out build/loop --data original
mkdir -p MYUSER
printf 'profile=restomod\n' > MYUSER/port.cfg
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy tnfs --user MYUSER --data original --mods build/loop --drive my-loop --racers 1 --screenshot out.ppm
```

`--drive` takes the piece's `id`. To install it and put it in an edition, follow *Try your piece* in [../modding.md](../modding.md). `tests/track_new.sh` walks the pieces made from two templates, from the first node to the last.

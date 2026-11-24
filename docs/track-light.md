# Track light file

In Restomod '26 the light of a track is one text file, `data/track_light/<TRACK>.txt` (`CY1.txt`). A mod can carry its own copy: `<mod>/track_light/<TRACK>.txt` is tried first. The file holds the **facts** of the track (where its
tunnels and halls are, whether it snows) and then **one block for each time of day** (A dawn, B day, C dusk, N night). The light rows are not stored: they are made when the track loads, from its terrain and tunnels, and a
block says only what is retouched. A track with no file has its light made in code with no retouches. Stock '96 and Restored '96 do not read the files at all.

A line the game does not understand refuses the whole file, with its line number in the log (`track light: <file> is not well-formed (line N), the light is made as before`): a bad file never half-applies.

## Grammar

`#` starts a comment; blank lines and indentation mean nothing; a word is a word (`50left` is no line). The first line with a word is `track STEM`, the file's own track. Facts come before the first `time`; a fact after it is a bad
line. A time appears at most once; a time with no block is the made light with no retouches. Node numbers are inclusive, 0 to the track's last node (the number of nodes is the track's, the file does not say it).

    track CY1
    tunnel 1064 1171            # a tunnel (the same at every time of day)
    tunnel 1 62 lit             # a hall: a roofed straight lit by its own lamps (TR7's steelworks)
    snow 0                      # snow 1: the track's rain is snow
    snow_map FROM TO            # a snowy track's road art: texture slot FROM is drawn as slot TO

    time A
      sun 150 20 55             # AZIMUTH ELEVATION SHADOW (degrees, degrees, percent of light left in full shadow); a part may be -
      tunnel_look 104 ff5a0d    # no nodes: the default look of the time's tunnels: light in percent (100 = row 255) and the colour they are cast in
      tunnel_look 1064 1171 90 lamps 220 reach 2
    time N
      tunnel_look 115
      lamp 130 150 ff2000:80 8
      brightness 852 943 60 left
      tone 10 20 p2 p4 ff8000:50
      rows 5 6 255 255 255 255 255 255 255 255 255 255 255

| line | meaning |
|---|---|
| `sun AZ EL SHADOW [REACH]` | the time's sun (at night the moon). Read and kept; the generator uses it where the sun's shadows are on. Every number is finite (`nan` and `inf` are refused). |
| `tunnel_look [A B] [LIGHT] [COLOUR] [lamps S [COLOUR]] [reach R]` | how the tunnels look at this time. Without `A B`: the default look of every tunnel with no look of its own. With `A B`: those nodes, which must lie inside a `tunnel` fact. LIGHT = percent of full light inside, COLOUR = `RRGGBB[:strength]` or `r/g/b`, `lamps S` = the lamps' strength in percent (0..1000), `reach R` = the pools' reach in nodes (0.5..4). What a line leaves out comes from the default look, and then from the code's. A hall takes only `lamps` and `reach`. |
| `lamp NODE PERCENT [COLOUR[:S]] [RADIUS] [STRIP]` | a street lamp or any light: its light in percent (1..1000), its colour (the warm street lamp's when none) and radius in nodes (1..64). STRIP (last word, as in `brightness`) is where across the road it stands: the mean place of those points of the row; with none it stands at a scenery post of its node, else at the road's centre. Allowed at every time. |
| `brightness A B PERCENT [STRIP]` | the light of those nodes times PERCENT (0..1000) on the strip (the whole row when none). |
| `tone A B [STRIP] RRGGBB:PERCENT` | a colour cast on the strip (exact: a polygon takes it when its four corners have it). 16 different tones a time at most. |
| `rows A B p0..p10` | the hand override of the made rows of those nodes (11 values, 0..255). A time's `rows` lines may write each node of the track 64 times in all (a longer file is refused at the line that passes it). |

STRIP is `pA pB` (the points A..B of the row's 11, inclusive; `pA` alone is one point), a list in one word with commas and no spaces, `p1,p3,p7` (any points, in any order: the ones that are not a run), or a word: `right` = p1..p5, `left` = p6..p10, `road` = p0 p1 p6, `all` = p0..p10. Point 0 is the centre, 1..5 a chain to the right,
6..10 one to the left.

Order of application: the made rows (terrain, tunnels, the looks) -> `rows` -> `brightness` and `tone` in file order -> the `lamp`s lift the row. A car reads the row by its lane after `brightness` and `tone` and before the
lamps' lift. At most 64 `tunnel_look`, `brightness` and `tone` lines in a time and 2048 `lamp` lines.

## What decides what

* A tunnel whose lamps are written at `lamps 0` is an unlit tunnel: the car in it has the same light on every node. With the lamps at any other strength (or not written, the default 100) the car keeps the alternation of the tunnel's lamps, its light dropping to 75 % on every second node (40 % in the exe-faithful level 0). The setting is per time and per tunnel: a `tunnel_look A B ... lamps 0` line unlights only those nodes. A hall always has lamps.
* The made rows of the day and the dusk fall to the tunnels' look only inside tunnels; the road points are full light outdoors.
* `TNFS_TRACK_LIGHT_FILE=0` ignores the files (the code's light only).
* What the file does not hold, the settings still decide: the night lift, the lamp pools' brightness (`city_lift`), the headlight beam.
* The weather (fog, rain, storm, snow) is chosen for the race, not in the file.

## The fingerprint test

`tests/track_light_fingerprint.sh` hashes the light the game makes for each of the 16 tracks at each time of day (SHA-256 of the rows, tunnel flags, halls, lamps and looks) against `tests/track_light_fingerprint.txt`. A change of
a file or of the generator that changes a light shows there; one that is meant is written in the same commit with `UPDATE=1 sh tests/track_light_fingerprint.sh <tnfs> original/`. It needs the CD's data (`original/`).

## The Studio

The **Track light** window of `tnfs-track-editor` edits these files (editor/light_track.h is its model: a file that was not changed is written back byte for byte, and a change the game would refuse is not
made, with its reason in red). Open it from the main menu: Tracks > **Open original track...** (a list of the workspace's 16 originals; **Add originals** / **Update originals** puts the game's tracks and their light
file in the workspace first); File > Open original track... is the same list. (`--ask light --light-dir DIR` opens the window on a plain folder of light files, with a **Light folder** / **Choose...** row
instead of the original track's.) What the window has, top to bottom:

* **Track** and the four moments **Dawn / Day / Dusk / Night**; changing the track with unsaved changes is refused ("Save or Revert before another track"). **View** (Bumper, Dash, Chase near/mid/far, High) and **HUD**
  are the photo's camera. **Photo at node**: the slider, **-10 -1 +1 +10** and a typed node.
* **Test drive** (and **Stop test drive**) with the weather (Clear, Fog, Rain, Storm): the game in a window of its own on this track, moment and node, following every edit without Save. It is always Restomod 16:9 at
  render scale 3 (a 2562x1440 window) with every effect on (lamp halos, soft shine, reflectors, headlight beams, sun_shadow, light effects), whatever the person's own settings say.
* **Sun** (**Moon** at night): the compass and three sliders, **Direction**, **Height** and **Shadow strength** (100 = no shadow). The box to the left of each slider turns it on; unticked it is "auto" (the
  generator's own), and a sun with all three unticked is no `sun` line.
* **Tunnels**: one row for each `tunnel` fact: **Remove**, **From**, **To**, **Kind** (tunnel / hall; a fact of every moment) and, for this moment, **Light %**, **Colour**, **Lamps %** and **Reach** (the
  `tunnel_look` line; an empty cell is inherited from the **default** row, and the row's cell, from the code). **Add tunnel** adds one from the photo's node to 50 nodes on. **Lamps % = 0** is an unlit tunnel:
  the car there does not blink (see What decides what).
* **Lamps (N)**: **Add lamp** at the photo's node; per lamp **Remove**, **Node**, **Light %**, **Colour**, **Radius** and **Strip** (none, all, left, right, road: where across the road it stands).
* **Retouches**: **Brightness (N)** and **Tone (N)** with **Add brightness** / **Add tone** (from the photo's node to 50 nodes on), the lines' numbers and the strip bar (a click on a point 10 9 8 7 6 C 1 2 3 4 5 turns it on or off, so the points need not be a run; **All /
  Left / Right / Road** only set the picked points, which can then be changed one by one; the line under the buttons says the strip as the file has it, e.g. `p1,p3,p7`); **Remove** takes a line away. Tunnels, Lamps, Brightness and Tone are kept in the order of their first node: a line that is added goes to its place, and an edited one when its field is let go. The file's order is the order the game applies the lines in, so a Brightness does not pass one that shares a node and a point with it, nor a Tone one that shares a node (a node has one tone: the last line's): those stay as they are.
* **Save** and **Revert** (the file as it was read or saved), and on an original track **Copy to game repo** (the same window as File > Copy to game repo: it lists what would change in the game repo's `data/`,
  the light file and the changed mask PNGs, and writes only on Copy; the repo's folder is set once in File > Game repo folder...; Save first).
* **Light compare** (its own tab): **Off**, **Before** (the file on disk beside the edit) or **Four times** (dawn, day, dusk and night at the node).

The live mode `light_edit=1` in `port.cfg` (or `TNFS_LIGHT_EDIT=1`) shows in a race the node the car is at and the values there; **F10** holds the race where it is, and saving the light file in the folder given as
`--mods` makes the race read it again (that run keeps no records and no replays: a tunnel fact changes the grip).

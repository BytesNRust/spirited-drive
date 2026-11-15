# The car model (CFM), measured

What a car's `.CFM` holds and how it is laid out, measured on the 39 CFM files of the CD (78 meshes, 78 texture banks). `tests/test_mesh_layout.c` checks every rule marked *checked* on the CD; the counts say in how many files each one holds and the exceptions are named. The field-by-field reference is in `core/formats/chunk.h`, `mesh.h` and `fsh.h`; this page is what a writer must do to produce what the game reads.

## The file

A `wwww` table of 4 children, in this order: the high-detail mesh (`ORIP`), its texture bank (`SHPI`), the low-detail mesh, its bank. *Checked*, 39 of 39:

- the table is 8 + 4 x 4 bytes and the first child follows it at offset 24;
- each child starts on a multiple of 4, and runs to the next one's start: its declared length padded up to 4;
- the padding bytes mean nothing (they are what was in memory: `JJ`, `1d 1d 1d`, `84 84`, `88 45 fc`, `14 02 33`); in 34 files there is none. A writer may write zeros.

## A mesh

### The header (0x70 bytes)

*Checked*, 77 of 77 (all but LDIABLO's low mesh, below):

| Offset | Value |
|--------|-------|
| `+04` | the declared length of the chunk |
| `+08` | `0x2BC` |
| `+0C` | 0 (the game stores a runtime value) |
| `+14` | the number of the model's own vertices; the ones after it are the front wheels' displaced vertices (*Section A*). `+14` equals the vertex count in every low mesh and in the high meshes without wheels |
| `+64` | 0 |
| `+68` | the polygon table's offset |
| `+6C` | 0 |

### The order of the tables

*Checked*, 77 of 77: right after the header, with no gap between them,

1. polygons (12 bytes each)
2. uvs (8)
3. texture references (20)
4. section A, the steered parts (20)
5. section B, the polygon groups (28)
6. section C, the named points (12)
7. labels (12)
8. vertices (12)
9. the vertex map, 4 bytes a slot, to the end of the chunk.

An empty table keeps its offset: the place where it would start.

### The vertex map

*Checked*, 77 of 77. The map holds, in this order and with no gap:

1. for each polygon, in polygon order: its uv run (if its `uv_slot` differs from its `vertex_slot`), then its vertex run. Each run has as many slots as the polygon has corners: 4 for a quad, **3 for a triangle**. A triangle's "fourth" slot, which the game reads, is therefore the first slot of whatever follows;
2. for each part of section A, in order, its list of vertices;
3. for each group of section B, in order, its list of polygons;

and it ends exactly at the chunk's declared end.

A polygon's corner count is `flags & 7`: 3 or 4 in every polygon of the CD (1938 polygons: 260 triangles, 1678 quads). Its texture index is below the texture count in all of them.

### Polygon flags and bytes 1 and 3

High meshes: flags `0x84` (940), `0x8c` (526), `0x83` (252), `0x8b` (8), `0x04` (6). Low meshes: `0x84` (16), `0x8c` (190). Byte 1 takes `0x00`..`0x03` and `0x10`..`0x13`, byte 3 `0`, `1`, `2` or `4`. Their meaning is the renderer's (`game/gfx/object3d.h`); a writer copies them from a donor polygon.

### Section A: the front wheels

Two parts, `left_tur` and `right_tu` (the names are 8 bytes: `left_turn` and `right_turn` cut), in the high mesh of the 15 cars that have them (the eight race cars, their copies, TRAFFC and the Warrior). They are **the front wheels the steering turns**, not turn signals (`game/race/car_struct.h`, `car_wheels.c`). *Checked*, 77 of 77, its three words:

| Word | Meaning |
|------|---------|
| 0 | how many vertices the part moves (8 on the race cars, 4 on TRAFFC and the Warrior) |
| 1 | the map slot where its list of those vertices starts |
| 2 | its first displaced vertex: the first part's is `+14`, the next one's follows; the last part's run ends at the vertex count |

The displaced vertices are not positions: each is an **offset** added to the matching vertex of the list, scaled by the steering (`object3d_morph_part`). Measured on all 15 (a least-squares fit, residual 1.6 units at most): the offsets turn the wheel's vertices about a vertical axis (y offset 0, except the RX-7 and the 911 by 1 to 4 units) by 22 to 37 degrees, through a point on the wheel's outer side halfway along it (DVIPER: left wheel 26.7 degrees about x -121, z 180; right 29.0 degrees about x 123, z 181). The list holds the wheel's vertices in descending order: one run (ANSX: 63, 62, ... 56) in 20 of the 30 parts, two runs of 4 in the other 10 (DVIPER's left wheel: 108..105 then 88..85).

### Section B: the groups

Names (12 bytes, the text then whatever was in memory): `inside`, `surface`, `outside`, `NON-SORT`, and once `badge` (empty). Every high mesh has `inside` and `surface`; 25 have `outside`; none has `convex`. Every low mesh has a single group, `NON-SORT`, holding all its polygons. *Checked*: word 0 is 0, 1 or `0xFFFFFFFF` (`outside` takes `0xFFFFFFFF` in 12), word 1 the map slot of the group's list, word 2 its length, word 3 is 0.

Measured: every polygon is in a group, in every mesh. In 20 high meshes some polygons are in both `NON-SORT` and `inside` (86 polygons); no other two groups share one.

### Section C: the points

High meshes only. `smok` (the exhaust, 15 meshes), `fl0 fl1 fr0 fr1 rl0 rl1 rr0 rr1` (the lights' points, 13), `d` (5) and one nameless record. *Checked*: each value is a vertex index. Relative to the model's box (0 the least, 1 the most, x y z), typically: `fl*` at x 0.02, `fr*` at x 0.98, the `*0` points at z 0.84..0.89 (front) and 0.28..0.34 (rear), the `*1` points at z 0.70..0.73 and 0.14..0.18, y 0 for all; `smok` at x 0.5, y about 0.75, z 0.18 or 0.8 (the exhaust's end of the car).

### Labels

High meshes only: `lt_frnt rt_frnt lt_rear rt_rear` (the lights, 15 meshes), `bkll bklr bott` (12), and a few others once or twice (`W`, `bacr`, `mm`, ...). Each value is a polygon index, below the polygon count in all.

### Texture references

20 bytes: an 8-byte label, the 4-byte name of the bank's entry, two words. The labels the race looks up are `frntires`, `rertires`, `tire1`..`tire4` (15 high meshes); others seen: `pal`, `side`, `front`, `rear`, `tires`, `top`, `topp`, `bott`, `red`, `rtir1`..`rtir4`. Most references have no label. The two words: the first 0, `0x80000000`, `0x81000000` or `0x82000000` with few exceptions, the second a small number (0..252); their meaning is unknown and a writer copies them from a donor.

### The low-detail mesh

38 low meshes (LDIABLO's aside): 4 or 6 polygons, 3 texture references, 10 to 20 vertices; no parts, points or labels (*checked*); one `NON-SORT` group. Its box against the high mesh's, x y z: within 10 % in 27 cars, up to 25 % wider and 28 % shorter in the others.

## A texture bank

An `SHPI` with directory id `WRAP` in all 78 (*checked*). Each entry: a 16-byte header (type, a 24-bit "block size", width, height, four words), then its data: 768 bytes for a palette (type `0x22` 33 entries or `0x24` 60), width x height for a bitmap (`0x7b`, 464). *Checked*: palettes are 256 wide with the words (256, 0, 0, 0); every other entry is a `0x7b` bitmap; every bank has `!PAL`.

- **Order.** `!PAL` is the first entry in 31 of the 39 high banks and in all 39 low ones; in the others it is 2nd to 13th. The `!xxx` entry (the car's own colours) is in the high banks. The entries lie in the directory's order in 76 banks; in F512M and F512TR the file holds `circ` last though the directory names it first.
- **Gaps.** The first entry follows the directory in 75 banks, 2 bytes after it in 3.
- **After the data.** In 84 bitmaps an 8-byte record follows the pixels, starting with `0x7c` (an attachment); their block size is then 16 + the data. Otherwise 0 to 3 bytes of no meaning follow (and 4 to 8 in a few), and the block size is 0 (201 entries) or a value with no relation to the entry (`0xFFxxxx`, 272 bitmaps). A writer writes 0, or 16 + the data when it adds an attachment.
- **Pixels.** Indexes `0xFA`..`0xFF` are used by 374 of the 464 bitmaps, so a builder accepts every index; `0x00`..`0xF9` are the colours a texture may use freely. What the race does with each, when it fits a car's bank to the screen palette (`race_palette_remap_bank`, `game/race/race_palette.c`, after `0048f220`):

  | Index | What it is |
  |-------|------------|
  | `0x00`..`0xF9` | the bank's own colours (its `!PAL`), each matched to the nearest screen colour |
  | `0xFA`..`0xFD` | left as they are; on the police car (slot 2) they are the four lights the race animates |
  | `0xFE` | the car's own colour: the race puts there the screen colour it gave the car's slot (`slot_colours`) |
  | `0xFF` | transparent (`game/gfx/quad.c`, 00486950) |

  How much `0xFE` the CD's high-detail textures hold: on the race cars only `rsid` (the side seen from the right), and little: 1 % of its pixels on ANSX, CZR1, DVIPER, F512TR, F512M and TESTA, under 1 % on LDIABL, MRX7, P911 and TSUPRA. The rivals' reduced models put it on `rear`: 8 % (MANSX), 7 % (MCZR1, MDVIPER), 5 % (MMRX7, MP911), 2 % (MTSUPRA), 1 % (MLDIABL, MTRAFFC). TRAFFC's `lite` is 15 %. The other cars hold it in a few pixels of `rear`, `side`, `bott`, `frnt`, `topp`, and the Warrior in its tyres (1 %). So the body's paint is not `0xFE`: it is the bank's own colours (`!PAL`, and `!xxx`, the car's own colours, `game/race/car_model.c` `own_colours`); `0xFE` marks a few pixels, most on the rival models' rear, that take the slot's colour. An importer can leave `0xFE` out of a new texture.
- The declared length equals the chunk's extent but in 4 high banks, where the CFM's padding follows.

## What the game accepts

Shown with the port by `tests/model_build.sh`: a piece made with `tnfs-studio car new` from the Viper, its CFM rewritten through the builders (`formats/mesh_build.h`, `shpi_build.h`, `cfm_build.h`) by `tests/model_build_piece.c`, validates with no finding and races in Restomod. The original executable was not run on these files.

- **Nothing changed**: describing the Viper and building it again gives its CFM byte for byte, and the same frame.
- **New geometry**: 8 vertices and 6 quads (a wing over the tail) are drawn. The vertices go after the last one, past the wheels' offsets: the port moves every vertex of the model, and the parts' words stay as they were. The quads go in `surface`. A polygon is culled by its winding on screen (`game/gfx/object3d.c`, `draw_polygon`), so each face's corners must turn as the donor's do seen from outside: the wing's take the turn of the roof's top, measured on the car. Byte 1 bit `0x02` draws a polygon when its winding is the other way (the Viper's `topv`, `rsid` and `frnt` polygons come in two sets, one with it and one without); the wing copies a roof quad without it.
- **A new texture**: a 48 x 20 bitmap `xfin` added at the end of the high bank and of its directory, a texture reference naming it added after the 22 the Viper has (copying the roof's other fields), and the wing's quads pointing at it. Its pixels are indexes of the car's own palette, 0x00..0xF9.
- **Size**: the body cut in 4 twice over, 996 polygons and 1490 vertices (the Viper has 81 and 125), draws and races.
- Through all of them the PDN, the low mesh and its bank are the Viper's.

## Exceptions

- **LDIABLO.CFM, low mesh**: its texture references are 19 bytes long, not 20, so every table after them sits 3 bytes early and the map does not end on a slot. The game never opens LDIABLO (the race's Diablo is LDIABL; LDIABLO is not in the executable's names). The layout test leaves it out by name.

## Unresolved

- What polygon flags, bytes 1 and 3, the texture references' two words and section B's word 0 mean for the renderer: carried from a donor.
- Section C's `d` point and the nameless one; the labels other than the lights.
- The rule, if any, behind the steering angle of each wheel (22 to 37 degrees, left and right differ on the same car).
- The `0x7c` attachment's meaning, and the "block size" values that relate to nothing.

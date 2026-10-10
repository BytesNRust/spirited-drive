# Modding inventory — files, formats and limits of NFS_WIN.EXE (TNFS SE)

Phase 0 of BYTES-0009 (tnfs-studio A). Status: **draft for human review** (gate before phase 1).
Written 2026-10-10 on branch `feat/studio-a`. Nothing here changes code.

Evidence tags used throughout:

- **[D]** checked against the files in `original/` this session (script over every file of the type).
- **[X]** read from the EXE (decompilation, `strings.tsv`, `tools/peek.py`); the address is given.
- **[S]** already documented in the port's sources (`src/formats/*.h`, `docs/*.tsv`) and ported/verified there.
- **[A]** stated only by `reference/nfsspecs-auroux-1996.txt` (DOS, 1995). Not evidence for this EXE.
- **[?]** open: needs RE (see "Questions for the RE session").

Nothing is marked as a limit unless an address or a data check backs it.

## 1. What is on the CD (`original/`)

| Directory | Files | Types | Role |
|---|---|---|---|
| `SIMDATA/MISC` | 85 | 17 TRI, 17 `*INFO` (text), 17 QFS, 4 FSH, 4 FSB, 2 MMB, 24 `B00..B23` | tracks (TRI + INFO), windshield masks, warning/ticket sprites |
| `SIMDATA/NTRACKFM` | 46 | FAM | track bitmaps and scenery (`TRn_Tnn`, `_Mnn`, `_Rnn`) |
| `SIMDATA/ETRACKFM`, `GTRACKFM` | 16 + 16 | FAM | the 16 `XXn_001.FAM` stage tracks (E = English, G = German) |
| `SIMDATA/CARFAMS` | 64 | 39 CFM, 22 PDN, 1 FAM, 1 FSH (`CARPAL`), `LN32.CFG` | car meshes and per-car physics-ish data |
| `SIMDATA/CARSPECS` | 10 | 9 PBS, `BY_R&T` (4 bytes) | car performance |
| `SIMDATA/SOUNDBNK` | 38 | BNK | engine, horn, collision, front-end sounds |
| `SIMDATA/DASH`, `GDASH` | 36 + 36 | FMM + FSH | dashboard (windshield shape + bitmaps) |
| `SIMDATA/SLIDES`, `GSLIDES` | 63 + 63 | QFS | showroom slides |
| `FRONTEND/ART`, `GART` | 214 + 227 | QFS, INV, FSH, FFN, TXT | menus (bitmaps with the text drawn in) |
| `FRONTEND/SHOW`, `GSHOW` | 40 + 40 | QFS | car showcase screens |
| `FRONTEND/SPEECH`, `GSPEECH`, `MUSIC`, `MOVIE` | 96, 97, 15, 26 | EAS, ASF, TGV | audio and video |
| `GAMEDATA` | 6 | `CONFIG/CONFIG.DAT`, `MODEM/MODEMS.CFG`, 4 `REPLAY/*.RPL`; `SAVEGAME/` empty | settings, replays |

There is **no `PATHS.DAT`** on the CD [D]; the EXE has built-in defaults (§3).

QFS containers on the CD [D]: first bytes `10 FB` (RefPack) 473, `46 FB` (BTree) 8, `32 FB`/`30 FB` (Huffman) 15,
no `4A FB` (RLE) — matches `qfs.h`. Their payload is always an SHPI (directory ids `LN32` 212, `GIMX` 99, `SPIT` 2
= `GAMEMODE`/`GAMEMOD2`; counted over unique basenames, 254 of 496 files, because `ART` and `GART` share names).
SHPI entry types seen: `0x7B` bitmap 5472, `0x22` 6-bit palette 144, `0x24` 8-bit palette 41. All 102 `.INV`
files are 32768 bytes.

## 2. Slots are fixed in the EXE (decision "add slots or only replace")

Every car, track and sound bank is reached through a table inside the EXE. Nothing is enumerated from the
directories. **Evidence points to "replace only" for Level 1**; the user decides.

| Table | Address | Content | Tag |
|---|---|---|---|
| car file stems, race slots | `004c5660` (16 pointers) | `TSupra LDiabl P911 CZR1 F512TR DVIPER ANSX MRX7 MRX7 MRX7 MRX7 Traffc`, then cops `CopMust CopCamaro CopCaprice CopVic` | [X] |
| traffic stems | `004c56a0` (12 pointers) | `Wagon CRX Vandura BMW SunBird JETTA RODEO axxess probe Jeep Lemans Pickup` | [X] |
| PBS name table | `004bc664` (`car_model_names[12]`, `car_load.h`); used only by `004400f4`: CARSPECS + name by entry model + `.PBS` (`004afd88`) | `TSupra LDiabl P911 CZR1 F512TR DVIPER ANSX MRX7 MRX7 MRX7 P911T Traffc`; index 12+ is NULL | [S] |
| track stems | `004af2fc..004af3dc` | `alpine springs avalley bsienna vridge tpolis lvegas citym coastm` + `…m` and `…r` variants + `cityr coastr` | [X] |
| engine banks | `004b00b0..004b01f7` | `Supra_sw Diablosw 911_sw ZR1_sw tr512_sw Vipersw NSX_sw RX7_sw traffc` (player) and `OSupmb ODiamb O911mb OZR1mb O512mb OVipmb ONSXmb ORX7mb traffcO` (opponent), `COLLSWWT3 COLSWMT3 COLLSBWT…`, `NFS_FMMB` | [X] |
| FAM name formats | `004b0a60..004b0a90` | `%s/%s_T%.2d.fam`, `_M%.2d`, `_R%.2d`, `%s/%s_%.3d.fam` | [X] |
| TRI name format | `004afc4c` | `%s%s.tri` (dir `simdata/misc/` + stem) | [X] |

How `00457d40` (car loader) turns a slot into files [X]: with `n = 004fba94[car slot]`:
`n < 100` traffic stem; `100..199` race slot `n-100` with an **`m` prefix** (`004b1174`) = restricted CFM
(`MTSUPRA.CFM`); `200..399` race slot `n-200`, full CFM; `>= 400` cop. The file is
`<carFams dir><prefix><stem>.cfm` (`004b1178`); the PDN is read by `0040fa3c`/`004407b8` (see STATE.md) as
`<stem>.PDN`. The CD's other CFMs (`F512M`, `LDIABLO`, `WARIOR`, `WARRIOR`, `TESTA`, `MF512M`, `MP911T`) have no stem in
`004c5660`/`004c56a0` — they look unused **[?]**. `P911T` appears only in the PBS table `004bc664` (index 10, and there is no `P911T.PBS` on the CD, so model 10 would fail at the specs load; whether the frontend offers it is unchecked) and as a CFM. A car slot is two files: the PBS named by `004bc664` by model, the CFM named by `004c5660`; the tables differ only at index 10 (`004fba94` is set in `00457870`: human model + 200, 'm' mesh = 100 less in multi-car races, opponents from `004c5628`).

Consequences for the epic:

- Adding a 13th player car, an 11th track or a new bank needs a new table entry = patching the EXE = Level 2.
- A slot's files can be replaced under the same names. Replacing a *stem* is impossible without patching.
- `TR8_R01.FAM` exists in `NTRACKFM` with no `TR8.TRI`: an unused leftover [D].

## 3. Where the game looks for files (PATHS.DAT)

`0042a188` [X] tries to read `gamedata\config\paths.dat`; if it is missing it fills the directory table from
built-in strings (`004aec68..004aee08`); English/German swaps `speech art show slides trackFm dash` for their `g*`
versions (`0052b31c`). If present, the file is copied **bytewise, 0x5f0 bytes = 19 slots of 80 chars**, over
`00524198..` (the loop count `0x5f0` in `0042a188`).

| Slot | Addr | Default (English) | Slot | Addr | Default (English) |
|---|---|---|---|---|---|
| 0 | 00524198 | `gamedata/config/` | 10 | 005244b8 | `simdata/etrackFm/` (German `gtrackFm`) |
| 1 | 005241e8 | `gamedata/savegame/` | 11 | 00524508 | `simdata/ntrackFm/` |
| 2 | 00524238 | `frontend/speech/` | 12 | 00524558 | `simdata/slides/` |
| 3 | 00524288 | `simdata/soundbnk/` | 13 | 005245a8 | `simdata/carFams/` |
| 4 | 005242d8 | `frontend/music/` | 14 | 005245f8 | `simdata/soundBnk/` |
| 5 | 00524328 | `frontend/art/` | 15 | 00524648 | `simdata/carspecs/` |
| 6 | 00524378 | `gamedata/modem/` | 16 | 00524698 | `simdata/dash/` |
| 7 | 005243c8 | `frontend/movie/` | 17 | 005246e8 | `simdata/misc/` |
| 8 | 00524418 | `gamedata/replay/` | 18 | 00524738 | `frontend/show/` |
| 9 | 00524468 | `simdata/misc/` | | | |

Slot address = `00524198 + 0x50 * slot`. The addresses are the destinations `0042a188` writes in its default branch [X].

**Mod relevance.** The original EXE can already be pointed at a different directory per slot. A directory
cannot fall back to `original/` file by file, so a mod used with the original EXE has to be a complete copy of
each redirected directory. The port's `--mods DIR` overlay (phase 6) is a different mechanism and does not need this.

Auroux's PATHS.DAT has 18 slots (1440 bytes) **[A]**; this EXE has 19 (1520): the extra slot is `ntrackFm`
(`00524508`), added by the SE. Version difference. Slot order otherwise agrees.

## 4. Per-track files

A track *n* needs (stems in §2):

| File | Format | Loader | Notes |
|---|---|---|---|
| `SIMDATA/MISC/<stem>.TRI` | TRI | `0043e4e0` [S] | nodes, scenery, terrain; 17 on the CD (`AL22.TRI` is a variant of AL2 with a 12-byte `CRCF` trailer [S][D]) |
| `SIMDATA/MISC/<stem>INFO` | text | `%s%sInfo` (`004afb38`) [X] | CRLF text: horizon/sky parameters and colour table, e.g. `#ring y offset`, `#RGB TIME OF DAY VALUES`. **Not in the epic's plan.** It is plain text and easy to expose through TSV/edit [D] |
| `SIMDATA/NTRACKFM/<T>_{Tnn,Mnn,Rnn}.FAM`, `E/GTRACKFM/<T>_001.FAM` | FAM (wwww) | `004511a4`, `004511fc` [S] | terrain textures, scenery bitmaps, horizon, 3D objects |
| `FRONTEND/ART/TRACKSEL/<…>SEL.QFS`, `<…>BST.QFS`, `CONTROL/<stem>.QFS` | QFS/SHPI | front end | name, map and labels are **bitmaps** (§9) |

Per-track data counts [D]: 17 TRI = 10 stage tracks (`circuit_segments` 0: AL1..3, CL1..3, CY1..3) and 7 circuits
(TR1..TR7: 128 or 256 segments). Stage tracks have 440..578 segments (1760..2312 nodes).

## 5. Per-car files

| File | Format | Loader | Notes |
|---|---|---|---|
| `CARFAMS/[M]<stem>.CFM` | wwww{ORIP, SHPI, ORIP, SHPI} | `00451278` + `00457d40` [S][X] | high- and low-detail mesh + bitmaps. 39 on the CD |
| `CARFAMS/<stem>.PDN` | QFS → 0x1CC bytes | `00457d40` family [S] | extents, mass, 100-word AI speed table (+0x1c), factors (+0x1ac). 22 files: 9 player + 12 traffic + `COPMUST` [D] |
| `CARSPECS/<stem>.PBS` | QFS → 0x778 bytes | `004400f4` [S] | performance; checksum at +0x774 [D]; 9 files |
| `CARSPECS/BY_R&T` | 4 bytes `AE 1F 57 00` | `r&t%d` (`004af9b8`)? | purpose **[?]** |
| `DASH/<stem>D[LH].{FMM,FSH}` | wwww{BAMF×4}, SHPI | `%s%sDH.fmm` (`004ade3c`) [X] | windshield shape + dashboard bitmaps; German in `GDASH` |
| `SLIDES/<stem>S<n>.QFS`, `GSLIDES` | QFS/SHPI | front end | 7 slides per car |
| `FRONTEND/SHOW/<…>.QFS`, `GSHOW` | QFS/SHPI | front end | showroom: name + 5 pages (GEN/HIS/MEC/PER/video) |
| `SOUNDBNK/<…>.BNK` | BNK | **[?]** (loader not located) | `*_sw` player engine bank (stereo 16-bit), `O*mb` opponent, `COLL*`, `FRONT_MW`, `NFS_FMMB`, `TRAFFC*` |
| `CONTROL/<cc>1.QFS`, `<cc>2.QFS` | QFS | front end | car screens in the setup menus (`SUP1`, `DIA2`, …) |

Required names in the CFM [X, `00457d40`]. After loading, the car loader looks up fixed names in the mesh and
**loops forever on failure** (a hang, not an error):

- Bitmap directory entries `004b1160` and `004b1180` (palette entries, type `0x24` is shifted right by 2 to 6-bit)
  and `004b1160` again in the low-detail directory.
- Mesh elements `inside`, `surface` (index must be < 8, else infinite loop), `outside`, `convex` (index ≥ 8 is
  tolerated and stored as 999 = absent) — names at `004b1188..004b11a0`.
- For race cars (slot ≥ 200): `frntires`, `rertires`, `tire1..tire4` (`004c5778`); a miss on the player's own car
  loops forever.
- For the player's car: `left_turn`, `right_turn` (`004b11c0`, `004b11cc`); a miss loops forever.

These are the inputs `validate` must check before the game would hang. Which ORIP section each lookup searches
(`FUN_00490b21`, `FUN_004911f2`, `FUN_004912d0`) is **[?]**.

## 6. Format by format: what we have, what we lack, and the Auroux contrast

Rule from the task: Auroux describes the 1995 DOS release; a difference counts as a **version difference** until
the EXE says otherwise. "Match" = the field/offset agrees and was checked on the data. Every row says which.

### 6.1 TRI (`src/formats/tri.h`)

| Item | Auroux | This EXE (`tri.h`, `0043e4e0`) | Verdict |
|---|---|---|---|
| Header size before nodes | `0x12F8`, two index tables of `0x960` | nodes at `0x98c` after one table at `0x2c` | **Version** (the second table is gone). All later offsets are Auroux − `0x96c` [D]: brightness `0x15b0c` (A: `0x16478`), `0x16214`=64 (A: `0x16B80`=`0x40`), `0x16218`=1000 (A: `0x16B84`=`0x3E8`), chunk at `0x1621c` (A: `0x16B88`) |
| First table | stride `0x548`, "lookup only" | stride **288** (= TRKD chunk size) in all 17 files [D] | Version |
| Second table, scenery start `0x1B000`, 5-row `0x554` records | yes | absent; terrain follows the object chunk | Version |
| Object chunk tag | `OBJS` | `SJBO` (`0x1621c`) [D] | Version (spelled differently) |
| Node record size/position | 36 bytes, 2400 nodes | 36 bytes, 2400 nodes [D] | Match |
| Node bytes 0..7 | `a0..a3`, `b0..b3` unknown | `left_verge`, `right_verge`, `left_barrier`, `right_barrier`, flags[3], `property` (names from OpenNFS1 notes, medium) | Match on layout; Auroux names nothing. Our names are **reference-derived, not verified** by the EXE except where AI code uses them (STATE.md: `+2/+3` barriers, `+4` nibbles, `+0x1a`) |
| Position `+8` | `x, z, y` (z vertical) | `x, y, z`; ours `y` = height, `z` = forward | **Match on offsets, naming differs**: Auroux's `z`/`y` are our `y`/`z`. Kept as the port's convention |
| `slope` `+0x14` | 14-bit, complement of `0x4000`, ≈ `Δz/152` | values 0..0x3FFF in all files; `Δheight / signed(slope)` median **152.1** [D] | **Match** |
| `slant-A` `+0x16`, `slant-B` `+0x1e` | `B ≈ −12.3 A` | median ratio **−12.42** over 1838 nodes [D] | **Match** |
| `orientation` `+0x18` | 14-bit, `0x1000` = east | `0x4000` = full turn [S] | Match |
| `y-orient` `+0x1c`, `x-orient` `+0x20` | norm ≈ 32000, `<0x8000` | `hypot` of the pair: 27009..32767 over all nodes of 17 files [D] | **Match** (norm up to `0x7FFF`) |
| Object record | 16 B: node(4), bitmap(1), flip(1), flags(4), x,z,y int16 at `0xA` | `TriScenery`: node, **descriptor index**, orientation, flags[4], x,y,z at `0xA` | Same bytes; the EXE adds a level: byte 4 indexes the 64-entry descriptor table. Auroux's unexplained `0x400`-byte block of 16-byte records **is** that table (`TriDescriptor`) |
| Objects capacity | 1000 | 1000 (`0043e4e0` scans 16000 bytes) | Match |
| TRKD record | `0x554` B, `TRKD`, len `0x548`, record number at `+8`, textures at `+0xE` ×10, ref point `+0x18`, 5 rows × 11 absolute i32 points + unknown triples | **288 B**, `TRKD`, len 276, `+8` = 0 in every file, textures `+0xE` ×10, 4 rows × (middle + 5 right + 5 left) × **int16 deltas** (66 B/row) at `+0x18` | **Version**: coordinate width and row count changed; ordering of the 11 points (A0 middle, A1..A5 right, A6..A10 left) **matches** `TriRow` |
| Textures T1..T10 | T1..T5 right, T6..T10 left | `texture[10]` at `+0xE`, `+0xD` has fence bits | Layout match; use of T1..T10 per side **[?]** (to confirm in the viewer's reader) |
| Texture index → bitmap | `3xx + (A,B,C)`, name `xxls`, closed vs open FAM | (see 6.4) | to verify in 6.4 |

Unchanged risk: the file is **fixed-capacity**. The loader reads `0x1621c` header bytes in one go, so the header
must be exactly that size. `segment_count` ≤ 599 follows from 2400 nodes + 1 closing node (`tri.h`). The 600-entry
offset table at `0x2c` is **not read** by the load path of `0043e4e0` (RE, 2026-10-10); its role is unconfirmed.

### 6.2 FSH / SHPI (`src/formats/fsh.h`)

| Item | Auroux | This EXE | Verdict |
|---|---|---|---|
| Header 16 B: `SHPI`, length, count, dir id | yes | same [D] | Match |
| Directory: 4-char name + offset from file start | yes | same | Match |
| Entry: `7B`, 3-byte size = `w*h+0x10`, w, h, 4 unknown bytes, x, y, pixels | yes | `misc[0..1]` unknown, `misc[2..3]` x,y [S] | Match |
| Palette entries `!PAL` 256×3 bytes | "RGB, 1 byte each" | type `0x22` = 6-bit VGA DAC (144 files), `0x24` = 8-bit (41) [D]; `00457d40` shifts `0x24` right by 2 | **Auroux does not distinguish the two types**; the port does. Version/precision difference |
| Directory ids | `GIMX` (dash), `LN32` (QFS), `WRAP` (CFM) | `GIMX`, `LN32`, plus `SPIT` (2 files) [D] | Match + new id |
| Entry types | `7B` only | `7B`, `22`, `24` only on the CD [D]; which types the draw code accepts is **[?]** | Open |

### 6.3 QFS and INV

| Item | Auroux | This EXE | Verdict |
|---|---|---|---|
| Compression | "some kind of Huffman" | 4 codecs behind `004a2520`: RefPack `10`, Huffman `30/32/34`, BTree `46`, RLE `4A` (unused on CD) [S][D] | Auroux knew one of four. The decoder is ported; **no encoder exists** |
| `LN32` = 15-bit colour | yes | directory id `LN32` on 212 payloads, pixels still stored as type `0x7B` bytes in the files inspected [D] | See below |
| `.INV`: 32768 bytes mapping 15-bit colour → palette index | yes | all 102 files are 32768 bytes [D] | **Match** (size). Semantics not re-verified by us |

Note: Auroux says `LN32` bitmaps are 15-bit words reduced through `.INV`; every bitmap entry found in the QFS files
inspected has type `0x7B` with `w*h` pixel bytes plus a palette entry (checked by rendering `RUSTSEL` and `DIABLO`
with the palette in `!pal`). So on this CD the images are already 8-bit indexed; whether the `.INV` still matters
is **[?]**. For the exporter: PNG export uses the resource's palette; the `.INV` is only relevant if some payload is
really 15-bit.

### 6.4 CFM and FAM (`chunk.h`, `mesh.h`)

Counted over every `ORIP` found by walking all 39 CFM and 78 FAM: **330 ORIP chunks** [D].

| Item | Auroux | This EXE | Verdict |
|---|---|---|---|
| `wwww` header: tag, count, offsets (CFM has 4 children) | yes | yes [S] | Match |
| ORIP header `+8` = `0x2BC` | yes | `0x2BC` in 330/330 [D] | Match |
| ORIP `+0x28` (polygon block) = `0x70`; `+0x68` = `0x70`; `+0x64` = 0 | yes | 330/330 [D] | **Match** — resolves our `unknown_64`/`unknown_68_offset` as "always 0" and "= polygon offset = 0x70" |
| ORIP `+0x14` = `+0x10` − `0x10` | yes | holds in **12 of 330** [D] (true for DVIPER) | **Does not generalise**; leave as unknown |
| Block roles: 1 polygons, 2 uv, 3 texture refs, 4 sections A, 5 sections B, 6/7 12-byte tables, 8 vertices, 9 index map | yes | same, as implemented | Match |
| Polygon `+0`: `83` tri, `84` quad, `8C` quad without uv | yes | flags set `{0x02, 0x04, 0x83, 0x84, 0x8B, 0x8C}` [D] | Match + `0x8B`, `0x02`, `0x04` not in Auroux; `8C`/`8B` = bit 3 = "no uv" matches `mesh.h` |
| Texture ref (20 B): id string 1 (8), id string 2 (4), 8 unknown | "string 1" 8 bytes | `MeshTexture.unknown_0[2]` = **Auroux's id string 1** (non-empty ASCII in 212 of 1395 records [D]); `name[4]` = id string 2 | **Our `unknown_0` is a name**; `mesh.h` should say so. Trailing 8 bytes: Auroux `00 00 00 81 F5 00 00 00`, here `00 00 00 00 EF 00 00 00` on DVIPER (42 distinct patterns) — Version |
| Section A (20 B): name 8, **vertex count**, **polygon offset**, ? | yes | DVIPER `left_tur` = `08, 0x240, 0x6d` | Match: `unknown[0]` = vertex count, `unknown[1]` = offset in the index map, `unknown[2]` = first vertex (per `mesh.h`) |
| Section B (28 B): name 12, ?, **polygon offset**, **vertex count**, 0 | yes | `inside` = `01, 0x250, 0x10, 0`; `surface` = `0, 0x260, 0x3d, 0`; `outside` = `-1, 0x29d, 4, 0` | **Match** — fixes `MeshSectionB.unknown[4]` = {?, index-map offset, count, 0} |
| Sections 6/7 (12 B): 8-byte name + 4 | yes | same widths | Match |
| Vertex 12 B (x, "z" vertical, y longitudinal) | yes | `x,y,z` same order | Naming only (see 6.1) |
| FAM tree (4 children: terrain, scenery, horizon, objects) | yes | yes [S] | Match |
| FAM open vs closed layout (single subchunk vs one per bitmap) | yes | `TRn` has 1 SHPI; `AL/CL/CY` 12..42 [S] | Match |
| Texture number → bitmap name: `n = 3xx + {A,B,C}`; names `xxls` (closed) / `l00s` (open) | yes | **not yet checked by us** | **[?]**: to confirm in `src/viewer`/`track_viewer.c` against `0043e4e0`/TRKD |

### 6.5 BNK (no loader in `src/formats`)

Walked all 38 files [D]:

| Item | Auroux | This EXE's data | Verdict |
|---|---|---|---|
| 128 × u32 offset table, 0 = absent | yes | yes | Match |
| Sample header 72 B, `EACS` at `+0x28`, `+4` = header offset + `0x28` | yes | `EACS` and `+4` agree in every file | **Match** (38/38) |
| Rate `0x3E80` (16 kHz) | yes | 16000 in 299 of 334 samples; **16247 in 27 and 22050 in 8**, all in the `COLL*` banks [D] | Match for car banks; **collision banks differ**: an exporter must write the stored rate |
| `+0x30` 1 = 8-bit, 2 = 16-bit; `+0x31` 1 = mono, 2 = stereo | yes | yes | Match |
| Car bank = 4 samples at slots 1, 2, 3, 32 (offsets `04, 08, 0C, 80`) | engine ×2, horn, gear change | `*_sw`: slots 1, 2 stereo 16-bit; 3 mono; 32 length 0 | **Match** |
| Header flags `+0`: 8-bit car banks `1, 2, 4000, 10` | | `*_sw`: `0x40, 0x2, 0x4000, 0x10`; `TRAFFC`: `0x2000, 2, 0x4000, 0x10`; `O*mb`: `0x10, 4, 8` (slots 65, 67, 69); `FRONT_MW`: `0xF8`; `COLL*`: `0x30`; `NFS_FMMB`: `0x100, 0x200…` | Flags differ by bank family: **meaning [?]** |
| "S/M = 8/16 and stereo/mono" file-name rule | file suffix | 8-bit mono banks are the `O*mb`/`COLL_SB`; `*_sw` are 16-bit | Auroux's `SB` car banks are not on this CD |
| Length at `+0x3C` in samples | yes | **0 for the 22..30 collision samples** and for the gear-change sample; their extent is the gap to the next offset | **Difference**: length is not always stored. An exporter cannot trust `+0x3C` |
| Data entirely inside the file | | `TESTBANK.BNK` slot 3 claims 64930 samples, **ends past the file** [D] | The EXE has no string `testbank`: an unused file. A second case: `OSUPMB3D.BNK` slot 70 claims 9653 bytes at 8848 in a 17744-byte file (757 past the end) [D]; no whole-string `*3D` name in the EXE either. **AC4's "all of `original/` passes without warnings" needs the rule "files the EXE references"**, or this file is flagged |

BNK names the EXE references (`004b00b0..004b01f7`): `Supra_sw Diablosw 911_sw ZR1_sw tr512_sw Vipersw NSX_sw RX7_sw
traffc OSupmb ODiamb O911mb OZR1mb O512mb OVipmb ONSXmb ORX7mb traffcO COLLSWWT3 COLSWMT3 COLLSBWT NFS_FMMB`
(partly truncated by the string finder). `FRONT_MW`, `COLL_SB`, `COLL_SW`, `COLLSW3D`, `COLLSBMT`, `COLLSWWT`,
`COLLSWMT`, the `*3D` banks and `TESTBANK` are not found as whole strings: either assembled at run time or unused **[?]**.

### 6.6 PBS and PDN

| Item | Auroux | This EXE | Verdict |
|---|---|---|---|
| PBS "somehow compressed" | yes | QFS Huffman `30 FB` → **1912 bytes** (`0x778`) [D]; checksum at `+0x774` = sum of the first `0x758` bytes **holds in all 9 files** [D][S] | Match (compression identified) |
| `BY_R&T` is modified by the game; must be made read-only | yes | 4-byte file. A copy check in `004400f4` (string `By_R&T`, `004afd90`): if it can `fopen` CARSPECS/`By_R&T` with `"wb"` it writes the FILE pointer and zeroes the car's power. On the read-only CD the open fails and nothing happens. `r&t%d` (`004af9b8`) is unrelated (shape `r&t<n>` in `control/ani.qfs`) [X] | **Match**: install and validate must warn if CARSPECS is writable (every car would get zero power). The port leaves the branch out |
| PBS copied over another changes performance but not sound | yes | consistent with the per-slot BNK table (§2) | Match |
| PDN | not described | QFS → **460 bytes** (`0x1CC`) in all 22 files [D][S] | New |
| Bytes `0x758..0x773` of PBS | | identical across the 9 files (`71 71 70 70 70 70 70 6F 6F 6F 6F 6F …`) [D] | Opaque, preserve |

### 6.7 PATHS.DAT, `*.SAV`, CONFIG.DAT

| File | Auroux | This EXE | Verdict |
|---|---|---|---|
| PATHS.DAT | 18 × 80 = 1440 | 19 × 80 = 1520 (`0042a188`); extra slot `ntrackFm` | **Version** (see §3) |
| CONFIG.DAT | 25552 bytes, "everything" | **24402 bytes = `0x5f52`**, exact size required (`config_dat.h`, `0042a010`) [S]; CD copy is 24402 [D] | **Version**. Layout in `config_dat.h`: name at 0, race/track/car words at `0x6d..0xb3`, hall-of-fame records `0xb5..0x5f33`, key words `0x5f36`, unlock level `0x5f50` |
| `*.SAV` | 840 bytes; strings at 0, 0x28, 0xEC, 0x114, 0x1D8, 0x20A, 0x270; win flags at 0x338.. | EXE builds `%s%s.SAV` (`004aeeb0`), `*.SAV` (`004af054`); **no sample on the CD** (`SAVEGAME/` is empty), the port has no SAV reader | **Cannot be compared**. Size and layout are **[?]**; only needed if a mod must pre-seed a tournament |
| Replays | not described | `REPLAY*.RPL`, 100374 bytes each [D]; `%s%s.rpl` | Opaque for this epic |

### 6.8 Other formats Auroux describes

`FMM`/`MMB`/`B00..B23` (BAMF windshield masks), `FFN`, `EAS`, `ASF`, `TGV`: out of the epic's scope except where a
mod would replace a car (the `D[LH].FMM` pair). Their loaders exist only partly (`ffn.c`). The BAMF format is
**not** in `src/formats`. A car mod that keeps the original dashboard does not need it.

## 7. Limits of the original EXE

Only entries with a source are listed. "Limit" = the original's behaviour changes (hang, crash or truncation) outside it.

| Element | Limit | Source | Tag |
|---|---|---|---|
| TRI header block | exactly `0x1621c` bytes reserved and read (a shorter file fails; extra bytes are read by the next chunks); nodes are 2400 × 36 from `0x98c`, so `4 × segments` ≤ 2400 and a closing node needs `segments` ≤ 599 | `0043e4e0` (asm, via tnfs-decomp) | [X] |
| TRI circuit size | `u16` at `+4` = 0 → stage (`segments` 0x8000, mask −1); else `segments = u16(+4)`, `nodes = 4 × segments`, mask = `nodes − 1`: **nodes must be a power of two** on circuits (inference from the mask; the 7 CD circuits have 128/256 segments [D]) | `0043e4e0` | [X] |
| TRI `SJBO` chunk | reserved and read from its own size field (`+4`); no cap other than memory | `0043e4e0` | [X] |
| TRI terrain | reserved and read as header `+0x24` bytes; no cap other than memory (`288 × segments` in 17/17 [D]) | `0043e4e0` | [X] |
| TRI fixed extra reserve | `0x8700` bytes, name at `004afc58` (`RoadSlices`), global `005041d8`; independent of the file | `0043e4e0` | [X] |
| TRI scenery records | counted to the `-1` terminator, **at most 1000 (16000 bytes, `cmp 0x3e80`) → `005041d0`; extra records, or a missing terminator, are ignored silently** (all 17 CD files are terminated: 138..998 records [D]) | `0043e4e0` | [X] |
| TRI descriptor index | the record byte `+4` (0..255) × 16 indexes the descriptor table **unchecked**; an index ≥ `descriptor_count` reads scenery bytes as a descriptor. **Limit for validate: `descriptor < descriptor_count`** (CD max 54 < 64 [D]) | `0043eca4`, `0044a4b0`, `0044a87c` | [X] |
| TRI `descriptor_count` | only used to skip the descriptors and find `00509788 = SJBO + count × 16`; no 64-entry array depends on it | `0043e4e0`, `0040d6a0` | [X] |
| TRI `scenery_slots` (`+0x16218`) | `0040d6a0` allocates `slots × 2` bytes of per-record state (knocked-down flag and minute), written by record index **without a check** (`0040fc64`, `0043eca4`). **`slots` must be ≥ the record count or the original writes past the buffer** (CD: 1000 everywhere [D]). The buffer is allocated only when `004b9098` is 0 | `0040d6a0` | [X] |
| TRI model descriptors (type 1) | `byte +2` indexes the array at `005150f0` (objects of `0x4e` bytes); its size, and the cap of the position stack `004fd134` (counters `005041b4`/`005041b8`), are **[?]** (question 3) | `0043eca4` | [X]+[?] |
| TRKD record | 288 bytes (`288 × segments` = terrain size in 17/17 [D]) | `tri.h` | [S][D] |
| TRI chunk size | `17036` in every file (`12 + 64×16 + 1000×16`) [D] | | [D] |
| Car CFM mesh element indices | `inside`, `surface` < 8, else infinite loop | `00457d40` | [X] |
| Car CFM required names | see §5 | `00457d40` | [X] |
| PBS size | 1912 bytes, checksum `+0x774` over `0x758` | `car_load.c`, [D] | [S][D] |
| PDN size | 460 bytes (`0x1CC`) | STATE.md `00457d40`, [D] | [S][D] |
| Slots | 12 race cars (16 with cops), 12 traffic, track stems as §2 | `004c5660`, `004c56a0`, `004af2fc` | [X] |
| PATHS.DAT | 19 × 80 chars, exactly 0x5f0 bytes | `0042a188` | [X] |
| CONFIG.DAT | exactly 24402 bytes | `0042a010` | [S] |
| BNK table | 128 offsets, headers 72 B | [D] | [D] |
| FAM file size | fixed buffer `0x162400` bytes (1 451 008), reserved by `00451040` (name `004b0a54`) at `00513e3c`; `004510e8` reads at most that many, **a larger FAM is truncated silently**. The largest on the CD is `TR4_M01.FAM`, 1 444 480 bytes (`0x160a80`), i.e. only 6 528 bytes of headroom [D] | `00451040`, `004510e8` | [X][D] |
| `wwww` tables | child count from `+4`, no cap; offsets are turned into pointers **without bounds checks** (offsets must lie inside the file). A zero offset is *not* left as 0: it receives the previous entry's pointer (`004511a4`), so a writer must keep zeros as they are | `004511a4` | [X] |
| FAM scenery objects (child 3) | object `i` is written at `005150f0 + i × 0x138` unchecked; the next global is the PDN table `00517580`, `0x2490 = 30 × 0x138` bytes later. **At most 30 objects** (inferred from the memory layout, not from a compare in the code); object 31 overwrites the PDN table. CD maximum is 8 [D] | `004511fc` → `004585e4` → `004576f0` | [X][D] |
| FAM object parts | child 0 = ORIP (`00490ea8`), child 1 = SHPI; a palette of type `0x24` has its `0x300` bytes shifted right by 2 | `004576f0` | [X] |
| ORIP | `00490ea8` only relocates (offsets at `+0x18,+0x20,+0x28,+0x3c,+0x44,+0x4c,+0x50,+0x58,+0x60,+0x68` become pointers; words `+4` and `+8` of each 12-byte polygon become `[+0x50] + value × 4`; if `[+0x64] != 0` it allocates `+0x6c` via `00474bec`). **No caps there; every index must fall inside its table** | `00490ea8` | [X] |
| ORIP instance | one allocation sized from the ORIP's own counts: `0x8c + V×0xc (+V×0xc per flag bit 0, bit 1) + V×8 (+V×8 for bit 2) + P×4 + T×0x14 + X×0x24` (V = `+0x10` vertices, P = `+0x24` polygons, T = `+0x38` texture entries, X = `+0x64`). **No vertex, polygon or uv cap beyond the memory of the FAM/CFM buffer** | `0048d798` (callers `004576f0`, `00457d40`; callees `004917f7`, `00491349`, `00490cc0`, `0048da62`, `0048dad1`) | [X] |
| ORIP part list quirk | with part `-1`, `004917f7` tests only polygon 0's byte `+1` bit `0x4` for every polygon (the entry pointer never advances): if polygon 0 has that bit set the list is empty. validate warns | `004917f7` | [X] |
| ORIP name searches | all compare 8 characters (`004a2da2`), return `-1` when absent, no cap: textures (`+0x38` count, `+0x3c` list, 0x14 B each) `00490b21`; table `+0x40/+0x44`, 0x14 B each `004912d0`; parts `+0x48/+0x4c`, 0x1c B each `004911f2`. A part's `+0x10` (first index into the `+0x50` list) and `+0x14` (count) are **not checked** against the polygon count. A `-1` makes the car loader spin forever | `00457d40`, `004917f7` | [X] |
| Texture names | each ORIP texture entry's 4-byte name at `+8` is looked up linearly in the SHPI directory (count at SHPI `+8`, 8-byte entries from `+0x10`): no cap; a missing name leaves a null texture pointer (render behaviour not traced). **compatible mode: a missing name is an error** | `00490b9a` → `004732e4` | [X] |
| Shape-bank registry | 16-byte entries in `0050b034`, counter `004bcbc4`; the 101st registration overwrites past the table and hangs: **at most 100 per race**. Per race: 1 (`00456a7c`, if its bank exists) + 1 FAM child 2 + 1 per bank of child 0 + 1 per bank of child 1 (`0044c36c`) + 1 (`00446a0c`) + 1 (`00450838`) ⇒ **child-0 banks + child-1 banks ≤ 96** | `00446f58`, `00447098`, `00455a5c` | [X] |
| Track FAM child 0 (background banks) | **at most 48 banks** (bank pointers at `esp+0x100+4i` under the sprintf buffer at `esp+0x1c0`; 3 × 0x20 bytes per bank in the table `0050eec4`, cleared for exactly 48 banks; bank 48 lands on `005100c4`); the registration loop runs to the full count and registers garbage for a 49th | `0044c36c` (from `0044ca3c`) | [X] |
| Track FAM child 1 (foreground banks) | **at most 63 banks**; with 64 the 0 terminator lands on child 0's first pointer, which is then registered as NULL | `0044c36c` | [X] |
| Track FAM children | fewer than 3 children calls the fatal handler `004c6f88` | `0044ca3c` | [X] |
| Resource load (SHPI/SHPF through QFS) | file name copied unchecked into a 128-byte stack buffer: **name incl. extension < 128**; `.fsh` appended when no dot. Packed iff byte 1 is `0xfb` or `0x32` and `byte0 & 0xfe` ∈ {10,30,32,34,46,4a}; unpacked size = bytes 2..4 big endian; byte 1 `0xfb/0x32` with another code prints "INVALID PACK CODE" and the data is taken as stored. **No cap on the unpacked size**; the handle is resized to `unpacked + 0x400` (failure: "INSUFFICIENT MEMORY TO UNPACK FILE", NULL). After unpacking the data must start with `SHPI` or `SHPF`, else NULL | `00476268` → `004760f0` → `0048d458`, `004a261c`, `00474d90` | [X] |
| In-place unpack | the packed bytes are moved to offset `unpacked + 0x400 − packed` and unpacked from the front. **Rule 1**: `packed ≤ unpacked + 0x400` (else a write before the buffer). **Rule 2**: at every step, output written ≤ input read + (`unpacked + 0x400 − packed`). The QFS encoder (phase 2) must check rule 2 and, when it fails, store the file unpacked | `004a2520` | [X] |
| `<track>INFO` text file | `%s%sInfo` = paths slot 17 (misc) + stem, opened `"r"`, read only if it opens and `004fa0ec != 1`. Lines via `00429020`: `fgets` 0x7f (**≤ 126 characters**), lines starting `#`, `\n`, `\r` skipped, at EOF the last line repeats. 33 data lines: 8 decimals (16.16, `00431f38`: vert/z offset, height, radius near/far); 2 lines of 3 integers packed `a<<10 \| b<<5 \| c` unmasked (**each 0..31**); ring type, y offset, height (×200/480 when `00512018` is 0); 3 groups of 5 integers (time-of-day RGB, 5th `<<8`); a last integer (nonzero forces ring type 1); integers by `atoi`. Without the file a built-in row is picked by stem (`cl1..cy3`, `tr1..tr7`); **an unknown stem with no Info file reads through a NULL row: validate requires the Info file for any stem outside that list** | `0043a370` (from `00455a5c`), `00429020`, `00431f38` | [X] |
| CARSPECS writable | with `004c5ccc` = 1 (never written) and the German flag `0052b31e` = 0, `004400f4` opens CARSPECS/`By_R&T` `"wb"`; success zeroes `specs+0x3c` (power). **install/validate warning: keep CARSPECS read-only** | `004400f4` (`004afd90`) | [X] |
| BNK load | race banks `00443924`; `"%sfront_mw.bnk"` `004648dc`; each file by `004438c8` (`00483ec0`), **a missing file is fatal** (`004c6f88`); 6 loaded-bank slots `0050a564`, counter `004bc6e8`, reset by `00443be0` | | [X] |
| BNK registration | `0048ed30`: each nonzero offset relocates the record's `+4` and the target's `+0x18` by the file base; the bank takes the first free of 5 entries (`004c79a0`). **Fatal if a sound number is already taken by a loaded bank, or no entry is free.** Banks loaded together need disjoint sound numbers (validate rule). CD numbers: `*_SW`/`TRAFFC` 1,2,3,32; `COLL*` 33.. (22–30 of them); `O*MB`/`TRAFFCO` 65,67,69; `O*MB3D` 65,67,68,69,70 (`OZR1MB3D` only 65,67,69); `FRONT_MW` 71..74; `NFS_FMMB` 97,98,100,105,106 | `0048ed30` | [X] |
| BNK record `+0` roles | `0x40` player, `0x10` opponents, `0x30` collisions, `0xf8` front, `0x100` FM, `0x2000` TRAFFC, `4` TESTBANK. Every record's `+4` is `0x228`. Which header fields playback reads (`0048eea0` on) is still open | | [X] |
| BNK files opened by name | the 12 `*_SW` (`00443670`), the opponent table `004436a0` (24 pointers, 2 × car), `COLLSWWT`/`COLLSWWT3`/`COLLSWMT`/`COLSWMT3` (two picked by race type 7, `004fb7c2`), `front_mw`. **Never opened**: `O*MB3D`, `COLLSW3D`, `COLL_SB`, `COLL_SW`, `TESTBANK` (no string in the EXE) | `00443670`, `004436a0` | [X] |

A limit without an address does not enter the table (AC5). Still open: see §9.

## 8. Decisions (user, relayed by tnfs-decomp)

**Resolved.** The user took the recommended option on all three points (relayed by tnfs-decomp; the gate approval
itself is pending my user's direct confirmation in this session):

1. Level 1 only **replaces** existing slots; adding slots is Level 2.
2. Engine sound = one WAV per sample + TSV sidecar. Open condition: confirm in the data that 8-bit samples are
   signed before the exporter ships.
3. Menu names are an image edit (Level 1). Residual risk (names drawn as font text elsewhere) stays as RE question 7.

The original analysis follows.

1. **Add slots or only replace?** Evidence (§2): all slot names are tables in the EXE; `004c5660` has 12 race-car
   entries, track stems are a fixed list. No code enumerates a directory. **Recommendation: replace only for
   Level 1**; adding is Level 2 (EXE patching). Effect on AC7: "sustituyo … en su hueco" holds as written.
2. **Engine-sound exchange format.** BNK is a table of 72-byte headers plus raw PCM [D]. Recommendation: **one WAV
   per sample** (PCM, 16 kHz, 8- or 16-bit, mono or stereo as the header says; 8-bit data is **signed** per Auroux
   [A] — to confirm in the data before the exporter ships), plus a **TSV sidecar** with the header fields
   (flags, loop start/length, opaque bytes). Reasons: it round-trips (the header carries what WAV cannot), it opens
   in any editor, and the loop points that matter for engine sound live in the TSV. Samples whose stored length is 0
   (collision/gear banks) are exported by extent and flagged. This is a proposal; the user decides.
3. **Risk ALTO — are menu names in the EXE?** Searched `strings.tsv` and the raw EXE for the display names
   (`Rusty`, `Lost Vegas`, `Autumn`, `Sienna`, `Lamborghini`, `Viper`): nothing but the file stems (`viper`, `lvegas`,
   `bsienna`, `DVIPER`, …) [X]. Rendering `TRACKSEL/RUSTSEL.QFS` and `SHOW/DIABLO.QFS` shows the title text
   ("Rusty Springs Raceway", "Lamborghini DIABLO VT") **drawn inside the bitmap** [D]. So for these screens a rename
   is an image edit — data, Level 1. Residual risk: other screens (results, stats, car-select tiles) may draw names
   from font text; one RE question below closes it. If a name does come from the EXE, AC7's rename moves to Level 2
   as the spec says, and we stop before phase 7.

### 8.1 Names drawn from EXE text (answer to question 9, tnfs-decomp)

- **Car short names are EXE text.** Eight names (RX-7, NSX, SUPRA, 911, ZR-1, VIPER, 512TR, DIABLO) sit in two identical
  copies at `004b11e8..` and `004b1924..`, in fixed slots (4 bytes for NSX and 911, 8 for the others). They are drawn with
  the font routine `00463cd8` on the results and records screens: table `004c5c84` used by `0045c45c`, `0045cea0`,
  `0045de08`, `0045eff4` (from `0040b648` / `0040c710`) and `0046067c`; `004676d8` copies the second table (`0046741c`).
  The index is the car number stored in the records (`00524eb4` / `00524f9e` + track × 0xa6b) or `00524e89`.
- **Track display names appear nowhere as EXE text.**
- The full names on the selection screens stay QFS bitmaps (Level 1).
- **Consequence for AC7:** renaming a car on the results and records screens is EXE text = Level 2 (the only Level 1 way
  would be a same-length binary patch inside the slot, which the spec places at Level 2). Track renames and car renames on
  the selection screens remain image edits. **Decision for the user** before phase 7: how AC7 words the car-name case.

## 9. Questions for the RE session (tnfs-decomp)

Each is answerable from Ghidra between two functions. They are limits the tool needs.

1. ~~TRI capacity~~ **Answered** by tnfs-decomp (see §7): fixed `0x1621c` header, `SJBO` and terrain sized by their own fields, `0x8700`
   extra reserve; `00474c8c` is the allocator (name, size, flags on the stack).
2. ~~`descriptor_count`~~ **Answered** (see §7): only a skip count; descriptor byte `+4` is unchecked; `scenery_slots × 2` state buffer.
3. ~~FAM/ORIP limits~~ **Answered** (see §7): FAM buffer `0x162400`, 30 objects, unchecked `wwww` offsets, no ORIP vertex/polygon/uv cap, shape-bank registry ≤ 100, child 0 ≤ 48 banks, child 1 ≤ 63.
4. ~~ORIP callees and name searches~~ **Answered** (see §7).
5. ~~Resource load / QFS output size~~ **Answered** (see §7).
6. ~~BNK loading and registration~~ **Answered** at bank level (see §7). Still open, deferred: the per-sample header fields the playback code reads and the meaning of each flag bit (`0048eea0` on); not needed by the loaders.
7. ~~`BY_R&T`~~ **Answered** (see §6.6, §7).
8. ~~`*INFO` parser~~ **Answered** (see §7).
9. ~~Names from EXE text~~ **Answered (risk partly materialised)**: see §8.1.
10. SAV: size and layout written by the EXE (`%s%s.SAV`, `004aeeb0`); needed only if tournament state must be authored.
11. FSH entry types accepted by the draw routines (only `0x7B`/`0x22`/`0x24` seen) and maximum bitmap size.
12. ~~`004bc664` versus `004c5660`~~ **Answered**: PBS table versus CFM table (see §2).

## 10. Facts with EXE addresses (for `re/STATE.md`)

- `0042a188`: reads `gamedata\config\paths.dat`; 19 slots × 80 chars (`0x5f0` bytes copied) to `00524198 + 0x50 i`;
  defaults at `004aec68..004aee08`; German swaps six slots (`0052b31c`). The SE slot absent from the DOS spec is
  `ntrackFm` (`00524508`).
- `00457d40`: file name = path + (`m` for `n` in `100..199`) + stem + `.cfm`; stems at `004c5660` (race, `n` mod 100 or
  200), `004c56a0` (traffic, `n < 100`), cops `004c5690..`; `004b1174` = `m`, `004b1178` = `.cfm`.
- `00457d40`: requires mesh elements `inside`, `surface` (index < 8), `outside`, `convex`; race cars also
  `frntires`, `rertires`, `tire1..4` (`004c5778`); the player's car also `left_turn`, `right_turn`. A missing required
  name spins in an infinite loop.
- `00457d40`: palette entries of type `0x24` are shifted right by 2 (to 6-bit) for three directories (`+0x522`,
  `+0x526`, `+0x65a`).
- `0043e4e0`: the TRI header read is exactly `0x1621c` bytes; scenery scan bound 16000 bytes; brightness table at
  `+0x15b0c`.
- `004afb38`: `%s%sInfo` = `<track>INFO` text files in `simdata/misc/`.
- `004b0a60..004b0a90`: FAM names `%s/%s_T%.2d`, `_M%.2d`, `_R%.2d`, `_%.3d`.
- `004b00b0..004b01f7`: the BNK names the EXE references (§6.5).

## 11. Verification done for this document

- TRI: all 17 files — version byte, segment table stride, `SJBO` tag and size, terrain tag and size, trailer; node
  orientation norms; slope and slant ratios (throwaway scripts in the session scratchpad, not committed).
- ORIP: all 330 chunks in 39 CFM and 78 FAM files — header constants and record walks.
- BNK: all 38 files — offset tables, `EACS` markers, rates, bit widths, data extents.
- PBS/PDN: decompressed with the port's `qfs_unpack` — sizes and the PBS checksum for all 9 PBS files.
- QFS/SHPI/INV: headers, directory ids, entry types, INV sizes; rendering of two screens to read their text.
- Not done: any change to the code, any comparison of exported bytes (that is phase 2).

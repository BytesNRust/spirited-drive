# Fuzzing the readers

Mods bring files from anywhere, so every reader of a format a mod can carry must survive hostile input: no crash, no read or write past
its buffers, no hang. Each reader has a fuzz harness in `fuzz/`. Every build replays the cases kept for it; a fuzz build runs it under
libFuzzer with AddressSanitizer and UndefinedBehaviorSanitizer.

## The readers

| Harness | Reader | What reaches it |
|---|---|---|
| `qfs` | `qfs_decompress` (refpack) | every packed file, through `resource_load` |
| `fsh` | `fsh_open`, `fsh_entry`, `fsh_palette_rgb` | the art archives (`.fsh`, unpacked `.qfs`) |
| `chunk` | `chunk_open`, `chunk_child`, `mesh_open` and its records, `chunk_bitmaps` | cars (`.cfm`) and track art (`.fam`) |
| `tri` | `tri_open` and its records, `tri_parts`, `tri_write` | tracks (`.tri`) |
| `bnk` | `bnk_open`, `bnk_sample`, `bnk_doc_read` and write | sound banks (`.bnk`) |
| `wav` | `wav_read` | the sound exchange |
| `png` | `png_read` (stb_image) | boot and logo art, the envmap, masks |
| `ico` | `ico_decode` | the window icon |
| `ffn` | `ffn_open`, glyphs and levels | fonts (`.ffn`) |
| `records` | `pbs_read`, `pdn_read` | car specs and bodies (`.pbs`, `.pdn`) |
| `psx_bigf` | `psx_bigf_member`, `psx_shpp_image8`, `psx_shpp_colours` | the PlayStation liveries and skies |
| `doc` | `doc_parse_chunks`, `doc_parse_shpi` and write | the studio's editor |
| `validate` | `validate_file` (its first byte picks the file name) | the check every mod file goes through |
| `track_light` | `track_light_parse` | `track_light/<TRK><L>.txt` |
| `track_places` | `track_light_place_parse` | `data/track/<TRK>.txt` |
| `terrain_rules` | `terrain_light_rules_parse` | a track's light rules |
| `settings_cfg` | `port_settings_parse`, `port_extras_parse`, `port_controls_parse` | `port.cfg`, the modes' files, `controls.cfg` |
| `config_dat` | `config_dat_parse` | `config.dat` |
| `psx_light` | `psx_lgs_parse`, `psx_lgt_parse`, `psx_sky_inf_parse`, `psx_sky_panorama` (first byte picks) | the PlayStation light and sky files |
| `masks` | `car_mask_parse`, `track_mask_parse`, `dash_mask_parse`, `dash_color_parse` (first byte picks the reader and a size) | the mask PNGs |
| `sound_wav` | `thunder_load`, `wet_roll_load`, `splash_load` | `data/sounds/*.wav` |
| `title_art` | `title_art_screen` | the title screens |
| `replay_trailer` | the replay's conditions, paints, snow and setup; `session_rules_get` | `.RPL` files, the network's rules record |
| `car_model` | `car_load_setup` (the player's car: tyres, signals and lights too) | cars (`.cfm`) |
| `track_art` | `track_art_load`, `track_art_load_animated` | track art (`.fam`) |
| `sound_bank` | `sound_bank_register`, every sound played through the mixer | sound banks (`.bnk`) |
| `music_stream` | the music reader, the stream queue and the mixer | music (`.asf`, `.as4`) |
| `tgv` | `tgv_decode` on each key frame and frame found in the input | movies (`.tgv`) |
| `shape_bank` | `shape_bank_valid`, then `shape_bank_find` on each name | every art bank the menus, the HUD, the cockpit, the smoke, the cars and the tracks search |

## Every build: the kept cases

`tests/fuzz/<harness>/` holds the inputs that once broke a reader (as small as the fuzzer could make them) and a few made-up seeds.
The ctest `fuzz_<harness>` runs the harness over them without libFuzzer (`fuzz/replay.c`), so a fixed bug stays fixed; in the ASan
build a read past an input fails it. Write the made-up seeds with `python3 -I fuzz/seeds.py .`.

Nothing from the CD goes in `tests/fuzz`: no seed copied from it, and no crash input grown from one. A crash found from a CD seed is
kept as a unit test that builds its input by hand (as `test_empty_glyph` in `tests/test_ffn.c` does).

## A fuzz build

It needs clang (the nix shell's has libFuzzer). Configure a separate build with the sanitizers and `TNFS_FUZZ`:

```sh
cmake -S . -B build/fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_FLAGS="-fsanitize=address,undefined" -DTNFS_FUZZ=ON \
      -DTNFS_FUZZ_SECONDS=600 -DTNFS_FUZZ_SEEDS=/path/to/your/original
cmake --build build/fuzz --target fuzz
```

`fuzz` builds the programs `fuzz_<harness>` and runs each for `TNFS_FUZZ_SECONDS` (`fuzz/run.sh`). Its corpus is kept in
`build/fuzz/fuzz-corpus/<harness>`, seeded the first time from `tests/fuzz/<harness>`, the repository's `data/` and, when
`TNFS_FUZZ_SEEDS` names one, your own copy of the game's files (picked by extension, a big one cut to its first megabyte; they
never leave your machine). A crash, a
sanitizer report or an input that runs longer than 10 s stops that harness and is written to `build/fuzz/fuzz-crashes/<harness>-*`,
with the log next to it.

One harness, or several at once:

```sh
fuzz/run.sh build/fuzz 600 tri
printf '%s\n' qfs fsh chunk tri | xargs -P 4 -I{} fuzz/run.sh build/fuzz 600 {}
```

Run a crash input again with `build/fuzz/fuzz_<harness> <file>`, or make it smaller with
`build/fuzz/fuzz_<harness> -minimize_crash=1 -runs=100000 <file>`.

## After a crash

1. Fix the reader so the input is refused or read safely.
2. Keep the minimized input in `tests/fuzz/<harness>/` with a name that says what it broke. If it grew from a CD seed, write a unit test
   that builds an input with the same fault instead.
3. Run the harness again until it runs its whole time clean.

## A new reader

Add `fuzz/<name>.c` with `LLVMFuzzerTestOneInput` (see `fuzz/fuzz.h`), list it in `TNFS_FUZZ_CORE` or `TNFS_FUZZ_GAME` in
`fuzz/fuzz.cmake`, give `fuzz/run.sh` its extensions, and add it to the table above. A harness passes a copy to a reader that writes
into its input, frees what the reader gives back, and reads (`fuzz_touch`) every span the reader hands out, so a span past the input
shows.

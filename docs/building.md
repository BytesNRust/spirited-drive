# Building, running and testing

A C99 + SDL3 program for macOS, Linux, Windows and the web. The game data is not part of this repository: extract it from your own CD into
`original/` (gitignored). The tables the original keeps in `NFS_WIN.EXE` are not in the repository either: the game reads them from that exe at
start-up (`game/exe/`), and says so and stops if the file is missing or is not the PC Special Edition's.

## Environments (Nix flake)

| Shell | Command | Contents |
|-------|---------|----------|
| native build | `nix develop` | cmake, ninja, SDL3, lldb |
| web build | `nix develop .#web` | emscripten, cmake, ninja |

nixpkgs is pinned to `nixpkgs-26.05-darwin`, the last release that builds for Intel Macs.

## Build

```sh
nix develop -c sh -c 'cmake -S . -B build/native -G Ninja && cmake --build build/native'
nix develop .#web -c sh -c 'emcmake cmake -S . -B build/web -G Ninja && cmake --build build/web'
```

Serve `build/web/` over HTTP (`python3 -m http.server -d build/web`) and open `tnfs.html`.

`-DTNFS_RELEASE=ON` builds for distribution: no fallbacks to the source tree's data.

## Run

```sh
./build/native/tnfs [--data original] [--german]      # boot: EA logo, title, Game Mode menu
./build/native/tnfs --view original/FRONTEND/ART/TITLE.QFS   # browse FSH/QFS bitmaps
```

Track viewer (development): `./build/native/tnfs --track TR1` flies along a track (Up/Down move, Left/Right turn; names as in `SIMDATA/MISC`:
TR1..TR7, AL1..AL3, CL1..CL3, CY1..CY3). `--node N --screenshot out.ppm` renders one frame headless. Geometry and textures use the ported
loaders; the drawing is a stand-in until the original renderer is ported.

Drive (development): `./build/native/tnfs --drive TR1 [--car N] [--racers 1..8]` races car N (0..7: TSupra, LDiabl, P911, CZR1, F512TR, DVIPER,
ANSX, MRX7) against the other models as computer racers (`--racers 1` drives alone). Arrows steer, accelerate and brake; A / Z change gear (the
cars are automatic: A once from N to D). The results print when the race ends, with `TNFS_VERBOSE=1`. `--node TICKS --screenshot out.ppm` runs
that many steps at full throttle headless and renders one frame.

`--user DIR` puts the player's files in `DIR` and `--mods DIR` says where the mods are ([playing.md](playing.md#files)); a test run should use
scratch folders for both, never a player's own.

The game prints nothing to the console but fatal errors; `TNFS_VERBOSE=1` (or `--verbose`) turns on the rest (the race results, a shape or file
that could not be read). The other development variables are in [settings.md](settings.md).

Headless runs: `TNFS_CAPTURE_DIR` writes every n-th presented frame as PPM and `TNFS_INPUT_SCRIPT` replays input (`ms:KEY` held until the game
reads it, `ms:MOUSE:x:y`). Example: boot, move down twice, then choose Exit with the mouse:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software TNFS_CAPTURE_DIR=/tmp/frames \
  TNFS_CAPTURE_EVERY=1 TNFS_INPUT_SCRIPT="16000:DOWN,17000:DOWN,22000:MOUSE:320:400,23000:ENTER" \
  ./build/native/tnfs
```

## Tests

```sh
ctest --test-dir build/native --output-on-failure      # add -j N to run N at once
```

The unit tests need nothing but the build. The tests that read the CD's data (the assets, the races run headless) need `original/` in the source
tree and skip themselves without it, so a checkout with no game data runs fewer tests: compare `ctest -N`'s count before trusting a pass. Some
tests read the docs themselves (`docs/settings.md`'s tables are checked against the code).

A sanitizer build is a separate build directory configured with the flags in `CMAKE_C_FLAGS`, for example
`cmake -S . -B build/asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"`; the tests that
run the real game get a longer timeout there.

## CI

The workflows are in `.github/workflows/`:

| Workflow | When | What |
|----------|------|------|
| `ci.yml` (CI) | every push to `main` and every pull request | build and run the tests on macOS (Apple Silicon and Intel); the tests that need the CD's data skip themselves |
| `build-test.yml` (Build Test) | by hand (Actions > Build Test > Run workflow) | the universal macOS app and its DMG from the branch you pick, as a downloadable artifact; nothing is released |
| `release-macos.yml` (Release macOS) | a pushed tag `vX.Y.Z` or `vX.Y.Z-beta.N` | the universal macOS app and its DMG, in a draft GitHub Release that a maintainer publishes by hand |

## Layout

- `game/`: recovered game code; each function names its `NFS_WIN.EXE` address
  - `sys/`: EA system layer (timer, video and palette fades, input, resources)
  - `frontend/`: boot screens and the front-end state machine
- `core/formats/`: QFS (RefPack, Huffman, BTree) and FSH, verified against the original
- `game/platform/platform.h`: the only OS interface game code may use
- `game/platform/sdl3/`: SDL3 backend (native and Emscripten)
- `tests/`: the unit tests and the headless game tests `ctest` runs
- `tools/`: development scripts (frame and menu comparisons, mask tools)
- `docs/`: this and the other documents

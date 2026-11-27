# Building, running and testing

Spirited Drive is a C99 + SDL3 program for macOS, Linux, Windows and the web; the program is called `tnfs`. The game data is not part of this repository: extract it from your own CD into
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
TR1..TR7, AL1..AL3, CL1..CL3, CY1..CY3). `--node N --screenshot out.ppm` renders one frame headless. It draws with the ported renderer; Space
switches to an earlier stand-in that also draws the scenery (`TNFS_STAND_IN=1` picks it for `--screenshot`).

Drive (development): `./build/native/tnfs --drive TR1 [--car N] [--racers 1..8]` races car N (0..7: TSupra, LDiabl, P911, CZR1, F512TR, DVIPER,
ANSX, MRX7) against the other models as computer racers (`--racers 1` drives alone). Arrows steer, accelerate and brake; A / Z change gear (the
cars are automatic: A once from N to D). The results print when the race ends, with `TNFS_VERBOSE=1`. `--node TICKS --screenshot out.ppm` runs
that many steps at full throttle headless and renders one frame.

`--user DIR` puts the player's files in `DIR` and `--mods DIR` says where the mods are ([playing.md](playing.md#files)); a test run should use
scratch folders for both, never a player's own.

The game prints nothing to the console but fatal errors; `TNFS_VERBOSE=1` (or `--verbose`) turns on the rest (the race results, a shape or file
that could not be read). The other development variables are in [settings.md](settings.md).

Headless runs: `TNFS_CAPTURE_DIR` writes every n-th presented frame as PPM and `TNFS_INPUT_SCRIPT` replays input (`ms:KEY` held until the game
reads it, `ms:MOUSE:x:y`). The pointer's x and y are the frame's: 640x480 headless (the dummy display is 4:3), but a real window
follows the monitor's shape in Restomod (854x480 on a 16:10 or 16:9 display, the 4:3 menus centred 107 to the right), so a window
run's clicks need the frame it got (the `--verbose` log's "presentation rect ... for WxH"). Example: boot, move down twice, then choose
Exit with the mouse:

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

Every test carries two labels, a layer and a product, so a run can pick a set: `ctest --test-dir build/native -L unit -j N` runs the unit tests only
(a few minutes: the `test_*`/`fuzz_*` binaries and the static checks), `-L integration` the short scripts that start the game, the editor or a tool,
`-L e2e` the long walks of menus, races and editor flows (20 s or more each). The product is `core`, `game`, `editor` or `both` (a test that links
or starts the two); repeat `-L` to combine (`-L unit -L editor`). The labels are set at the end of `CMakeLists.txt` from what each test is: a binary
is unit, a script integration, and the slow scripts are named in `TNFS_E2E_TESTS` there (add a new long script to that list). `cmake` prints
`ctest labels: no layer for: …` if a test has none.

A sanitizer build is a separate build directory configured with the flags in `CMAKE_C_FLAGS`, for example
`cmake -S . -B build/asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"`; the tests that
run the real game get a longer timeout there.

## CI

The workflows are in `.github/workflows/`:

| Workflow | When | What |
|----------|------|------|
| `checks.yml` (Checks) | every push to any branch and every pull request (not when only `*.md`, `docs/` or `notes/` change), and by hand | in parallel: Linux in Release with Studio (the flake's toolchain, every test) and Windows (MSYS2 UCRT64 gcc: every test, the exe's smoke); on a push to main and on a run by hand also macOS Apple Silicon in Release with Studio (every test), so a green branch says nothing about the Mac; a compiler warning fails any; the tests that need the CD's data skip themselves |
| `nightly-checks.yml` (Nightly Checks) | every night on `main`, by hand, and called by the release workflow on the tagged commit | what the checks of a push leave out, every job read-only: macOS Apple Silicon and Intel in Release (with Studio), Apple Silicon also with AddressSanitizer + UndefinedBehaviorSanitizer; Linux (Ubuntu, the flake's toolchain) in Release and with the sanitizers; the game-only release configuration of the downloads on macOS and on Windows (every test, and the Windows zip runs by itself); and the update notice's local part (fake servers: silence, no hang, proxy) on macOS, Windows and Linux |
| `check-app-update-infra.yml` (Check app update infra) | once a week, by hand, and on `ci/update-**` branches | the update notice end to end on macOS, Windows and Linux against the live API (counts a start in its totals, so never on every push); the local part is in Nightly Checks |

Caches and cancelling: Checks and Nightly Checks cancel a run that a newer push to the same ref supersedes (on `main` every commit's Checks run stays, because main saves the ccache; a tag's run of Nightly Checks is never cancelled). Build Test and the release workflows are never cancelled. Caches: ccache (macOS, Windows and Linux, saved on `main` by Checks and Nightly Checks), the MSYS2 packages (setup-msys2's cache), and in Build Test SDL's universal build (keyed by `SDL_COMMIT`) and the Windows ccache. The release workflows use **no caches**: a release is a clean build that no poisoned cache can reach.
| `build-test.yml` (Build Test) | by hand (Actions > Build Test > Run workflow) | the universal macOS app and its DMG, the Windows zip and installer, and the Linux AppImage (`tools/package_appimage.sh`: built on Ubuntu 22.04, so glibc 2.35 or newer, with SDL3 linked in; `tools/appimage_smoke.sh` runs it by itself on clean Ubuntu 22.04 and 24.04; `tools/appimage_desktop_smoke.sh` opens it in a real X11 and Wayland window and uploads pictures of the screen, and with your own CD data as a third argument it reaches the boot and title screens) from the branch you pick, as downloadable artifacts; nothing is released |
| `release.yml` (Release) | a pushed tag `vX.Y.Z` or `vX.Y.Z-beta.N` | the universal macOS app (DMG), the Windows game (zip and installer) and the Linux game (AppImage, built on Ubuntu 22.04), each built and tested by its own job; when all pass, ONE draft GitHub Release with the downloads, their checksum lists and `RELEASE_NOTES.md`, that a maintainer publishes by hand |

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

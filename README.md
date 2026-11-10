# tnfs

Portable reimplementation of *The Need for Speed SE* (EA, 1996), recovered from
`NFS_WIN.EXE` with Ghidra. C99 + SDL3, targets macOS, Linux, Windows and the web.

Game data is not part of this repo. Extract it from your own CD into `original/`
(gitignored). The tables the original keeps in `NFS_WIN.EXE` are not in this repo either: the game reads them from that exe at
start-up (`game/exe/`), and says so and stops if the file is missing or is not the PC Special Edition's.

## Environments (Nix flake)

| Shell | Command | Contents |
|-------|---------|----------|
| native build | `nix develop` | cmake, ninja, SDL3, lldb |
| web build | `nix develop .#web` | emscripten, cmake, ninja |
| reverse engineering | `nix develop .#re` | Ghidra 12 (prebuilt), python3, DOSBox-X |

nixpkgs is pinned to `nixpkgs-26.05-darwin`, the last release that builds for
Intel Macs.

## Build

```sh
nix develop -c sh -c 'cmake -S . -B build/native -G Ninja && cmake --build build/native'
nix develop .#web -c sh -c 'emcmake cmake -S . -B build/web -G Ninja && cmake --build build/web'
```

Serve `build/web/` over HTTP (`python3 -m http.server -d build/web`) and open `tnfs.html`.

## Run

```sh
./build/native/tnfs [--data original] [--german]      # boot: EA logo, title, Game Mode menu
./build/native/tnfs --view original/FRONTEND/ART/TITLE.QFS   # browse FSH/QFS bitmaps
```

Track viewer (development): `./build/native/tnfs --track TR1` flies along a
track (Up/Down move, Left/Right turn; names as in `SIMDATA/MISC`: TR1..TR7, AL1..AL3,
CL1..CL3, CY1..CY3). `--node N --screenshot out.ppm` renders one frame headless.
Geometry and textures use the ported loaders; the drawing is a stand-in until the
original renderer is ported.

Drive (development): `./build/native/tnfs --drive TR1 [--car N] [--racers 1..8]`
races car N (0..7: TSupra, LDiabl, P911, CZR1, F512TR, DVIPER, ANSX, MRX7)
against the other models as computer racers (`--racers 1` drives alone).
Arrows steer, accelerate and brake; A / Z change gear (the cars are automatic:
A once from N to D). The results print when the race ends, with `TNFS_VERBOSE=1`.
`--node TICKS --screenshot out.ppm` runs that many steps at full throttle
headless and renders one frame.

The game prints nothing to the console but fatal errors; `TNFS_VERBOSE=1` (or `--verbose`)
turns on the rest (the race results, a shape or file that could not be read).

Headless runs: `TNFS_CAPTURE_DIR` writes every n-th presented frame as PPM and
`TNFS_INPUT_SCRIPT` replays input (`ms:KEY` held until the game reads it,
`ms:MOUSE:x:y`). Example: boot, move down twice, then choose Exit with the mouse:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software TNFS_CAPTURE_DIR=/tmp/frames \
  TNFS_CAPTURE_EVERY=1 TNFS_INPUT_SCRIPT="16000:DOWN,17000:DOWN,22000:MOUSE:320:400,23000:ENTER" \
  ./build/native/tnfs
```

## Playing: what the port adds

With everything off and the 4:3 format the game plays as the original. The additions:

- **Graphics** (Options → Graphics, one page):
  - format 4:3, 16:9 or 21:9, with the same 640x480-style pixels, just wider;
  - scaling: fill, or whole multiples only;
  - full screen and refresh 60 / 120 / 144 Hz (the race still steps at the original rate; the frames between steps are
    interpolated);
  - field of view;
  - view distance Close / Medium / Far / Ultra;
  - perspective-correct 3D, texture filter and fog (all off by default);
  - render threads: Auto by default (as many as the machine's logical cores, at most eight), or off (the original's one
    thread), 2 or 4. The picture is the same with any of them; the wall is quicker with the threads on. `TNFS_BANDS` in
    the environment wins over the setting.
  - render scale: 1x by default, or 2x or 3x — the many times the world is drawn over the frame's size, so the 3D
    picture is sharper the more of them. `TNFS_RENDER_SCALE` in the environment wins over the setting.
  The pause menu's Graphics opens the same screen; the rows that need a new race say "(next race)".
- **Controls** (Options → Controls setup): page 1 is the original's. Pages 2-3 bind any action (steering, gas, brake,
  gears, camera, handbrake, horn, pause, view, Nitro) to keys, pad buttons, triggers or axes. They also set the dead
  zone, sensitivity and force feedback.
- **Customize Game** (Game Mode, or Options), two pages:
  - Page 1 — Cheats: Explode, Nitro, Turbo, Warrior player car; Unlocks & mods: Warrior as a rival, mirror and rally
    tracks, Lost Vegas, Lunar Springs (the cut moon track, run as Rusty Springs' rally), unlock level, time of day with
    variants;
  - Page 2 — In-race options: speed units, no driving assists, bumper-view mirror and radar, replay cameras in races,
    rival car markers, 3D render stats, dash box (the map, the gear lever or neither); Miscellaneous: start screen
    (Game Mode, Network or Control Central), intro videos.
  The typed cheat codes still work. With a cheat on, Control Central shows CHEATS ENABLED.
- **Records**: mirror and rally have their own tables. A race with Explode, Nitro, Turbo or the Warrior goes to a
  separate "cheats" table, each time marked N / E / T / W (Best Times' "Cheats" footer item shows it).
- **Keys in a race**: F1 / F2 the bumper view's mirror and radar, F3 view distance, F5 sound, F6 rival car markers,
  F7 / F8 HUD, F9 replay, F12 the 3D render stats overlay. With the Nitro cheat, hold N (or the pad's Nitro button)
  at the start for the launch.

### Files

The game's own `config.dat` is kept as the original writes it. The port adds its own files beside it, in the player's
folder: one folder named `TNFS Restomod` in the OS's per-user data directory (`~/Library/Application Support/TNFS Restomod/`
on macOS, `%APPDATA%\TNFS Restomod\` on Windows, `~/.local/share/TNFS Restomod/` on Linux). `--user DIR` puts it at `DIR`
instead (a portable install, a test); `TNFS_PREF_DIR` does the same for the tests. `--mods DIR` only says where the mods
are (by default `<player's folder>/mods/`, when it exists) and holds none of the player's files. Anything the earlier
folders held (`tnfs/tnfs/` in the same place, or a mods directory's `user/`) moves to the new one once, said in `tnfs.log`; a
file that is already in the new folder stays and the old copy is left alone.

| File | Contents |
|------|----------|
| `port.cfg` | The mode in use (Stock, Restored or Restomod) and the PS1 folder |
| `stock.cfg`, `restored.cfg`, `restomod.cfg` | What the player changed in that mode, and only that |
| `controls.cfg` | the bindings and axis settings |
| `extras.cfg` | the EXTRAS page's switches |
| `records.dat` | the records the original has no place for: variants and the cheats table |
| `config.dat`, `Replay.rpl`, saved games | the game's own player file, the last replay and the saves |
| `tnfs.log` | what the last run said (send it with a bug report; the EXTRAS page opens the folder) |
| `gamedata/` | the player's copy of the CD, made by the first-run wizard |
| `mods/` | the default place for a mod (an overlay of the game's files) |

## Network game

Two or more machines race over UDP (the native build only; the web and Windows
builds have no network yet). Each machine runs its own copy:

```sh
./build/native/tnfs [--net-port N] [--net-host IP:PORT]...
```

- `--net-port N`: the UDP port this machine uses, 15000 by default.
- `--net-host IP:PORT`: also look for games at this address, besides the LAN
  (up to 8). Needed when the machines are not on the same LAN, or the LAN
  broadcast does not reach.
- Without flags, a machine finds games by a broadcast to the LAN on its own
  port, so on a LAN keep the same port on every machine.

In the menus:

1. Game Mode: **Network**, and enter your name.
2. Network Setup: Servers is **UDP (LAN / IP)**. Choose **Select game**:
   - to host: **New Game**;
   - to join: the host's game in the list.
3. Control Central: the host sets the race type and location, then chooses
   **Race** (bottom left); the others start with it.

**View only** on Network Setup (Yes) joins a game to watch it, with no car.
Set it before Select game.

Firewall: allow incoming UDP on the port (15000) on every machine. Over the
internet, the host's router must forward that UDP port to the host.

If the machines don't see each other:
- check they use the same port, or start the joiner with
  `--net-host HOST_IP:PORT`;
- check that the firewall lets UDP through on that port, on both machines;
- run from a terminal: a port that cannot be opened prints
  `net_udp_open: port N: ...` (for example, already in use).

## Analysis

```sh
nix develop .#re -c sh -c '"$(dirname "$(readlink -f "$(which ghidra)")")/support/analyzeHeadless" \
  ghidra/project tnfs -import original/NFS_WIN.EXE -overwrite \
  -scriptPath ghidra/scripts -postScript ExportMap.java "$PWD/analysis/nfs_win"'
```

`ExportMap.java` writes memory blocks, imports, functions, call edges and
strings as TSV to `analysis/nfs_win/`. Helpers: `tools/callgraph.py` (paths and
trees), `tools/fsummary.py` (per-function APIs, strings, callees),
`tools/peek.py` (initialised data), `tools/emu_oracle.py` (runs original
functions under Unicorn for byte-exact checks). Open the project interactively with `nix develop .#re -c ghidra`.

## Layout

- `game/`: recovered game code; each function names its NFS_WIN.EXE address
  - `sys/`: EA system layer (timer, video and palette fades, input, resources)
  - `frontend/`: boot screens and the front-end state machine
- `core/formats/`: QFS (RefPack, Huffman, BTree) and FSH, verified against the original
- `game/platform/platform.h`: the only OS interface game code may use
- `game/platform/sdl3/`: SDL3 backend (native and Emscripten)
- `ghidra/scripts/`: headless Ghidra scripts (`tools/ghidra_rebuild.sh` runs the full pipeline)
- `analysis/`: exported maps of the original binary

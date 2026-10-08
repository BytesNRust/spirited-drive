# tnfs

Portable reimplementation of *The Need for Speed SE* (EA, 1996), recovered from
`NFS_WIN.EXE` with Ghidra. C99 + SDL3, targets macOS, Linux, Windows and the web.

Game data is not part of this repo. Extract it from your own CD into `original/`
(gitignored).

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

## Analysis

```sh
nix develop .#re -c sh -c '"$(dirname "$(readlink -f "$(which ghidra)")")/support/analyzeHeadless" \
  ghidra/project tnfs -import original/NFS_WIN.EXE -overwrite \
  -scriptPath ghidra/scripts -postScript ExportMap.java "$PWD/analysis/nfs_win"'
```

`ExportMap.java` writes memory blocks, imports, functions and strings as TSV to
`analysis/nfs_win/`. Open the project interactively with `nix develop .#re -c ghidra`.

## Layout

- `src/platform/platform.h`: the only OS interface game code may use
- `src/platform/sdl3/`: SDL3 backend (native and Emscripten)
- `ghidra/scripts/`: headless Ghidra scripts
- `analysis/`: exported maps of the original binary

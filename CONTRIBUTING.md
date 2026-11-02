# Contributing to TNFS Restomod

Thanks for helping restore a 1996 classic.

## The one hard rule

**Never add game data** to the repository, an issue, a pull request or a release: nothing from the PC CD or the
PlayStation disc — no textures, tracks, cars, sounds, music, videos, executables or dumps of them, not even small
pieces in tests. Pull requests that contain any are closed. Our own data (light maps, car masks, artwork) is fine,
under CC BY-SA 4.0.

Do not ask for, or link to, copies of the game.

## Reporting bugs

Open an issue with the **Bug report** template. Please include:

- the game version (main menu corner, or Options);
- your OS and Mac model (Intel or Apple Silicon);
- the steps to reproduce, what you expected and what happened;
- the log file (the template says where it is).

Questions, ideas and show-and-tell go to [Discussions](../../discussions), not issues.

## Building

You need CMake 3.24+, Ninja, a C99 compiler and SDL 3.

```sh
cmake -S . -B build/native -G Ninja
cmake --build build/native
ctest --test-dir build/native
```

With Nix: `nix develop -c sh -c '…the same…'`. The game reads its data from `original/` (copy your CD there, it is
gitignored) or from the folder chosen in the first-run screen.

## Pull requests

1. Fork the repo and branch from `main`.
2. Keep each pull request to one change. Behaviour changes go behind a setting whose default `0` means "as the
   original executable".
3. Add or update tests; `ctest` must pass.
4. Commit messages: `<type>: <description>` (`feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`, `ci`).
5. Open the pull request with what changed and why. CI must be green and a maintainer must approve it.

Recovered functions name the original executable's address in a comment. Keep that convention.

## Licence of contributions

By submitting a pull request you agree that your contribution is licensed under the
**GNU General Public License v3.0 or later** (code) or **CC BY-SA 4.0** (data and artwork), the same licences as
the project, and that you have the right to submit it.

## Conduct

Be kind, assume good faith, keep it about the work. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

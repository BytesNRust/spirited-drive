# The exe's tables: what the game needs from your CD

Some of the original game's tables (the in-race menu's map, the viewports, the AI drivers,
the split times, the track renderer's topology and the default records) live inside its
program file, `NFS_WIN.EXE`. This port does not carry them. At start-up it reads them from
your own copy of that file, so the file has to be there.

## Which file

`NFS_WIN.EXE` from the **PC Special Edition** CD of *The Need for Speed*. The port checks
that it is exactly that build before reading anything from it:

| | |
|---|---|
| Size | 1069056 bytes |
| SHA-256 | `ac72e595…2594b` (the full digest is in `game/sys/data_manifest.c`) |

Another release of the game, or a patched or cracked exe, keeps those tables at other
places, so it is refused (see below) instead of read wrongly.

## Where to put it

Next to the rest of the CD's files, in the game-data folder:

- **First run.** The wizard asks you to choose the CD's folder and copies the data into
  your player folder (`gamedata/`). If `NFS_WIN.EXE` is in the folder you chose, nothing
  more is needed.
- **By hand.** Copy `NFS_WIN.EXE` into `<player folder>/gamedata/`:
  - macOS: `~/Library/Application Support/Spirited Drive/gamedata/`
  - Windows: `%APPDATA%\Spirited Drive\gamedata\`
  - Linux: `~/.local/share/Spirited Drive/gamedata/`
- **Another folder.** `tnfs --data DIR` reads the CD's files, `NFS_WIN.EXE` included, from
  `DIR`. A development build also looks in `./original`.

The file name is matched without regard to upper or lower case.

## What it says when something is wrong

The game stops at start-up with one of these, and the same text goes to `spirited-drive.log` in the
player folder (attach that file to a bug report):

| Message | Meaning and fix |
|---|---|
| `NFS_WIN.EXE not found in <folder>. The port reads its tables from the PC Special Edition's own file: choose the folder of that CD.` | The file is not in the data folder. Copy it there, or point `--data` at the CD's folder. |
| `NFS_WIN.EXE in <folder> is not the PC Special Edition build this port was made for (…)` | The file is there but is another build. The message gives the size and SHA-256 found and expected; use the Special Edition CD's file. |
| `cannot read <path>` | The file exists but cannot be opened: check its permissions. |
| `out of memory reading the exe's tables` | The machine ran out of memory while loading the file. |
| `NFS_WIN.EXE: the <family> tables: <why>` | The file passed the size and digest check but a table did not read as expected; report it with `spirited-drive.log`. |

Nothing from the exe is written anywhere: the tables are read into memory at each start.

# Playing Spirited Drive

Spirited Drive plays *The Need for Speed SE* (PC, 1996) from your own copy of the game, in one of three modes. You pick one at the
first start, and you can change it any time on the top row of the Game Mode screen (`‹ Restomod '26 ›`, Left / Right or a click):

- **Stock '96**: the original as it shipped, 4:3, with only its bugs fixed;
- **Restored '96**: the same look, widescreen and up to 144 Hz;
- **Restomod '26**: the original game with modern running gear: the additions below, on by default, and everything yours to tune.

What you change is kept for that mode alone. The additions:

- **Graphics Detail** (Options → Graphics, one page):
  - format 4:3, 16:9 or 21:9, with the same 640x480-style pixels, just wider;
  - scaling: fill, or whole multiples only;
  - full screen and refresh 60 / 120 / 144 Hz (the race still steps at the original rate; the frames between steps are
    interpolated);
  - field of view;
  - view distance Close / Medium / Far / Ultra;
  - perspective-correct 3D, texture filter and fog;
  - light effects: the tracks' baked light and the cars' soft shadows;
  - render threads: Auto by default (as many as the machine's logical cores, at most eight), or off (the original's one
    thread), 2 or 4. The picture is the same with any of them; the game runs quicker with the threads on.
  - render scale: 1x by default, or 2x or 3x: the 3D world is drawn at that many times the frame's size and scaled
    down, so it is sharper (and slower) the higher it is.
  The pause menu's Graphics opens the same screen; the rows that need a new race say "(next race)".
- **Controls** (Options → Controls setup): page 1 is the original's. Pages 2-3 bind any action (steering, gas, brake,
  gears, camera, handbrake, horn, pause, view, Nitro) to keys, pad buttons, triggers or axes. They also set the dead
  zone, sensitivity and force feedback.
- **Customize Game** (Game Mode, or Options), three pages:
  - Page 1 — Cheats: Explode, Nitro, Turbo, Warrior player car; Unlocks & mods: Warrior as a rival, mirror and rally
    tracks, Lost Vegas, Lunar Springs (the cut moon track, run as Rusty Springs' rally) and its moon gravity, unlock
    level, car colours;
  - Page 2 — In-race options: speed units, no driving assists, bumper-view mirror, police radar in the exterior views,
    replay cameras in races, rival car markers, 3D render stats, dash box (the map, the gear lever or neither), 360
    camera, difficulty (Easy, Original, Hard);
  - Page 3 — Miscellaneous: start screen (Game Mode, Network or Control Central), intro videos, updates and anonymous
    stats, open the log folder, restore the mode's defaults.
  The typed cheat codes still work. With a cheat on, Control Central shows CHEATS ENABLED.
- **Records**: mirror and rally have their own tables. A race with Explode, Nitro, Turbo or the Warrior goes to a
  separate "cheats" table, each time marked N / E / T / W (Best Times' "Cheats" footer item shows it).
- **Keys in a race**: F1 / F2 the bumper view's mirror and radar, F3 view distance, F5 sound, F6 rival car markers,
  F7 / F8 HUD, F9 replay, F12 the 3D render stats overlay. With the Nitro cheat, hold N (or the pad's Nitro button)
  at the start for the launch.

Every setting, its range and what each mode sets is in [settings.md](settings.md). Network races are in
[online.md](online.md), mods in [modding.md](modding.md).

## Files

The game's own `config.dat` is kept as the original writes it. Spirited Drive adds its own files beside it, in **the game's user folder**:

<!-- the one place the folder's name and paths are written: a rename changes this line -->
the game's user folder is `Spirited Drive` in the OS's per-user data directory: `~/Library/Application Support/Spirited Drive/` on macOS, `%APPDATA%\Spirited Drive\` on Windows, `~/.local/share/Spirited Drive/` on Linux.

`--user DIR` on the command line puts the game's user folder at `DIR` instead (a portable install). `--mods DIR`
only says where the mods are (by default the `mods/` folder in the game's user folder, when it exists) and holds none of the player's files.
Anything the earlier folders held (`TNFS Restomod/`, then `tnfs/tnfs/`, in the same place, or a mods directory's `user/`) moves to the game's user folder once, said in
`spirited-drive.log`; a file that is already there stays and the old copy is left alone.

| File | Contents |
|------|----------|
| `port.cfg` | The mode in use (Stock, Restored or Restomod) and, if you have one, the folder of a PlayStation disc's files |
| `stock.cfg`, `restored.cfg`, `restomod.cfg` | What the player changed in that mode, and only that |
| `controls.cfg` | the bindings and axis settings |
| `extras.cfg` | Customize Game's switches |
| `records.dat` | the records the original has no place for: variants and the cheats table |
| `config.dat`, `Replay.rpl`, saved games | the game's own player file, the last replay and the saves |
| `spirited-drive.log` | what the last run said (send it with a bug report; Customize Game's page 3, Open the log folder, shows it) |
| `gamedata/` | your copy of the game's files, made at the first start from your CD, a folder or an ISO image |
| `mods/` | the default place for a mod (an overlay of the game's files) |

# Playing: what the port adds

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

Every setting, its range and what each mode (Stock, Restored, Restomod) sets is in [settings.md](settings.md). Network races are in
[online.md](online.md), mods in [modding.md](modding.md).

## Files

The game's own `config.dat` is kept as the original writes it. The port adds its own files beside it, in **the game's user folder**:

<!-- the one place the folder's name and paths are written: a rename changes this line -->
the game's user folder is `TNFS Restomod` in the OS's per-user data directory: `~/Library/Application Support/TNFS Restomod/` on macOS, `%APPDATA%\TNFS Restomod\` on Windows, `~/.local/share/TNFS Restomod/` on Linux.

`--user DIR` puts the game's user folder at `DIR` instead (a portable install, a test); `TNFS_PREF_DIR` does the same for the tests. `--mods DIR`
only says where the mods are (by default the `mods/` folder in the game's user folder, when it exists) and holds none of the player's files.
Anything the earlier folders held (`tnfs/tnfs/` in the same place, or a mods directory's `user/`) moves to the game's user folder once, said in
`tnfs.log`; a file that is already there stays and the old copy is left alone.

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

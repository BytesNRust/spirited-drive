# Original-bug fixes the port makes (inventory for the profiles)

Base: origin/main 699ec6a8. Revised after checking each fix against the binary (see section 6). Written for M1 phase 2: "Stock '96 = the exe, bugs included" against "Restored '96 = bugs fixed".
No code in this change. The doc is for a decision by the owner.

How it was made: the `port:` comments and `port_*` fields in `game/` and `core/`, plus the `fix:`/`feat:` commit subjects on main.
It is a reading of the code, not a run of the exe against the port. Where a claim is not checked it says "to verify".
Fixes that only make the port match the exe (the many `(0x00...) checked against the original` commits) are not listed:
they are Stock behaviour and need no switch.

## 0. The finding that shapes everything

Every behavioural fix below has its `0 = the exe` branch in code, but **nothing a player can set reaches it**.
They are hard-wired to 1 where the race is built:

- `game/viewer/track_viewer.c:477-480` sets `port_replay_launch`, `port_nitro_start`, `port_pad_and_keys` and `port_stop_replaced` to 1.
- `game/viewer/drive_pause.c:130` sets `port_resume_keys` to 1.
- `game/viewer/drive_replay.c:379` sets `port_resume` to 1.
- `TNFS_WIDE_BOARD` is an environment variable read in `ported_view.c`; default on.

So today **Stock '96 does not reproduce the exe's bugs** in the real game. Only the oracles run the 0 paths, by building the structs themselves.
The profile table (`port_profile.c`, feat/profiles) has no column for any of them.
Making Stock honest needs one place the profile can reach, which is the proposal in section 5.

## 1. Fixes with a flag already (0 = exe), always on today

| # | Exe behaviour | Port fix | Flag (where) | Stock | Restored | Restomod |
|---|---|---|---|---|---|---|
| 1 | A looping horn (voices 0x3f..0x41) or skid (0x2a) voice is not cut when another sound takes its channel: the record is lost and it loops to the end of the race | The voice is cut when its channel is taken | `RaceState.port_stop_replaced` (set at `track_viewer.c:480`; logic in `race_sound.c`) | 0 | 1 | 1 |
| 2 | After the pause menu closes, ESC is dead for `MENU_GAP` (0xb4) ticks, and back from a replay the ESC that closed it is read as the menu's | ESC and the pause key work at once; ESC while paused (P) also ends the pause and opens the in-race menu (the exe waits for P alone), `port_menu_after_pause` | `port_resume_keys` in `DrivePause`, `DriveMenu`, `RacePauseInputs` (`drive_pause.c:130`; state `port_esc_up_wait`/`port_pause_up_wait` in `race_schedule.c`) | 0 | 1 | 1 |
| 3 | A scenery board is dropped whole when one corner is past +-0x800 across (0044a450) while its middle shows: a big tree or a building's face pops out near the screen edge | Drawn by the near call, clipped at the near plane, perspective-correct | Env `TNFS_WIDE_BOARD` only (`ported_view.c`), default on; `track_port_wide_board` in `track_object_draw.c`. To verify: whether it needs Modern 3D (the near call is `ops.port_near_camera`) | 0 | 1 | 1 |

Reading of the columns: Stock 0 is the whole point of Stock. Restored 1 is the "bugs fixed" promise. Restomod 1 because it is Restored plus more.
Items 1-2 are exe behaviour confirmed at the binary (6). Item 3 is the exe's own cull (`0044a450`), a rendering artefact; if the owner wants Restored to be pixel-identical to the exe in a 4:3 frame, move it to Restomod only.

## 2. Fixes with no flag

### 2.1 Crash and fault guards (the exe faults or reads past a table)

The exe has no behaviour to keep here: it crashes, or reads whatever is after the table. The guard is the same in every profile.
A Stock that "includes every bug" cannot include a crash that kills the process, so these stay on in Stock too. No flag proposed.
(The profile blurb "Every bolt, every bug" is still honest for behaviour, but should not promise crashes.)

| Place | The exe |
|---|---|
| `game/frontend/central_draw.c:95` | a "r" past the tenth track reads the colours' first as a name: faults |
| `game/viewer/object_sink.c:16` | past the last sink table: crashes |
| `game/viewer/drive_slide.c:18`, `game/race/cockpit.h:222`, `hud.c:123`, `race_chase.c:313,543`, `mirror_scene.c:170`, `race_sound.c:966`, `race_replay_seek.c:322`, `ai_setup.c:75` | table read past its end |
| `game/race/race_splits.c:133`, `car_physics.c:109`, `audio/sound_stream.c:25`, `audio/sound_driver.c:303`, `hud_redraw.c:33`, `gfx/flat_edge.c:40` | divide by 0 or INT_MIN / -1 |
| `game/race/ai_police.c:111`, `race_camera.c:64`, `dash_load.h:18`, `puffs.h:51` | NULL or missing data (no leader, no viewed car, no .mmb, a bank with no '!PAL') |
| `game/net/net_queue.h:89` | a chain walked past its end |

### 2.2 Uninitialised memory made deterministic

- `game/gfx/flat.c:13`, `game/gfx/quad.c:8`: the exe's per-row tables live on the stack uninitialised; the port zeroes them. Visible only for concave or self-crossing outlines.
- `game/race/sky_ring.h:42`: `0043abfc` copies uninitialised memory into `ring[1], [5], [6]` and `[0x69..0x88]`; the exe never reads it, the port does not keep it.

The exe's output there is not reproducible, so there is nothing to put behind a Stock switch. No flag.

### 2.3 Limits of the exe lifted

- `dd8525f5`: the intro movies play from a data folder with a path over 0x50 bytes (the port allows 512). Needed for any install under a deep path (an .app in a long folder). No flag; Stock would only mean "movies silently missing" for long paths.
- `f07112c1`: the first call of the segment window loads the segments ahead of the camera wherever it is; the exe's loop only fills them from a start near segment 0. To verify whether the exe path can reach it (replay seek, `TNFS_START_NODE`); if only the port's own starts hit it, no flag.
- `port_free_laps` (`race_state.h:453`): places the lap count outgrows (the splits' 85, 20 lap times, the finish estimate's int32, the AI zones' int16) guard themselves. Set from `free_race` (`track_viewer.c:462`), so it is already only on when the player picks a free lap count. No change.

## 3. Items that look like fixes and are not (do not put them under "bugs fixed")

3.0 Extensions of the exe's format (moved here from the table): `port_replay_launch` records the NITRO start in the replay and plays it back. The exe's launch routine only runs with `playback == 0`, so a replay never launches: an exe limitation we extended (feat `c392d3d8`, `269247ec`), not a fix. It depends on `port_nitro_start`; they move together.

3.1 `port_nitro_start` (N alone opens the launch) and `port_pad_and_keys` (keys and pad both live; the exe's config.dat device words pick one device per action) are input design changes.
Hard-wired 1 as well. Same story as section 0: propose they follow the switch of section 5, or stay as player options in the controls screen. Owner's call.

3.2 Wide-screen consequences, not bugs: `port_reach`/`port_all_sides` (cars and models the exe culls by its 90 degree cone), `port_wide` (sky slices), `port_eye_ahead`, `port_near_scale`, `port_near_camera`, `TNFS_TWO_SIDED`/`port_two_sided_scenery`.
They are 0 or automatic in a 4:3 frame (`port_two_sided_scenery(automatic)` follows reverse/follow camera). They follow the profile's `widescreen` column already; nothing to decide. To verify: that all of them are 0 in 4:3 with no env var.

3.3 Port additions behind their own settings: `port_banner` (CHEATS ENABLED banner and its redraw, `central_banner.c`; 0 is the exe), `port_combine_variants` (EXTRAS Mirror and Rally together; the exe's switch never clears the other variant, so both can end set), `port_look_back`, `port_follow_camera`, `port_f3_ultra`, lunar gravity, rally gravel, difficulty, free records.
Not bugs. `port_banner` is the one with a Stock-vs-port argument: it is on-screen feedback the exe lacks. Proposal: Stock 0, Restored 0 (looks like the exe), Restomod 1. No assignment of `port_banner` was found in `game/` outside the ops initializer; to verify what the real menus pass.

3.4 Lighting and paint fixes on the port's own looks (night recolour inside the lamp's rectangle, black-body tones, lamp slots cleared at race end `3ce6f500`, mirror pass PS1 light per node): bugs of port features, so they ride on those features' switches (`psx_light`, `car_colours`, `emissive_lamps`). Nothing to add.

## 4. Gaps found while reading

- Honest Stock needs the exe's own bugs *in the real game*, and today only the oracles have them. Section 0.
- No test or oracle asserts that flag 0 still equals the exe for items 1-4 outside the oracles' hand-built structs. When the switch exists, one test per item should build the race through the real path with the profile Stock and check the 0 branch ran.
- `TNFS_WIDE_BOARD` has no settings key and no oracle named in its commit.

## 5. Proposal (for the owner to choose)

**A. One switch (recommended).** A new `PortSettings` key `bug_fixes=0|1` (menu row "Original bugs: Kept / Fixed", Options page next to the profile row), written by the profile table as one more column:

| Stock | Restored | Restomod |
|---|---|---|
| 0 | 1 | 1 |

`port_bug_fixes()` (same shape as `port_psx_flares()`: env `TNFS_BUG_FIXES` overrides, else port.cfg) is read at the places in section 0 for items 1-3 instead of the literal 1.
Covers items 1-3. Section 2 stays always on. Cost: small, mostly the table column, the key, the row and the five call sites; the Options undo already restores port.cfg.

**B. One key per fix** (`stop_replaced`, `resume_keys`, `resume`, `replay_launch`, `wide_board`). More exact, but five rows that mean nothing to a player, and the profile table grows five columns. Only worth it if the owner wants to mix (for instance the horn fix without the ESC fix). Not recommended.

**C. Leave as is** and drop the claim "Stock = exe including bugs" from the blurb ("Factory spec. Every bolt, every bug." / "Same look, bugs fixed"). Zero code, but then Stock and Restored differ only in screen format and refresh.

Decisions needed from the owner:
1. A, B or C.
2. Whether item 5 (wide board) is "bug fixed" (Restored on) or Restomod only.
3. Whether `port_nitro_start` / `port_pad_and_keys` (3.1) follow the switch or become player options.
4. `port_banner` default per profile (3.3).

## 6. Check against the binary (revision)

| Fix | Verdict | Evidence |
|---|---|---|
| Horn/skid voice (`port_stop_replaced`) | Exe | `0044404c` writes the channel's record (`0050a4cc/d0`) over a playing voice without stopping it (read in the decompile; no sound oracle) |
| ESC gap (`port_resume_keys`) | Exe | `004557e8` stores the tick in `00513e54`, `004557f4` needs `+ 0xb4`; `tools/oracle_input.py` `menukeys` runs the exe's `0x4557F4` |
| Wait leaving a replay (`port_resume`) | **Port regression, flag removed** | the exe's `0044fe84` (lines 60-61) puts `004c5590` back with the timer; the `--drive` host did not (`8ecf685f` broke it, `c66eae0a` fixed it). Its "0" was never the exe. Branch `fix/resume-no-flag` drops the flag and keeps the fix always on |
| NITRO start in replays (`port_replay_launch`) | Extension | the exe's launch needs `playback == 0` |
| Wide board (`TNFS_WIDE_BOARD`) | Exe cull | `0044a450` rejects a board with a corner past +-0x800 |

The same-day regressions of the ESC fix (`2e45296e`, the ESC held at return from a replay in `c66eae0a`) were ours too; they sit on the `port_resume_keys` path.

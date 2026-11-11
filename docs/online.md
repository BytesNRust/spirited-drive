# Online games (LAN and internet)

A link game is the original's own: one machine hosts, up to seven join, and every machine runs the race from the same setup and the players' inputs.
Spirited Drive carries it over UDP (`--net-port N`, `--net-host IP:PORT` for a game the LAN's broadcast does not reach).

## Playing a network game

Two or more machines race over UDP (the macOS and Linux builds; the web and Windows
builds have no network yet). Each machine runs its own copy. Without options the game
finds games on the LAN by itself; to give it options, start it from a terminal:

```sh
"/Applications/Spirited Drive.app/Contents/MacOS/tnfs" [--net-port N] [--net-host IP:PORT]...   # the macOS app
./build/native/tnfs [--net-port N] [--net-host IP:PORT]...                                      # a build of your own
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
- look in `spirited-drive.log` (Customize Game, page 3, Open the log folder): a port that
  cannot be opened says `net_udp_open: port N: ...` (for example, already in use).

## What the host decides

Every machine runs the host's race: the original's setup (track, race type, laps, cars, track flags) and these rules of
Spirited Drive's, so that no two machines run different physics or draw a different race:

| Rule | From the host |
|------|---------------|
| mode | Stock '96, Restored '96 or Restomod '26: the session's shared rules |
| difficulty | Easy, Original or Hard for the rivals, the police and the traffic |
| weather, rain grip | clear, fog, rain or storm, and whether wet roads grip less |
| time of day | the exe's (morning, midday, evening) and the baked light's |
| reverse | the circuit driven the other way round |
| the moon | Lunar Springs' textures, the moon's gravity, rally gravel |
| cheats | NITRO, EXPLODE and TURBO |
| car colours | whether CUSTOMIZE colours are on, and the paint of each human's car |

Transmission, ABS and traction control stay each player's own choice, as in the original. Settings that only change what one player's own screen or
sound shows are not the host's and are not sent.

A guest never writes the host's values into its own files: they last for the race.

## Colours

Each player's car has the paint that player picked. The rivals' paint is drawn from the race's seed on the host, so every machine shows the same colour
on every car.

## For developers

### Colours on the wire

Each guest sends its paint with its info record (`paint_pick` in the entry, spare byte +0x3d). The host puts every human's paint in its list; the one
car a rival must differ from is the host's.

### The rules on the wire

The setup goes in chunks `{1, 0, chunk, bytes}` of up to 0xf7 bytes, then `{1}`. The host's rules are one more chunk, number 4, before the end. A receiver
that knows no rules drops it, because chunk 4 lies past the end of the setup. A guest whose host sent none (an older version) keeps what it did before:
Original difficulty, no reverse, its own settings.

The record is `TNFSSYN1`, a version byte, then one byte per field in the order of `game/race/session_rules.h`; a later version may add fields at the end.

### Testing it

`tools/link_loopback.sh BUILD_DIR finish|continue|quit|quitclick [OUT_DIR]` runs a host and a guest on one machine (UDP on localhost, SDL dummy drivers).
`HOST_ENV="TNFS_WEATHER=rain TNFS_REVERSE=1"` sets the host's environment only; the guest's log then says `session rules: ... weather 2 reverse 1`.

### Trying it on one Mac

`tools/try_online_local.sh` opens two game windows, a host and a guest, that talk over UDP on localhost. Nothing needs a second computer.

1. Run it from the folder that holds `original/` (the game's data) and `mods/`: `tools/try_online_local.sh /path/to/tnfs`, or `./tnfs` / `build/release/tnfs` found by itself.
   It makes `user/` (the host's player folder) and `user-guest/` (the guest's) there and writes nothing else. The host listens on UDP 15000, the guest on 15001.
   Both windows run for at most two hours (`timeout -k`); closing one window, or Ctrl-C in the terminal, closes the other.
2. In the first window (the host): Game Mode, **Network**, your name, then Network Setup's **Select game** and **New** in the box of games. In Control
   Central choose the track, race type and settings you want (weather, reverse, difficulty and the car colours are the host's), and wait there.
3. In the second window (the guest): the same screens, and in the box of games pick the host's game. The host's Control Central then lists the guest,
   and the host starts the race with the button at the bottom left.
4. In the race, look for:
   - the host's weather (rain, fog) and the circuit's direction on both windows, whatever the guest has in its own settings;
   - the same colour on each car in both windows (a car's paint is the same in every slot), and the guest's own paint where it chose one;
   - the same result table when the race ends.
5. `user/spirited-drive.log` and `user-guest/spirited-drive.log` are the logs. A guest's log says `session rules: ... weather 2 reverse 1 ...` when it got the host's rules.

`DRY_RUN=1 tools/try_online_local.sh` only prints the two commands.

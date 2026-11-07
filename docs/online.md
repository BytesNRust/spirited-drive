# Online games (LAN and internet)

A link game is the original's own: one machine hosts, up to seven join, and every machine runs the race from the same setup and the players' inputs.
The port carries it over UDP (`--net-port N`, `--net-host IP:PORT` for a game the LAN's broadcast does not reach).

## What the host decides

The host's setup (track, race type, laps, cars, seed, track flags) was always sent. The port's own settings used to be read on each machine, so two
machines could run different physics or draw a different race. Now the host sends them too, after the setup, and every machine runs the host's:

| Rule | From the host |
|------|---------------|
| mode | Stock '96, Restored '96 or Restomod '26: the session's shared rules |
| difficulty | Easy, Original or Hard for the rivals, the police and the traffic |
| weather, rain grip | clear, fog, rain or storm, and whether wet roads grip less |
| time of day | the exe's (morning, midday, evening) and the PS1 light's |
| reverse | the circuit driven the other way round |
| the moon | Lunar Springs' textures, the moon's gravity, rally gravel |
| cheats | NITRO, EXPLODE and TURBO |
| car colours | whether CUSTOMIZE colours are on, and the paint of each human's car |

Transmission, ABS and traction control stay each player's own choice, as in the original. Settings that only change what one player's own screen or
sound shows are not the host's and are not sent.

A guest never writes the host's values into its own files: they last for the race.

## Colours

Each guest sends the paint it picked for its car with its info record (`paint_pick` in the entry, spare byte +0x3d). The host puts every human's paint in
its list; the rivals' paint is the race's draw from the seed, and the one car a rival must differ from is the host's, so every machine draws the same colours
in every slot.

## On the wire

The setup goes in chunks `{1, 0, chunk, bytes}` of up to 0xf7 bytes, then `{1}`. The host's rules are one more chunk, number 4, before the end. A receiver
that knows no rules drops it, because chunk 4 lies past the end of the setup. A guest whose host sent none (an older version) keeps what it did before:
Original difficulty, no reverse, its own settings.

The record is `TNFSSYN1`, a version byte, then one byte per field in the order of `game/race/session_rules.h`; a later version may add fields at the end.

## Testing it

`tools/link_loopback.sh BUILD_DIR finish|continue|quit|quitclick [OUT_DIR]` runs a host and a guest on one machine (UDP on localhost, SDL dummy drivers).
`HOST_ENV="TNFS_WEATHER=rain TNFS_REVERSE=1"` sets the host's environment only; the guest's log then says `session rules: ... weather 2 reverse 1`.

# Sounds the port adds

The game's own sounds are the exe's (the banks the race loads). Everything here is the port's, laid over the race's mix, and **only in Restomod '26**: each one is behind a flag that is 0 in Stock and Restored, so those two play exactly the exe's sounds. Every sample is CC0 or public domain, with its source, author, licence and check date in [`data/sounds/LICENSES.md`](../data/sounds/LICENSES.md); the test of each sound checks that its entry is there. The flags are in [settings.md](settings.md).

All five are 16-bit mono WAVs at the mixer's rate (22050 Hz) in `data/sounds/`, read once when first wanted. A missing or unusable file leaves that sound silent and says so once in the log (`wet_roll: no sounds/<file>: the roll is silent`); nothing else breaks.

| sound | file | plays | flag (env) | LICENSES.md entry |
|---|---|---|---|---|
| Thunder | `thunder.wav`, 13.4 s | in a storm, after each lightning | `rain_v2` (`TNFS_RAIN_V2`), with `weather=storm` | `## thunder.wav` (WuxiaScrub, opengameart.org, CC0) |
| Rain, stopped, outside | `rain_stopped_out.wav`, 26.7 s loop | in rain or a storm, the followed car stopped, a view outside | `rain_tyre_roll` (`TNFS_RAIN_TYRE_ROLL`), with `rain_v2` | `## rain_stopped_out.wav` (Ylmir, opengameart.org, CC0) |
| Rain, stopped, cockpit | `rain_stopped_in.wav`, 22.5 s loop | in rain or a storm, the followed car stopped, the dash view | the same | `## rain_stopped_in.wav` (Nox_Sound, freesound.org, CC0) |
| Wet road | `rain_tyre_roll.wav`, 10.15 s loop | in rain, a storm or snow falling, the followed car moving on asphalt (under the gravel roll on a rally) | the same | `## rain_tyre_roll.wav` (DreamArtSoundDesigners, freesound.org, CC0) |
| Roll on snow | `snow_roll.wav`, 5.3 s loop | on the snowy rally (Alpine 3) only, the followed car moving on the ground | `snow_weather` (`TNFS_SNOW_WEATHER`); no flag of its own | `## snow_roll.wav` (kyles, freesound.org, CC0) |

## Thunder

- **When:** `weather=storm` with `rain_v2`. A clap follows each lightning 0.5 to 4 s later (`weather_thunder_delay`, a pure function of the tick, so a replay sounds the same), at full level for the shortest delay and 35 % for the longest (`weather_thunder_volume`). Inside a closed zone (a tunnel, a gallery) the clap is muffled to 40 % (`weather_thunder_inside`, by the rain's openness).
- **Not when:** no storm, `rain_v2` off, or the snow (Alpine 3 with `snow_weather`: the storm is snow, with no lightning and no thunder).
- **Where in the code:** `storm_thunder_update` (ported_view.c) strikes it, `drive_audio_thunder` (drive_audio.c) mixes it, `game/audio/thunder.c` reads the WAV (the loader the two rolls below reuse).

## Rain

- **When:** `weather=rain` or `storm` with `rain_v2` and `rain_tyre_roll`; the car the camera follows. Three layers (`rain_roll.h`), each its own looping voice:
  - stopped, a view outside: the rain round the car (`rain_stopped_out.wav`) at 45 %;
  - stopped, the dash view: the rain on the roof (`rain_stopped_in.wav`) at 40 %;
  - moving: the wet road (`rain_tyre_roll.wav`). From 2 to 15 m/s the stopped layer gives way to it with an equal-power cross-fade; it then rises from 140 to 280 % at 60 m/s (the voice takes up to 400 %), pitch 800 permille of the sample's rate standing, its own rate at 25 m/s (the sample already sounds like a middle speed) and at most 1100 at 60 m/s. By the view: 3 dB lower in the dash view; in the chase views 0 dB up to the near camera's 4.35 m from the car and -6 dB from the far one's 6.8 m on (straight in dB between, by the eye's real distance), gliding over a quarter of a second on a change of view. On the rally's gravel it is a quarter of that (-12 dB), a layer under the exe's gravel roll: wet gravel, mud.
- **Snow falling** (`snow_weather`): snow is silent, so no stopped layer; it melts on the road, so the wet road plays as in rain, on asphalt and under a rally's gravel roll alike.
- **Not when:** dry or fog; the snowy rally's track (`RALLY_GRAVEL_SNOW`: snow, not water; its own roll below); inside a closed zone (the openness 0 silences all three, which fade in and out at the zone's mouths). The wet road is silent on a shoulder, the grass and in the air; the rain round the car is not.
- **Where in the code:** `rain_roll.[ch]` (the levels), `weather_hooks_rain_roll` (weather_hooks.c, once a frame), `drive_audio_rain_roll` (drive_audio.c), `game/audio/wet_roll.[ch]` (the looping voice).

## Roll on snow

- **When:** the rally on a snowy track only (`RALLY_GRAVEL_SNOW`: Alpine 3's places file says `snow 1`, with `snow_weather`); the followed car on the ground and moving. A drag over snow that sounds like a friction rising with speed: level 0 at 2 m/s to 90 % at 50 m/s, pitch 700 to 1600 permille, steeper than the wet road's (`snow_roll.h`). In the dash view it is 6 dB lower; where snow falls too, the openness scales it as it does the rain.
- **Not when:** standing (under 2 m/s), in the air, inside a closed zone where snow falls, any other rally or track (snow falling there is the rain's wet road). On the snow's rally the exe's gravel sample is no longer struck (race_sound.c), so this is the only roll heard there; the rain's layers are not heard on it.
- **Where in the code:** `snow_roll.[ch]` (the curves), `weather_hooks_snow_roll` (weather_hooks.c, once a frame), `drive_audio_snow_roll` (drive_audio.c, the same looping voice with its own sample).

## The loop voice

The four loops are one mechanism (`game/audio/wet_roll.c`): a looping sample, its level and pitch set by the caller each frame and eased so a change is no click. A caller that stops (a pause, the menus, the race's end) leaves it to fall silent after half a second. It is mixed over the race's mix, in the device's thread and in the offline dump (`TNFS_AUDIO_DUMP=file.wav`, for checking a sound without a device: run the same drive with and without the sample file and compare the two dumps).

## Adding a sound

1. A CC0 or public-domain recording only; read the licence on its page, and say on which day.
2. Trim and loop it (a loop's end cross-faded into its start), down-mix to mono, 22050 Hz, about -18 dBFS RMS with the peak at half scale, into `data/sounds/<name>.wav`.
3. An entry in `LICENSES.md` with *What*, *Made from*, *Source*, *Author*, *Licence* and *Checked on the page on*, in the form of the entries there.
4. Behind a flag that is 0 in Stock and Restored (or behind a kind that already is, as the snow's is); pure functions for its level and pitch, with a test for the curve and for the licence entry (see `tests/test_rain_roll.c`, `tests/test_snow_roll.c`).

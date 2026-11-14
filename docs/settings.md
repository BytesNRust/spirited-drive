# Settings reference: the modes and their files

Every key the port reads from its two files, its default, the value that reproduces the exe, its range, where a menu shows it, its `TNFS_*` variable, and what each profile sets. Read from `game/sys/port_settings.{c,h}`, `game/sys/port_extras.{c,h}`, `game/sys/port_profile.c` (`TABLES`) and the menus in `game/frontend/`, at `origin/combo/c45` (95d989a2). Every key added to the port since (`police_glow`, `radar_all_views`, `tach_dim`, `gauge_wheel`, `replay_no_ffb`, `last_camera`/`camera_kind`, `sky_fill_below`, `chase_height`, `rally_gravel_shoulder`) is in the tables, with its value in each profile; a key that is in no profile column says "not set" (`camera_kind` and the player's own matters below).

`port.cfg` holds the machine's own: `profile=`, `psx_dir=` and `light_edit=` (0..1, 0 by default whatever the mode: the live light editor in a race, `docs/track-light.md`; `TNFS_LIGHT_EDIT` for one run; no menu row; no mode file holds it). A line put in `port.cfg` by hand that is not one of the modes' settings (`light_edit`, or a key the game does not know) is kept when the game writes `port.cfg` and is not taken for an old setting to move. `stock.cfg`, `restored.cfg` and `restomod.cfg` hold what the player changed in that mode, and only that. No file means Stock. All are plain `key=value` lines in the user folder. An unknown key, or a value out of range, is ignored. `config.dat` and `controls.cfg` are shared by the modes.

An older `port.cfg` that still carries settings moves once at startup to the mode's file (`game/sys/port_old_files.c`), with `extras.cfg`. Copies stay as `port.cfg.bak` and `extras.cfg.bak`, and `extras.cfg` goes. A copy already there is kept.

- A named mode keeps its name. Its lines at a table column are not pinned: the old game ignored them, so the table's value stays and a later table reaches the player. What it left alone moves if it is free in the mode.
- `custom`, or no `profile` line, becomes Restomod. Every key the file set stays set in `restomod.cfg`, even at the table's value, and survives later saves. A key the file did not set takes Restomod's value.

## How the pieces fit

- **`TNFS_*` variable.** Where a key has one, the variable wins over the file when it is set (`0`/`1` unless noted). Most are for A/B comparisons.
- **Modes** (`profile=stock|restored|restomod`). A mode starts from its `TABLES` row for the keys in the table below. Its file's value counts only for a key that is free in the mode. Free in all three: `fullscreen`, `explode`, `nitro`, `turbo`, `warrior`, `mirror`, `rally`, `vegas`, `units`, `update_check`, `scale`. Restored adds `format`, `refresh_hz`, `render_bands`, `stats_overlay`. Restomod leaves every key free but `profile` and `psx_dir`.
- **"Exe"** is the value that gives the original game's behaviour (the `Stock` profile, where the key is in it). Where the default is not the exe's, the column says so.
- Many look keys do nothing without another: the ones marked "needs" are inert unless that key is on.

## Settings (`port_settings`)

### Screen and speed

| key | default | exe | range | menu | env | Stock / Restored / Restomod |
|---|---|---|---|---|---|---|
| `format` | `4:3` | `4:3` | `4:3`, `16:9`, `21:9` | Graphics Detail: Screen Format | `TNFS_SCREEN` | 4:3 / the monitor's format / the monitor's format (the table's `widescreen`) |
| `refresh_hz` | 60 | 30 (the exe's 30 steps a second, each drawn once) | 30, 60, 120, 144 | Graphics Detail: Refresh Rate | `TNFS_REFRESH_HZ` | 30 / 144 / 144 |
| `scale` | `fill` | `fill` | `fill`, `integer` | Graphics Detail: Scaling (free in every mode; the window counts real pixels on a retina display, so Whole is 3x/4x there, and spirited-drive.log has its sizes) | none | integer / integer / fill (`scale_integer`) |
| `fullscreen` | 0 | 0 | 0, 1 | Graphics Detail: Full Screen | none | not set |
| `fov_adjust` | 0 | 0 | -10..+20 (degrees added to the vertical field) | Graphics Detail: Field Of View | none | not set |
| `render_scale` | 1 | 1 | 1, 2, 3 (the 3D drawn at that many times the frame's size) | Graphics Detail: Render Scale | `TNFS_RENDER_SCALE` | 1 / 1 / 2 |
| `render_bands` | auto (-1) | 0 (off, the plain path) | `auto` (-1), or 0 (off) .. 8 threads; the menu offers Off, Auto, 2 and 4 | Graphics Detail: Render Threads | `TNFS_BANDS` (wins over the file) | not set |

### View and 3D

| key | default | exe | range | menu | env | Stock / Restored / Restomod |
|---|---|---|---|---|---|---|
| `view_ultra` | 0 | 0 | 0, 1 (the Ultra view distance) | Graphics Detail: View Distance (shows Ultra over Far) | `TNFS_VIEW_ULTRA` | 0 / 0 / 1 |
| `f3_ultra` | 1 | 0 (F3 cycles three distances) | 0, 1 (F3 cycles Far, Medium, Close, Ultra) | none (the F3 key) | `TNFS_F3_ULTRA` | 0 / 0 / 1 |
| `perspective` | 0 | 0 | 0, 1 (perspective-correct, clipped 3D) | Graphics Detail: Perspective Correction | `TNFS_MODERN_3D` (fog weather forces it on) | 0 / 0 / 1 |
| `texture_filter` | 0 | 0 | 0, 1 | Graphics Detail: Texture Filter | `TNFS_TEXTURE_FILTER` | 0 / 0 / 1 |
| `fog` | 0 | 0 | 0, 1 (the port's distance fog) | Graphics Detail: Distance Fog | `TNFS_FOG` | 0 / 0 / 1 |
| `fog_sample` | 0 | 0 | 0, 1 (the fog's colour from the band round the horizon, not the art's last rows) | none | `TNFS_FOG_SAMPLE` (`TNFS_FOG_LIFT` 0..100, dev) | 0 / 0 / 1 |
| `weather` | `clear` | `clear` | `clear`, `fog`, `rain`, `storm` (Restomod only) | Track Select: Weather | `TNFS_WEATHER` | not set |
| `body_shine` | 0 | 0 | 0, 4 (the envmap; 0 the exe's frames; 1..3 in a file or `TNFS_BODY_SHINE` read as 4) | none | `TNFS_BODY_SHINE` | 0 / 0 / 4 |
| `car_colours` | 0 | 0 | 0, 1 (the screen palette's car slots fitted to the colours the cars show; up to 12 palette entries are kept out of the fit for the police lights, the cars' lamps and the night lamps) | none (the CUSTOMIZE "Car Colours" row is extras.cfg's `extra_liveries`) | `TNFS_CAR_COLOURS` | 0 / 0 / 1 |
| `laps` | 0 | 0 (the exe's laps) | 0, 1..99 | none (the track screen's lap count follows it) | `TNFS_LAPS` | not set |
| `records_unified` | 0 | 0 (the original's records: config.dat's tables, every race mixed, the Warrior's kept nowhere; a laps=N race only on a count the track has) | 0, 1 (`race_records.dat`: a table per track, layout (mirror, reverse, rally, all eight), cheats and lap count, every row with its cheats, weather, time of day and difficulty; the best lap shared by every lap count; every race mode, saved after each race; the first run imports config.dat's, `records.dat`'s and `free_records.dat`'s records and leaves those files as they were; the CHEATS ENABLED banner and the results' table line) | none | `TNFS_RECORDS_UNIFIED` | 0 / 0 / 1 |

### Light effects (light, night, sky)

The effects' keys used to be called `psx_*` (`psx_light`, `psx_shadow`, `psx_time`, `psx_flares`, `psx_liveries`, and the variables `TNFS_PSX_*`; `psx_headlights` went with `headlight_beams`, now part of `light_effects`); a file or a variable that still uses one is read as the new name, and a save writes the new one. `psx_dir` is the folder of the PS1 CD's extracted files; the baked light and the second liveries read from it at each race start.

| key | default | exe | range | menu | env | Stock / Restored / Restomod | needs |
|---|---|---|---|---|---|---|---|
| `psx_dir` | empty | empty | a folder path | none (edit the file) | none | not set | |
| `light_edit` | 0 | 0 | 0, 1 (the live light editor in a race: the node, the light file's values, the texture under the pointer; F10 holds the race, F11 copies a line; `docs/track-light.md`). The machine's, in `port.cfg` beside `profile` and `psx_dir`: the same in every mode, no mode file holds it | none (edit `port.cfg`) | `TNFS_LIGHT_EDIT` | 0 in every mode | |
| `light_effects` (older files: `psx_light`) | 0 | 0 | 0, 1 (with it, at night, the night's second look: the road dark and lit by a pool ahead of each car, the headlight beams; the older `headlight_beams` / `psx_headlights` key is ignored) | Graphics Detail: Light Effects (switches `light_effects` and `car_soft_shadow` together) | `TNFS_LIGHT_EFFECTS` (`TNFS_PSX_LIGHT` is read too) | 0 / 0 / 1 | |
| `car_soft_shadow` (older files: `psx_shadow`) | 0 | 0 | 0, 1 | same row as `light_effects` | `TNFS_CAR_SOFT_SHADOW` (`TNFS_PSX_SHADOW` is read too) | 0 / 0 / 1 | |
| `light_time` (older files: `psx_time`) | `day` | `day` | `dawn`, `day`, `dusk` (`a`, `b`, `c` also read) | Track Select: time of day, with the baked light on | `TNFS_LIGHT_TIME` (`TNFS_PSX_TIME` is read too) | not set | `light_effects` |
| `lamp_flares` (older files: `psx_flares`) | 0 | 0 | 0, 1 (a soft halo round lit brake lights and tunnel lamps in low light and fog) | none | `TNFS_LAMP_FLARES` (`TNFS_PSX_FLARES` is read too) | 0 / 0 / 1 | `light_effects` |
| `police_glow` | 0 | 0 | 0, 1 (a near-white core in the police lights, a stronger halo in poor light) | none | `TNFS_POLICE_GLOW` | 0 / 0 / 1 | `lamp_flares` for the halo |
| `sky_fill` | 0 | 0 | 0, 1 (the flat sky's art carried up to the view's top) | none | `TNFS_SKY_FILL` (`TNFS_SKY_FILL_MODE` a/b, dev) | 0 / 0 / 1 | |
| `sky_fill_below` | 0 | 0 | 0, 1 (the same carried down to the view's bottom; 16:9 and 21:9 only, never 4:3) | none | `TNFS_SKY_FILL_BELOW` | 0 / 0 / 1 | |
| `rally_gravel_shoulder` | 0 | 0 | 0, 1 (the rolling gravel scrubs with the shoulder's own tyre sound, by speed, no gravel strikes) | none | `TNFS_RALLY_GRAVEL_SHOULDER` | 0 / 0 / 1 | extras `rally_gravel` |
| `brake_over_gas` | 0 | 0 | 0, 1 (a pad's brake past its dead zone beats the gas: the car brakes and the brake light lights; 0 sums them as the exe does) | none | `TNFS_BRAKE_OVER_GAS` | 0 / 0 / 1 | none |
| `sign_glow` | 0 | 0 | 0, 1 (at night, with `light_effects` (the headlight beams are part of it), the City's matrix sign, EXPRESS LANES CLOSED (CY1), has its amber bulbs lit like the windows; 0 the exe's) | none | `TNFS_SIGN_GLOW` | 0 / 0 / 1 | `headlight_beams`, CY1 |
| `analog_pad` | 0 | 0 | 0, 1 (a control bound to an axis (pad stick or trigger, wheel, pedals) steers and works the pedals in proportion, a key or button keeps full travel; 0 the exe's key paths) | none | `TNFS_ANALOG_PAD` | 1 / 1 / 1 | a game controller |
| `rain_v2` | 0 | 0 | 0, 1 (with `weather=rain` the rain has half the streaks, with `weather=storm` it has the old rain's count plus a quarter, with thunder after each lightning (0.5 to 4 s later, quieter the later; the sample is `data/sounds/thunder.wav`, CC0 by WuxiaScrub, see `data/sounds/LICENSES.md`), the streaks fall from higher, the wet road's lamp reflections show on the carriageway only, and the traffic's and police's spray wisp is a little longer and never gone at low speed; 0 as before) | none | `TNFS_RAIN_V2` | 0 / 0 / 1 | `weather=rain` or `storm`, `rain_fx` |
| `rain_tyre_roll` | 0 | 0 | 0, 1 (in rain or a storm with `rain_v2`: the rain round the followed car outside or on its roof in the dash view, stopped and moving, cross-faded by its speed; every car's tyre spray where the car is (the four loudest), its level and pitch rising with speed, lower the farther and in the dash view, under the gravel roll on a rally, with puddle splashes at random, more the faster; snow falling: the sprays and splashes alone; none on the snowy rally's track; silent inside a closed zone, fading at the zone's mouths (docs/sounds.md); 0 none) | none | `TNFS_RAIN_TYRE_ROLL` | 0 / 0 / 1 | Restomod '26's rain |
| `snow_weather` | 0 | 0 | 0, 1 (on a track whose places file says `snow 1`, Alpine 3, rain is snow: white flakes falling slowly and drifting, with the weather's openness; the storm is heavier snow with no lightning or thunder; the road stays wet as in rain; in the rally the road and the shoulder are covered in snow with a loose, slidy grip (rally_gravel.h); 0 the rain as it was) | none | `TNFS_SNOW_WEATHER` | 0 / 0 / 1 | Restomod '26's Alpine |
| `traffic_halo` | 0 | 0 | 0..1 (at night the traffic's lit lamps get a halo as the racers' do; 0 as now) | none | `TNFS_TRAFFIC_HALO` | 0 / 0 / 1 | `lamp_flares`, night |
| `rally_dry_lamps` | 0 | 0 | 0..1 (in a rally (dirt) race in rain the cars' lamps are not reflected in the road, as dirt is no mirror; 0 the exe's wet-asphalt reflections) | none | `TNFS_RALLY_DRY_LAMPS` | 0 / 0 / 1 | `weather=rain`, `rain_fx`, a rally race |
| `rain_fx` | 0 | 0 | 0, 1 (rain streaks by speed and light) | none | `TNFS_RAIN_FX` | 0 / 0 / 1 | `weather=rain` |
| `rain_grip` | 0 | 0 | 0, 1 (with `weather=rain` the road grips at 4/5 and the race's times and records are not kept, provisionally; the shoulder keeps its grip) | none | `TNFS_RAIN_GRIP` | 0 / 0 / 1 | `weather=rain` |
| `gauge_light` | 0 | 0 | 0..5 (at night the lit parts of the dash gauges take a colour: 1 greenish, 2 reddish, 3 orange, 4 bluish, 5 "Del coche": the colour painted at each pixel in the dash's `dash_color.png` (below), orange where none or the car has no file; 0 the exe's untinted white; Graphics options page; a hand-made mask per dash, `data/dash_masks/<CAR>/dash_mask.png` or `$TNFS_DASH_MASKS`, from `tools/dash_mask_from_psd.py`, marks the lit parts: blue the dash, green a dial, yellow ffff00 or white always lit. The optional PSD layer "color" gives `data/dash_masks/<CAR>/dash_color.png` (same tool, `$TNFS_DASH_MASKS` too): paint the lit parts in the colour each is to glow, a different one per dial if wanted; only the strongest channel sets the hue, the dash's art the brightness; transparent or black = no colour (orange); up to 12 distinct colours; no menu row: the player does not pick it, the car's own colours do; Restomod defaults to 5) | none | `TNFS_GAUGE_LIGHT` | 0 / 0 / 5 | `weather=rain` |
| `finish_gate_light` | 0 | 0 | 0, 1 (at night, with `light_effects` (the headlight beams are part of it), the stage tracks' gate that ends a stage (CY, CL, AL; CHECKPOINT, FINISH on the third stage) keeps its daylight light against the dark; 0 the exe's) | none | `TNFS_FINISH_GATE_LIGHT` | 0 / 0 / 1 | `headlight_beams`, CY/CL/AL |
| `reverse_rows` | 0 | 0 | 0, 1 (a reversed circuit's rows are read the way the forward circuit's are (the exe's reverse swaps the two chains of each row, which bends a tunnel's roof into a slab and lays its wall down as a hillside): the tunnels keep their walls and roof; 0 the rows swapped as the exe's reverse has them) | none | `TNFS_REVERSE_ROWS` | 0 / 0 / 1 | none |
| `city_lift` | 0 | 0 | 0, 1 (with the baked light and headlights the City nights CY1..CY3 are lifted: a ceiling of .42 over the exe's .35, a little of the dark floor raised, brighter tunnels and lamp pools, the beams standing out on top; an older file's 2 or 3 reads as 1; 0 the exe's darkness) | none | `TNFS_CITY_LIFT` | 0 / 0 / 1 | `light_effects` |
| `vegas_lift` | 0 | 0 | 0, 1 (the same for Lost Vegas' night: a ceiling of .80 and its dark floor raised; an older file's 2 reads as 1; 0 the exe's) | none | `TNFS_VEGAS_LIFT` | 0 / 0 / 1 | `light_effects` |
| `rain_lamps` | 0 | 0 | 0, 1 (with `weather=rain` or `storm` every car drives with its headlights on, day included: lit front lamps and their warm reflections on the wet road; no night beam or pools by day) | none | `TNFS_RAIN_LAMPS` | 0 / 0 / 1 | `weather=rain` |
| `gauge_exterior` | 0 | 0 | 0, 1 (with `gauge_light`, the gauges are lit whenever the exterior lights are on: night, dusk, and rain or storm with `rain_lamps`; by day dry they stay unlit; 0 at night only) | none | `TNFS_GAUGE_EXTERIOR` | 0 / 0 / 1 | `gauge_light` |
| `close_houses` | 0 | 0 | 0, 1 (the TR3 village houses get their right wall, back wall and back roof slope, the long building its far end cap; 0 they are open as in the exe) | none | `TNFS_CLOSE_HOUSES` | 0 / 0 / 1 | none |
| `reflectors` | 0 | 0 | 0, 1 (at night or in a dark tunnel, with the headlight beams, the texels a track mask marks reflective shine in the player's lamp cone out to 200 m (reflector.h); 0 the exe's) | none | `TNFS_REFLECTORS` | 0 / 0 / 1 | `headlight_beams` |
| `handbrake_lamps` | 0 | 0 | 0, 1 (a brake that is the handbrake's alone does not light the brake lamps: in the rally the exe turns the handbrake into a full brake, car_physics.c, and the lamps showed it; the physics are the exe's either way; 0 the exe's) | none | `TNFS_HANDBRAKE_LAMPS` | 0 / 0 / 1 |
| `rally_spray` | 0 | 0 | 0, 1 (in a rally with rain_fx, in rain or snow, every car, the player's too, throws the rain spray tinted by the road under it: grey on gravel, brown on dirt, white on snow; the dust is not drawn over it; 0 as before) | none | `TNFS_RALLY_SPRAY` | 0 / 0 / 1 | none |
| `snow_fx` | 0 | 0 | 0, 1 (with snow_weather, the falling snow: soft round flakes, a little less regular, leaning away from the vanishing point with the car's speed at a smaller angle than the rain's streaks, the snowstorm's wind swinging from one side to the other over about a minute, and whiter, rarer marks on the lens; 0 as before) | none | `TNFS_SNOW_FX` | 0 / 0 / 1 | none |
| `lamp_halos` | 0 | 0 | 0, 1 (at night, the street lamps get the same soft dithered halo as the brake lights and the tunnel lamps: found as the blobs of the lamp boards' emissive entries after the 3D, only where a lamp's own sprite was drawn, the nearest or largest few; the lit windows and the traffic signs the reflectors light never; 0 as before) | none | `TNFS_LAMP_HALOS` | 0 / 0 / 1 | none |
0 | none | `TNFS_LAMP_HALOS` | 0 / 0 / 0 | none |
| `wipers` | 0 | 0 | 0, 1 (in the dash view, in rain, storm and snow, two wiper blades sweep the glass, at a pace that follows what falls (rain and snow 1.8 s a cycle, the storm 1.3 s), and park when the cycle is done; each pass clears the lens drops and the snow's marks it crosses; a dash whose art draws wiper blades (the P911's, a red `dash_mask.png` colour) shows them only when the wipers are off; needs the wide path (a wide format or render_scale above 1); 0 as before) | none | `TNFS_WIPERS` | 0 / 0 / 1 | none |
| `weather_fog` | 0 | 0 | 0, 1 (the weather's fog in snow and in the rain storm, with Modern 3D: 1 the snow's fog is whiter and denser (a snow-white colour, closer, with the fog weather's exponential curve, denser in the snow storm), the rain storm's fog is denser than the rain's, and the rain's dark grey mood is not laid over the snow, so the picture is whiter; an older file's 2 reads as 1; 0 the same fog as the rain in all four) | none | `TNFS_WEATHER_FOG` | 0 / 0 / 1 | `modern_3d` |
| `spray_v4` | 0 | 0 | 0, 1 (the wheels' spray and dust as the generated sprites of each material, born every simulation tick at the wheel's own place and height: on a wet road small drops thrown back and up in a fan that fall under gravity and vanish where they touch the road, and a low flat mist; the desert's dust a slow cottony cloud; the snow's powder and small lumps; gravel and the shoulder small angular stones with a fine dust; replaces rain_fx's spray and the rally's dust; 0 as before) | none | `TNFS_SPRAY_V4` | 0 / 0 / 1 | `weather=rain`, or a rally's dust, gravel or snow, or the shoulder |
| `menu_pad_cancel` | 0 | 0 | 0..1 (the gamepad's second button, B by default (the exe's Enter), is Escape in the menus and the name box: back, exit, cancel; the first, A, keeps accepting (Space); 0 the exe's: both accept) | none | `TNFS_MENU_PAD_CANCEL` | 0 / 0 / 1 | a game controller |
| `tunnel_lamps` | 0 | 0 | 0, 1 (by day and at night; an older file's 2 reads as 1: the lamps of the tunnels and caverns (the lamp textures of `game/race/tunnel_lamp_textures.c`: CY1, CY2, CY3, TR3, TR4) light the walls and the road in a pool under each lamp, like the street lamps' pools; the light is ADDED over the tunnel's own (`tunnel_tone`), so a dark tunnel still has bright pools under its lamps; the car takes them too: its light in the tunnel is the between-lamps level everywhere and each lamp's pool lifts it to full light by its distance to the lamp, in place of the node-parity alternation; 0 the exe's) | none | `TNFS_TUNNEL_LAMPS` | 0 / 0 / 1 | `light_effects`, `headlight_beams`, night |
| `wrong_way` | 0 | 0 | 0..1 (a centred blinking WRONG WAY, FALSCHE RICHTUNG with `--german`, in the upper third when the player's car drives forwards (not in reverse, forward speed above 30 km/h) with its nose more than a quarter turn from the road's heading at its node, not spinning, for 1.5 s; it goes after 0.5 s of anything else; reversing never turns it on, whichever way the nose points; not in the countdown, after the finish or in a replay; 0 the exe's: no warning) | none | `TNFS_WRONG_WAY` | 0 / 0 / 1 | a race |

### Dash, cameras and replay

| key | default | exe | range | menu | env | Stock / Restored / Restomod |
|---|---|---|---|---|---|---|
| `radar_all_views` | 0 | 0 | 0, 1 (the police radar shows in every view outside the car) | EXTRAS page 2: the radar row, labelled "Police radar" when on | `TNFS_RADAR_ALL_VIEWS` | 0 / 0 / 1 |
| `tach_dim` | 0 | 0 | 0, 1 (the outside views' tachometer takes the dash's half tint) | none | `TNFS_TACH_DIM` | 0 / 0 / 1 |
| `gauge_wheel` | 0 | 0 | 0, 1 (at night the wheel in front of the gauges takes the dash's half tint) | none | `TNFS_GAUGE_WHEEL` | 0 / 0 / 1 |
| `replay_no_ffb` | 0 | 0 | 0, 1 (a replay leaves the force-feedback stick and the pad's rumble still) | none | `TNFS_REPLAY_NO_FFB` | 0 / 0 / 1 |
| `last_camera` | 0 | 0 (a race starts in the dash) | 0, 1 (a race starts in the last camera used) | none | `TNFS_LAST_CAMERA` | 0 / 0 / 1 |
| `camera_kind` | -1 | -1 | -1 (none yet), 0..5 | none; the game writes it, read only with `last_camera` | none | not set |

### Profile

| key | default | exe | range | menu | env | note |
|---|---|---|---|---|---|---|
| `profile` | `stock` (no line) | `stock` | `stock`, `restored`, `restomod` | Game Mode: Profile | none | the table below |

### Settings Restomod leaves with no visible effect (an open question)

Found by reading the code from each setting to what it gates (2026-11-11; a capture in the scene the code needs, on and off, agrees). In Restomod these eight change nothing the player sees or hears, because another setting that Restomod turns on takes over. They are not removed: each does something when the other setting is changed, and whether to keep, hide or drop them is the owner's decision, still open.

| Setting | Restomod | Taken over by | Where it shows again |
|---------|----------|---------------|----------------------|
| `rally_spray` | 1 | `spray_v4` 1 | `spray_v4=0`, a rally road in rain, rivals in view |
| `gauge_wheel` | 1 | `gauge_light` above 0 (Restomod 5): the dash's mask decides alone and already treats the wheel | `gauge_light=0`, a cockpit view at night: about 40 pixels at the wheel's hub |

Two more of the 34 settings checked are not in this table because they are not overridden but easy to miss: `rain_tyre_roll` and `rally_gravel_shoulder` only make a sound, so no picture shows them. The other 24 act only in one scene each (a track, a stretch of nodes, a weather).

## Switches (`port_extras`)

Switches are `0`/`1`. Defaults are from `port_extras_defaults`.

### Menu rows (EXTRAS)

| key | default | exe | menu | note | Stock / Restored / Restomod |
|---|---|---|---|---|---|
| `explode`, `nitro`, `turbo` | 0 | 0 | page 1 Cheats: Explode / Nitro / Turbo mode | a race run with them is filed apart from the normal records | not set |
| `warrior` | 0 | 0 | page 1: Warrior Player Car | forced on by Unlock Level 1 or more, as the exe does | not set |
| `warrior_rival` | 0 | 0 | page 1: Warrior Rival Car | | not set |
| `mirror` | 0 | 0 | page 1: Mirror track mode | | not set |
| `rally` | 0 | 0 | page 1: Rally track mode | | not set |
| `vegas` | 0 | 0 | page 1: Lost Vegas Track | | not set |
| `lunar_springs` | 0 | 0 | page 1: Lunar Springs Track | | not set |
| `lunar_gravity` | 0 | 0 | page 1: Lunar Gravity | a third of the pull, two thirds of the grip, on the moon | not set |
| `variant_time` | 0 | 0 | none | the time of day offered with Mirror or Rally; set by the mode (Restomod only) | not set |
| `extra_liveries` (older files: `psx_liveries`) | 0 | 0 | page 1: Car Colours | catalogue paints for every car, the console version second livery with `psx_dir`; the player's pick is `car_paint` | 0 / 0 / 1 |
| `units` | `mph` | `mph` | page 2: Speed units | `mph`, `kmh`; an old `language` reads as `mph` | not set |
| `no_assists` | 0 | 0 | page 2: No driving assists | | not set |
| `bumper_mirror` | 0 | 0 | page 2: Bumper view mirror | rear-view mirror in the bumper view; F1 in a race; `TNFS_BUMPER_MIRROR` | 0 / 0 / 1 |
| `bumper_radar` | 0 | 0 | page 2: Bumper view police radar ("Police radar" with `radar_all_views`) | radar detector; F2 in a race; `TNFS_BUMPER_RADAR` | 0 / 0 / 1 |
| `replay_cameras` | 0 | 0 | page 2: Replay cameras in race mode | | not set |
| `car_markers` | 1 | 1 | page 2: Rival car markers | F6 in a race writes it back | not set |
| `stats_overlay` | 0 | 0 | page 2: 3D render stats | F12 in a race writes it back | not set |
| `dash_box` | `map` | `map` | page 2: Dash box | `map`, `lever`, `hidden` | not set |
| `follow_camera` | 0 | 0 | page 2: 360 Camera | every camera follows the car's heading, the road behind drawn | 0 / 0 / 1 |
| `difficulty` | 1 | 1 | page 2: Difficulty | 0 Easy, 1 Original, 2 Hard (rivals, police and traffic together) | not set |
| `start` | `game_mode` | `game_mode` | page 3: Start Screen | `game_mode`, `network`, `central` | not set |
| `no_movies` | 0 | 0 | page 3: Intro video (inverted) | | not set |
| `update_check` | 1 | not applicable | page 3: Updates and anonymous stats | the update check and the three anonymous counters; each sends the version and the system (macOS, Windows, Linux), the API works out the country from the connection and keeps only daily totals | not set |

### Keys with no EXTRAS row

| key | default | exe | where it is set | note | Stock / Restored / Restomod |
|---|---|---|---|---|---|
| `reverse` | 0 | 0 | Track Select: Reverse (offered when this is on); `TNFS_REVERSE=0\|1` overrides | circuits run the other way round; the tunnel/LOD byte and the models are handled in `tri_reverse` | not set |
| `rally_gravel` | 0 | 0 | none | gravel noise all along the rally and dust from the desert's tyres | 0 / 0 / 1 |
| `movies_wide` | 1 | 0 (the 4:3 frame in the middle) | none | on a wide screen a movie fills the width, top and bottom cropped | 0 / 1 / 1 |
| `car_paint` | all 0 | all 0 | the car showcase's Color box | one hex digit a car (cars 0..11): 0 own, 1 console livery, 2+ the catalogue's paints | not set |

## What each profile sets (`TABLES`)

All 60 columns of the table. A column that is not here is not touched by any profile. "group" is the section above the key is in.

| column | group | Stock | Restored | Restomod |
|---|---|---|---|---|
| `widescreen` (sets `format`) | settings | 0 (4:3) | 1 (monitor's) | 1 (monitor's) |
| `refresh_hz` | settings | 30 | 144 | 144 |
| `view_ultra` | settings | 0 | 0 | 1 |
| `perspective` | settings | 0 | 0 | 1 |
| `texture_filter` | settings | 0 | 0 | 1 |
| `fog` | settings | 0 | 0 | 1 |
| `f3_ultra` | settings | 0 | 0 | 1 |
| `render_scale` | settings | 1 | 1 | 2 |
| `light_effects` | settings | 0 | 0 | 1 |
| `car_soft_shadow` | settings | 0 | 0 | 1 |
| `lamp_flares` | settings | 0 | 0 | 1 |
| `body_shine` | settings | 0 | 0 | 4 |
| `car_colours` | settings | 0 | 0 | 1 |
| `fog_sample` | settings | 0 | 0 | 1 |
| `sky_fill` | settings | 0 | 0 | 1 |
| `movies_wide` | switches | 0 | 1 | 1 |
| `extra_liveries` | switches | 0 | 0 | 1 |
| `rally_gravel` | switches | 0 | 0 | 1 |
| `bumper_mirror` | switches | 0 | 0 | 1 |
| `bumper_radar` | switches | 0 | 0 | 1 |
| `follow_camera` | switches | 0 | 0 | 1 |
| `scale_integer` (`scale=integer`) | settings | 1 | 1 | 0 |
| `last_camera` | settings | 0 | 0 | 1 |
| `replay_no_ffb` | settings | 0 | 0 | 1 |
| `sky_fill_below` | settings | 0 | 0 | 1 |
| `radar_all_views` | settings | 0 | 0 | 1 |
| `tach_dim` | settings | 0 | 0 | 1 |
| `gauge_wheel` | settings | 0 | 0 | 1 |
| `police_glow` | settings | 0 | 0 | 1 |
| `rally_gravel_shoulder` | settings | 0 | 0 | 1 |
| `close_houses` | settings | 0 | 0 | 1 |
| `rain_fx` | settings | 0 | 0 | 1 |
| `rain_grip` | settings | 0 | 0 | 1 |
| `gauge_light` | settings | 0 | 0 | 5 |
| `finish_gate_light` | settings | 0 | 0 | 1 |
| `sign_glow` | settings | 0 | 0 | 1 |
| `analog_pad` | settings | 1 | 1 | 1 |
| `rain_v2` | settings | 0 | 0 | 1 |
| `rain_tyre_roll` | settings | 0 | 0 | 1 |
| `snow_weather` | settings | 0 | 0 | 1 |
| `traffic_halo` | settings | 0 | 0 | 1 |
| `rally_dry_lamps` | settings | 0 | 0 | 1 |
| `reverse_rows` | settings | 0 | 0 | 1 |
| `city_lift` | settings | 0 | 0 | 1 |
| `vegas_lift` | settings | 0 | 0 | 1 |
| `rain_lamps` | settings | 0 | 0 | 1 |
| `gauge_exterior` | settings | 0 | 0 | 1 |
| `brake_over_gas` | settings | 0 | 0 | 1 |
| `reflectors` | settings | 0 | 0 | 1 |
| `handbrake_lamps` | settings | 0 | 0 | 1 |
| `rally_spray` | settings | 0 | 0 | 1 |
| `snow_fx` | settings | 0 | 0 | 1 |
| `lamp_halos` | settings | 0 | 0 | 1 |
| `wipers` | settings | 0 | 0 | 1 |
| `weather_fog` | settings | 0 | 0 | 1 |
| `spray_v4` | settings | 0 | 0 | 1 |
| `menu_pad_cancel` | settings | 0 | 0 | 1 |
| `tunnel_lamps` | settings | 0 | 0 | 1 |
| `wrong_way` | settings | 0 | 0 | 1 |
| `records_unified` | settings | 0 | 0 | 1 |

The profile does not set: `fullscreen`, `fov_adjust`, `laps`, `psx_dir`, `light_time`, `weather`, `render_bands`, `camera_kind`, and extras' `mirror`, `rally`, `warrior`, `vegas`, the cheats, `lunar_*`, `reverse`, `units`, `difficulty`, `start`, `no_movies`, `no_assists`, `replay_cameras`, `car_markers`, `stats_overlay`, `dash_box`, `variant_time`, `car_paint`, `update_check`.

## Development-only variables

Not a setting: a way to drive the headless runs and the checks. They are not stable and none is written anywhere. Headless runs go with the dummy SDL drivers, `timeout -k`, and `--user <scratch>` (the player's folder: `--mods` no longer holds it).

- **Place and time:** `TNFS_START_NODE=N` (start the player on node N), `TNFS_START_SIDE=M` (the car M metres across the road from its centre line, negative to the left), `TNFS_START_TURN=1|2` (the start car turned half a turn, or a quarter across the road), `TNFS_RIVAL_BEHIND=N` (a `--drive` with a rival: the first rival put N nodes behind the player, active; negative, ahead), `TNFS_DRIVE_THEN=NAME` (a second track raced in the same process after the `--drive` one, as the menus do after a race), `TNFS_FRAMES=N`, `TNFS_FRAME_SKIP=n` (draws one tick in n of the last `TNFS_FRAMES`, the last one always, as a slow machine does), `TNFS_SHOTS=prefix:every` (screenshots during a run), `TNFS_MIDDAY=1`, `TNFS_DRIVE_TIME`, `TNFS_RUN`, `TNFS_RALLY=1`, `TNFS_RALLY_ART=1` (the headless `--drive` with the rally's art, as the menus' rally has it; `TNFS_RALLY` is its physics), `TNFS_OPPONENT=<car>`, `TNFS_REPLAY_REQUEST`.
- **Cars and cameras:** `TNFS_FOLLOW_CAR=<slot>` (the chase follows that car, 2 is the police), `TNFS_POLICE_NEAR=N` (the police N nodes behind the player, chasing; race type 1), `TNFS_VIEW`, `TNFS_VIEW_AT=tick:kind` (the chase camera goes to that kind at that tick, as a view key would), `TNFS_LOOK_BACK_AT=from:to`, `TNFS_STEER=left|right`, `TNFS_SCRIPT=spin`, `TNFS_CAR=NAME` (`--track` stand-in).
- **Input scripts:** `TNFS_INPUT_SCRIPT="ms:KEY,..."`, `TNFS_INPUT_SETTLE`, `TNFS_F3_AT`, `TNFS_REPLAY_BUTTONS`, `TNFS_REPLAY_AT=frame` and `TNFS_REPLAY_BACK=frame` (headless F9 pressed once on that frame), `TNFS_FAKE_PAD`, `TNFS_AUTOPILOT=1`.
- **The headless menu walk:** `tools/menu_walk.sh <bin> <outdir> <target>` walks the front end to a screen and saves its picture as `<outdir>/<target>.png` (targets: `options`, `controls`, `controls-p2`, `controls-p3`, `graphics`, `extras`, `extras-p2`, `extras-p3`). Every step waits for the `mark:screen=NAME` line the front end logs as a screen opens (`WAIT`, with `TNFS_INPUT_SETTLE`), not for a time. A `WAIT`'s watch is armed when the event before it begins, so the mark that event's reaction logs counts (it used to be armed a frame too late, which lost the mark of a fast screen). A step that does not come stops the run with the step named, exit 3. The marks: `intro`, `game_mode`, `name_box`, `central`, `options`, `controls_p1` (the exe's page, logged on opening Controls setup), `controls_keyboard`, `controls_actions`, `controls_tune`, `graphics`, `extras_p1`, `extras_p2`, `extras_p3`. `tools/menu_compare.sh <binA> <binB> <outdir>` walks every target with both builds (2 walks at a time, about 6 minutes) and prints per target `same` or `differs N px` with a diff image in `<outdir>/diff/` (exit 1 if any differs, 3 if a walk failed), to catch an unintended change of the menus' art. The walk assumes the three-page Controls setup (Keyboard, Game controller, Tune) and the default 4:3 test config it writes.
- **Capture and dumps:** `TNFS_CAPTURE_DIR` with `TNFS_CAPTURE_EVERY` and `TNFS_CAPTURE_FROM` (every presented frame: set the interval, a capture of every frame fills the disk), `TNFS_AUDIO_DUMP=file.wav`, `TNFS_SCENERY_DUMP`, `TNFS_SCENERY_DUMP_NIGHT`, `TNFS_SLOT_DUMP`.
- **Tests:** `TNFS_FORCE_SPLIT=1` keeps the split present on when the frame's palettes are all the same (drive_light_needs_split), the picture the plain present must equal (tests/day_plain_present.cmake).
- **Traces:** `TNFS_VERBOSE=1` (or `--verbose`), `TNFS_TRACE_FAM`, `TNFS_TRACE_PLAYER=N`, `TNFS_FRAME_TIMES=1`, `TNFS_NOTICE=1|2`, `TNFS_CLASS_CACHE_CHECK=1` (window run: every 2D-layer class row the frame cache keeps is made again and compared; a difference prints `class cache: row N kept but its classes changed`).
- **A/B overrides of automatic behaviour:** `TNFS_TWO_SIDED`, `TNFS_CLOSE_BOXES` (both automatic with a reversed track or the 360 camera), `TNFS_CAR_DRAW_DISTANCE=0` (the exe's distances at render scale 2 or 3), `TNFS_WIDE_BOARD=0`, `TNFS_EMISSIVE_LAMPS`, `TNFS_CELL_ON=1` (the headlight cone's grid of cells on a lit road polygon, off by default as it costs 3..8 % of a frame at night), `TNFS_FB_RELIGHT_OFF=1` (the road drawn behind the camera is not relit by the headlight cones at each point, for before / after), `TNFS_FOG_CURVE=line`, `TNFS_FOG_LIFT`, `TNFS_SKY_FILL_MODE`, `TNFS_STAND_IN`.
- **Data and folders:** `TNFS_PREF_DIR` (the player's folder, for tests and modders; `--user DIR` does it for one run), `TNFS_CAR_ENVMAP`, `TNFS_CAR_MASKS`.
- **Captures of the light and the rain (read where they act, kept for before / after pictures):** `TNFS_CITY_BEAM_OPEN=n` (1..255: the open-ground light of the headlight beam in the City nights, in place of the per-track value, `city_lift 3`), `TNFS_CITY_MADE_SCALE=n` (1..256: the share of the terrain's light the City's made night rows take, in place of the per-track fit), `TNFS_LAMP_CAR_MODE=2` (the car's lift under the street lamps is steady along the lamps' stretch, instead of following each pool), `TNFS_RAIN_SPEED=n` (metres a second the rain's streaks are drawn for, for a capture of a car the harness has not got up to speed), `TNFS_RAIN_WET_OFF=1` (leaves out the rain's grey scene and the tail lights' reflections), `TNFS_LAMP_HUE=0` (the tunnel lamps' pools matched to the palette by the plain nearest colour, as before i193: their edges went mauve).
- **Development builds only:** `TNFS_STATS_DRYRUN=<version>`, `TNFS_UPDATE_FAKE=9.9.9`; a release build ignores them.

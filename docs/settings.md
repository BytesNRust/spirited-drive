# Settings reference: the modes and their files

Every key the port reads from its two files, its default, the value that reproduces the exe, its range, where a menu shows it, its `TNFS_*` variable, and what each profile sets. Read from `game/sys/port_settings.{c,h}`, `game/sys/port_extras.{c,h}`, `game/sys/port_profile.c` (`TABLES`) and the menus in `game/frontend/`, at `origin/combo/c45` (95d989a2). Every key added to the port since (`police_glow`, `radar_all_views`, `tach_dim`, `gauge_wheel`, `replay_no_ffb`, `last_camera`/`camera_kind`, `sky_fill_below`, `chase_height`, `lamp_pool_tint`, `trans_shadows`, `car_tunnel_light`, `vegas_car_night`, `rally_gravel_shoulder`) is in the tables, with its value in each profile; a key that is in no profile column says "not set" (`camera_kind` and the player's own matters below).

`port.cfg` holds only `profile=` and `psx_dir=`. `stock.cfg`, `restored.cfg` and `restomod.cfg` hold what the player changed in that mode, and only that. No file means Stock. All are plain `key=value` lines in the user folder. An unknown key, or a value out of range, is ignored. `config.dat` and `controls.cfg` are shared by the modes.

An older `port.cfg` that still carries settings moves once at startup to the mode's file (`game/sys/port_old_files.c`), with `extras.cfg`. Copies stay as `port.cfg.bak` and `extras.cfg.bak`, and `extras.cfg` goes. A copy already there is kept.

- A named mode keeps its name. Its lines at a table column are not pinned: the old game ignored them, so the table's value stays and a later table reaches the player. What it left alone moves if it is free in the mode.
- `custom`, or no `profile` line, becomes Restomod. Every key the file set stays set in `restomod.cfg`, even at the table's value, and survives later saves. A key the file did not set takes Restomod's value.

## How the pieces fit

- **`TNFS_*` variable.** Where a key has one, the variable wins over the file when it is set (`0`/`1` unless noted). Most are for A/B comparisons.
- **Modes** (`profile=stock|restored|restomod`). A mode starts from its `TABLES` row for the keys in the table below. Its file's value counts only for a key that is free in the mode. Free in all three: `fullscreen`, `explode`, `nitro`, `turbo`, `warrior`, `mirror`, `rally`, `vegas`, `units`, `update_check`. Restored adds `format`, `refresh_hz`, `scale`, `render_bands`, `stats_overlay`. Restomod leaves every key free but `profile` and `psx_dir`.
- **"Exe"** is the value that gives the original game's behaviour (the `Stock` profile, where the key is in it). Where the default is not the exe's, the column says so.
- Many look keys do nothing without another: the ones marked "needs" are inert unless that key is on.

## Settings (`port_settings`)

### Screen and speed

| key | default | exe | range | menu | env | Stock / Restored / Restomod |
|---|---|---|---|---|---|---|
| `format` | `4:3` | `4:3` | `4:3`, `16:9`, `21:9` | Graphics Detail: Screen Format | `TNFS_SCREEN` | 4:3 / the monitor's format / the monitor's format (the table's `widescreen`) |
| `refresh_hz` | 60 | 30 (the exe's 30 steps a second, each drawn once) | 30, 60, 120, 144 | Graphics Detail: Refresh Rate | `TNFS_REFRESH_HZ` | 30 / 144 / 144 |
| `scale` | `fill` | `fill` | `fill`, `integer` | Graphics Detail: Scaling | none | integer / integer / fill (`scale_integer`) |
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
| `body_shine` | 0 | 0 | 0..4 (1 subtle, 2 default look, 3 strong, 4 the envmap; 0 the exe's frames) | none | `TNFS_BODY_SHINE` | 0 / 0 / 4 |
| `car_colours` | 0 | 0 | 0, 1 (the screen palette's car slots fitted to the colours the cars show; up to 12 palette entries are kept out of the fit for the police lights, the cars' lamps and the night lamps) | none (the CUSTOMIZE "Car Colours" row is extras.cfg's `psx_liveries`) | `TNFS_CAR_COLOURS` | 0 / 0 / 1 |
| `laps` | 0 | 0 (the exe's laps) | 0, 1..99 | none (the track screen's lap count follows it) | `TNFS_LAPS` | not set |
| `free_records` | 0 | 0 (no table for a free lap count) | 0, 1 | none | `TNFS_FREE_RECORDS` | not set |

### PS1 look (light, night, sky)

`psx_dir` is the folder of the PS1 CD's extracted files; the PS1 light and the second liveries read from it at each race start.

| key | default | exe | range | menu | env | Stock / Restored / Restomod | needs |
|---|---|---|---|---|---|---|---|
| `psx_dir` | empty | empty | a folder path | none (edit the file) | none | not set | |
| `psx_light` | 0 | 0 | 0, 1 | Graphics Detail: Light Effects (switches `psx_light` and `psx_shadow` together) | `TNFS_PSX_LIGHT` | 0 / 0 / 1 | |
| `psx_shadow` | 0 | 0 | 0, 1 | same row as `psx_light` | `TNFS_PSX_SHADOW` | 0 / 0 / 1 | |
| `psx_time` | `day` | `day` | `dawn`, `day`, `dusk` (`a`, `b`, `c` also read) | Track Select: time of day, with the PS1 light on | `TNFS_PSX_TIME` | not set | `psx_light` |
| `psx_headlights` | 0 | 0 | 0, 1 (the night's second look: dark road, pool of light ahead of each car) | none | `TNFS_PSX_HEADLIGHTS` | 0 / 0 / 1 | `psx_light`, night |
| `psx_flares` | 0 | 0 | 0, 1 (a soft halo round lit brake lights and tunnel lamps in low light and fog) | none | `TNFS_PSX_FLARES` | 0 / 0 / 1 | `psx_light` |
| `police_glow` | 0 | 0 | 0, 1 (a near-white core in the police lights, a stronger halo in poor light) | none | `TNFS_POLICE_GLOW` | 0 / 0 / 1 | `psx_flares` for the halo |
| `sky_fill` | 0 | 0 | 0, 1 (the flat sky's art carried up to the view's top) | none | `TNFS_SKY_FILL` (`TNFS_SKY_FILL_MODE` a/b, dev) | 0 / 0 / 1 | |
| `sky_fill_below` | 0 | 0 | 0, 1 (the same carried down to the view's bottom; 16:9 and 21:9 only, never 4:3) | none | `TNFS_SKY_FILL_BELOW` | 0 / 0 / 1 | |
| `night_sky_hue` | 0 | 0 | 0, 1 (the dusk art's night sky takes a blue-black hue) | none | `TNFS_NIGHT_SKY_HUE` | 0 / 0 / 1 | `psx_light`, night |
| `night_stretch_clean` | 0 | 0 | 0, 1 (a star is not carried up as a stripe) | none | `TNFS_NIGHT_STRETCH_CLEAN` | 0 / 0 / 1 | `sky_fill`, night |
| `forest_dark` | 0 | 0 | 0, 1 (Vertigo Ridge's forest before the finish alternates two dark tones) | none | `TNFS_FOREST_DARK` | 0 / 0 / 1 | `psx_light` |
| `made_tunnels` | 0 | 0 | 0, 1 (made-light tracks take their tunnels from the road's property bit) | none | `TNFS_MADE_TUNNELS` | 0 / 0 / 1 | `psx_light`, TR6/TR7 |
| `car_tunnel_light` | 0 | 0 | 0, 1 (the car's light in a made-light tunnel is the tunnel's alone, with a ramp at the mouths) | none | `TNFS_CAR_TUNNEL_LIGHT` | 0 / 0 / 1 | `psx_light`, `made_tunnels` |
| `hall_light` | 0 | 0 | 0, 1 (TR7's foundry hall lights the car and its shadow with the scenery's light) | none | `TNFS_HALL_LIGHT` | 0 / 0 / 1 | `psx_light`, TR7 |
| `headlight_relight` | 0 | 0 | 0, 1 (the night pools' light is asked again every frame) | none | `TNFS_HEADLIGHT_RELIGHT` | 0 / 0 / 1 | `psx_headlights` |
| `vegas_car_night` | 0 | 0 | 0, 1 (Lost Vegas' made night takes the darker tint under the CD nights' ceiling) | none | `TNFS_VEGAS_CAR_NIGHT` | 0 / 0 / 1 | `psx_light`, night |
| `rally_gravel_shoulder` | 0 | 0 | 0, 1 (the rolling gravel scrubs with the shoulder's own tyre sound, by speed, no gravel strikes) | none | `TNFS_RALLY_GRAVEL_SHOULDER` | 0 / 0 / 1 | extras `rally_gravel` |
| `lamp_pool_tint` | 0 | 0 | 0, 1 (the road under a street lamp takes a warm yellow tint, strongest under the lamp and fading with the pool) | none | `TNFS_LAMP_POOL_TINT` | 0 / 0 / 1 | `psx_headlights` |
| `trans_shadows` | 0 | 0 | 0, 1 (Transtropolis' made light has no shade of the walls beside the airport's open approach to the truss bridge) | none | `TNFS_TRANS_SHADOWS` | 0 / 0 / 1 | `psx_light` |
| `brake_over_gas` | 0 | 0 | 0, 1 (a pad's brake past its dead zone beats the gas: the car brakes and the brake light lights; 0 sums them as the exe does) | none | `TNFS_BRAKE_OVER_GAS` | 0 / 0 / 1 | none |
| `lamp_pools` | 0 | 0 | 0, 1 (with the CD's light loaded at night, the street lamps' pools are laid over it: the ground round each lamp is lit) | none | `TNFS_LAMP_POOLS` | 0 / 0 / 1 | `psx_headlights` |
| `lamp_pool_bright` | 0 | 0 | 0, 1 (a street lamp's pool is not dimmed by the night's dark floor: under the lamp the light is full, fading with the pool; 0 the pool is dimmed by the headlights' gain) | none | `TNFS_LAMP_POOL_BRIGHT` | 0 / 0 / 1 | `psx_headlights` |
| `sign_glow` | 0 | 0 | 0, 1 (at night, with `psx_headlights`, the City's matrix sign, EXPRESS LANES CLOSED (CY1), has its amber bulbs lit like the windows; 0 the exe's) | none | `TNFS_SIGN_GLOW` | 0 / 0 / 1 | `psx_headlights`, CY1 |
| `analog_pad` | 0 | 0 | 0, 1 (a control bound to an axis (pad stick or trigger, wheel, pedals) steers and works the pedals in proportion, a key or button keeps full travel; 0 the exe's key paths) | none | `TNFS_ANALOG_PAD` | 1 / 1 / 1 | a game controller |
| `rain_v2` | 0 | 0 | 0, 1 (with `weather=rain` the rain has half the streaks, with `weather=storm` it has the old rain's count plus a quarter, with thunder after each lightning (0.5 to 4 s later, quieter the later; the sample is `data/sounds/thunder.wav`, CC0 by WuxiaScrub, see `data/sounds/LICENSES.md`), the streaks fall from higher, the wet road's lamp reflections show on the carriageway only, and the traffic's and police's spray wisp is a little longer and never gone at low speed; 0 as before) | none | `TNFS_RAIN_V2` | 0 / 0 / 1 | `weather=rain` or `storm`, `rain_fx` |
| `shoulder_light` | 0 | 0 | 0, 1 (at night with the headlights the shoulder's far edge takes at least the road's light before the beam and the lamps scale it: the shoulder and a third lane out there are lit like the road, not left near black; 0 as before) | none | `TNFS_SHOULDER_LIGHT` | 0 / 0 / 1 | `psx_headlights` |
| `police_halo` | 0 | 0 | 0..3 (the police roof lights' halo size: 0 as now, 1..3 progressively bigger (near and far)) | none | `TNFS_POLICE_HALO` | 0 / 0 / 1 | `police_glow`, `psx_flares` |
| `traffic_halo` | 0 | 0 | 0..1 (at night the traffic's lit lamps get a halo as the racers' do; 0 as now) | none | `TNFS_TRAFFIC_HALO` | 0 / 0 / 0 | `psx_flares`, night |
| `rally_dry_lamps` | 0 | 0 | 0..1 (in a rally (dirt) race in rain the cars' lamps are not reflected in the road, as dirt is no mirror; 0 the exe's wet-asphalt reflections) | none | `TNFS_RALLY_DRY_LAMPS` | 0 / 0 / 0 | `weather=rain`, `rain_fx`, a rally race |
| `traffic_spray` | 0 | 0 | 0..1 (the traffic's and the police's rain spray is a short wisp, not the long plume; the player's and the rivals' keep theirs; 0 the exe's) | none | `TNFS_TRAFFIC_SPRAY` | 0 / 0 / 0 | `weather=rain`, `rain_fx` |
| `headlight_cone` | 0 | 0 | 0, 1 (the car's own beam is a cone from its position and heading: it follows the nose, the turns and a U-turn, with no jump at section boundaries; 0 the pool along the track's nodes) | none | `TNFS_HEADLIGHT_CONE` | 0 / 0 / 1 | `psx_headlights`, `perspective` (without Perspective Correction the cone counts as 0: the node pool) |
| `rain_fx` | 0 | 0 | 0, 1 (rain streaks by speed and light) | none | `TNFS_RAIN_FX` | 0 / 0 / 1 | `weather=rain` |
| `rain_grip` | 0 | 0 | 0, 1 (with `weather=rain` the road grips at 4/5 and the race's times and records are not kept, provisionally; the shoulder keeps its grip) | none | `TNFS_RAIN_GRIP` | 0 / 0 / 1 | `weather=rain` |
| `gauge_light` | 0 | 0 | 0..5 (at night the lit parts of the dash gauges take a colour: 1 greenish, 2 reddish, 3 orange, 4 bluish, 5 "Del coche": the colour painted at each pixel in the dash's `dash_color.png` (below), orange where none or the car has no file; 0 the exe's untinted white; Graphics options page; a hand-made mask per dash, `data/dash_masks/<CAR>/dash_mask.png` or `$TNFS_DASH_MASKS`, from `tools/dash_mask_from_psd.py`, marks the lit parts: blue the dash, green a dial, yellow ffff00 or white always lit. The optional PSD layer "color" gives `data/dash_masks/<CAR>/dash_color.png` (same tool, `$TNFS_DASH_MASKS` too): paint the lit parts in the colour each is to glow, a different one per dial if wanted; only the strongest channel sets the hue, the dash's art the brightness; transparent or black = no colour (orange); up to 12 distinct colours; no menu row: the player does not pick it, the car's own colours do; Restomod defaults to 5) | none | `TNFS_GAUGE_LIGHT` | 0 / 0 / 5 | `weather=rain` |
| `finish_gate_light` | 0 | 0 | 0, 1 (at night, with `psx_headlights`, the stage tracks' gate that ends a stage (CY, CL, AL; CHECKPOINT, FINISH on the third stage) keeps its daylight light against the dark; 0 the exe's) | none | `TNFS_FINISH_GATE_LIGHT` | 0 / 0 / 1 | `psx_headlights`, CY/CL/AL |
| `lamp_car_shade` | 0 | 0 | 0, 1 (a car under a street lamp is lit by its pool: warmer and lighter, fading as it leaves) | none | `TNFS_LAMP_CAR_SHADE` | 0 / 0 / 1 | `psx_headlights` |
| `reverse_rows` | 0 | 0 | 0, 1 (a reversed circuit's rows are read the way the forward circuit's are (the exe's reverse swaps the two chains of each row, which bends a tunnel's roof into a slab and lays its wall down as a hillside): the tunnels keep their walls and roof; 0 the rows swapped as the exe's reverse has them) | none | `TNFS_REVERSE_ROWS` | 0 / 0 / 1 | none |
| `city_lift` | 0 | 0 | 0, 1, 2, 3 (with the PS1 light and headlights the City nights CY1..CY3 are brightened: 1 mild, 2 more, by raising the night tint's ceiling and the open night's dark floor, the beams added on top of that floor; 3 a darker base than 1 with brighter tunnels and lamp pools, so the beams stand out; 0 the exe's darkness) | none | `TNFS_CITY_LIFT` | 0 / 0 / 1 | `psx_light` |
| `vegas_lift` | 0 | 0 | 0, 1, 2 (the same for Lost Vegas' night: 1 mild, 2 more; 0 the exe's) | none | `TNFS_VEGAS_LIFT` | 0 / 0 / 2 | `psx_light` |
| `night_tone` | 0 | 0 | 0, 1, 2 (the night's flat tint of a track with no PS1 light, i.e. with no disc or no light file for it: 0 the fallback's warm brown-green; 1 each track's own CD night tint (constants, no PS1 data), brought under the City's and Lost Vegas' lift ceilings as on the CD path, with a brightness match to the CD's road; 2 one neutral cool blue-grey, the City's and Lost Vegas' following `city_lift` and `vegas_lift` from the look at city_lift 3 / vegas_lift 2; the CD's own light and tints are never changed) | none | `TNFS_NIGHT_TONE` | 0 / 0 / 2 | `psx_light`, night |
| `rain_lamps` | 0 | 0 | 0, 1 (with `weather=rain` or `storm` every car drives with its headlights on, day included: lit front lamps and their warm reflections on the wet road; no night beam or pools by day) | none | `TNFS_RAIN_LAMPS` | 0 / 0 / 0 | `weather=rain` |
| `gauge_exterior` | 0 | 0 | 0, 1 (with `gauge_light`, the gauges are lit whenever the exterior lights are on: night, dusk, and rain or storm with `rain_lamps`; by day dry they stay unlit; 0 at night only) | none | `TNFS_GAUGE_EXTERIOR` | 0 / 0 / 0 | `gauge_light` |
| `tunnel_headlight` | 0 | 0 | 0, 1 (with the PS1 headlights at night, the beam lights a tunnel's walls and road, and a lit hall's, to 180 of 255; 0 the exe's 150 in a tunnel, nothing in a hall) | none | `TNFS_TUNNEL_HEADLIGHT` | 0 / 0 / 1 | `psx_light` |
| `parking_dark` | 0 | 0 | 0, 1 (with car_tunnel_light, the car stays at the darkest tone of the hall in TR7's whole parking, nodes 465..570, with no alternating light; 0 the lamps' alternation) | none | `TNFS_PARKING_DARK` | 0 / 0 / 1 | `psx_light` |
| `forest_neutral` | 0 | 0 | 0, 1 (with the PS1 light, Vertigo Ridge's forest road, nodes 852..943 (the CD's rows mark its shade like a tunnel), is drawn with the road's neutral light instead of the tunnel's olive-orange cast; 0 the cast, yellow/orange asphalt) | none | `TNFS_FOREST_NEUTRAL` | 0 / 0 / 1 | `psx_light` |
| `cavern_dark` | 0 | 0 | 0, 1 (with car_tunnel_light, TR6's cavern after the blue-lit mine tunnel, nodes 322..546, keeps the car at the lamp alternation's dim tone with no alternating light until the exit; 0 the alternation) | none | `TNFS_CAVERN_DARK` | 0 / 0 / 1 | `psx_light` |
| `vegas_objects` | 0 | 0 | 0, 1 (with the PS1 light, Lost Vegas' buildings and 3D objects keep 245 of 255 of light at night while the road stays dark with the headlights and pools; 0 they are as dark as the road) | none | `TNFS_VEGAS_OBJECTS` | 0 / 0 / 1 | `psx_light` |
| `close_houses` | 0 | 0 | 0, 1 (the TR3 village houses get their right wall, back wall and back roof slope, the long building its far end cap; 0 they are open as in the exe) | none | `TNFS_CLOSE_HOUSES` | 0 / 0 / 1 | none |

### Dash, cameras and replay

| key | default | exe | range | menu | env | Stock / Restored / Restomod |
|---|---|---|---|---|---|---|
| `radar_all_views` | 0 | 0 | 0, 1 (the police radar shows in every view outside the car) | EXTRAS page 2: the radar row, labelled "Police radar" when on | `TNFS_RADAR_ALL_VIEWS` | 0 / 0 / 1 |
| `tach_dim` | 0 | 0 | 0, 1 (the outside views' tachometer takes the dash's half tint) | none | `TNFS_TACH_DIM` | 0 / 0 / 1 |
| `gauge_wheel` | 0 | 0 | 0, 1 (at night the wheel in front of the gauges takes the dash's half tint) | none | `TNFS_GAUGE_WHEEL` | 0 / 0 / 1 |
| `replay_no_ffb` | 0 | 0 | 0, 1 (a replay leaves the force-feedback stick and the pad's rumble still) | none | `TNFS_REPLAY_NO_FFB` | 0 / 0 / 1 |
| `chase_height` | 0 | 0 | 0, 1 (the chase eye behind the car keeps the flat chase's height above the road on a climb or a descent; acts only with extras `follow_camera`) | none | `TNFS_CHASE_HEIGHT` | 0 / 0 / 1 |
| `last_camera` | 0 | 0 (a race starts in the dash) | 0, 1 (a race starts in the last camera used) | none | `TNFS_LAST_CAMERA` | 0 / 0 / 1 |
| `camera_kind` | -1 | -1 | -1 (none yet), 0..5 | none; the game writes it, read only with `last_camera` | none | not set |

### Profile

| key | default | exe | range | menu | env | note |
|---|---|---|---|---|---|---|
| `profile` | `stock` (no line) | `stock` | `stock`, `restored`, `restomod` | Game Mode: Profile | none | the table below |

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
| `psx_liveries` | 0 | 0 | page 1: Car Colours | catalogue paints for every car, the PS1 second livery with `psx_dir`; the player's pick is `car_paint` | 0 / 0 / 1 |
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
| `update_check` | 1 | not applicable | page 3: Updates and anonymous stats | the update check and the three anonymous counters | not set |

### Keys with no EXTRAS row

| key | default | exe | where it is set | note | Stock / Restored / Restomod |
|---|---|---|---|---|---|
| `reverse` | 0 | 0 | Track Select: Reverse (offered when this is on); `TNFS_REVERSE=0\|1` overrides | circuits run the other way round; the tunnel/LOD byte and the models are handled in `tri_reverse` | not set |
| `rally_gravel` | 0 | 0 | none | gravel noise all along the rally and dust from the desert's tyres | 0 / 0 / 1 |
| `movies_wide` | 1 | 0 (the 4:3 frame in the middle) | none | on a wide screen a movie fills the width, top and bottom cropped | 0 / 1 / 1 |
| `car_paint` | all 0 | all 0 | the car showcase's Color box | one hex digit a car (cars 0..11): 0 own, 1 PS1 livery, 2+ the catalogue's paints | not set |

## What each profile sets (`TABLES`)

All 64 columns of the table, in file order. A column that is not here is not touched by any profile. "group" is the section above the key is in.

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
| `psx_light` | settings | 0 | 0 | 1 |
| `psx_headlights` | settings | 0 | 0 | 1 |
| `psx_shadow` | settings | 0 | 0 | 1 |
| `psx_flares` | settings | 0 | 0 | 1 |
| `body_shine` | settings | 0 | 0 | 4 |
| `car_colours` | settings | 0 | 0 | 1 |
| `fog_sample` | settings | 0 | 0 | 1 |
| `sky_fill` | settings | 0 | 0 | 1 |
| `movies_wide` | switches | 0 | 1 | 1 |
| `psx_liveries` | switches | 0 | 0 | 1 |
| `rally_gravel` | switches | 0 | 0 | 1 |
| `made_tunnels` | settings | 0 | 0 | 1 |
| `hall_light` | settings | 0 | 0 | 1 |
| `headlight_relight` | settings | 0 | 0 | 1 |
| `night_stretch_clean` | settings | 0 | 0 | 1 |
| `forest_dark` | settings | 0 | 0 | 1 |
| `night_sky_hue` | settings | 0 | 0 | 1 |
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
| `car_tunnel_light` | settings | 0 | 0 | 1 |
| `vegas_car_night` | settings | 0 | 0 | 1 |
| `rally_gravel_shoulder` | settings | 0 | 0 | 1 |
| `chase_height` | settings | 0 | 0 | 1 |
| `lamp_pool_tint` | settings | 0 | 0 | 1 |
| `trans_shadows` | settings | 0 | 0 | 1 |
| `tunnel_headlight` | settings | 0 | 0 | 1 |
| `parking_dark` | settings | 0 | 0 | 1 |
| `forest_neutral` | settings | 0 | 0 | 1 |
| `cavern_dark` | settings | 0 | 0 | 1 |
| `vegas_objects` | settings | 0 | 0 | 1 |
| `close_houses` | settings | 0 | 0 | 1 |
| `lamp_car_shade` | settings | 0 | 0 | 1 |
| `headlight_cone` | settings | 0 | 0 | 1 |
| `rain_fx` | settings | 0 | 0 | 1 |
| `rain_grip` | settings | 0 | 0 | 1 |
| `gauge_light` | settings | 0 | 0 | 5 |
| `finish_gate_light` | settings | 0 | 0 | 1 |
| `lamp_pool_bright` | settings | 0 | 0 | 1 |
| `sign_glow` | settings | 0 | 0 | 1 |
| `analog_pad` | settings | 1 | 1 | 1 |
| `police_halo` | settings | 0 | 0 | 1 |
| `rain_v2` | settings | 0 | 0 | 1 |
| `shoulder_light` | settings | 0 | 0 | 1 |
| `traffic_halo` | settings | 0 | 0 | 0 |
| `rally_dry_lamps` | settings | 0 | 0 | 0 |
| `traffic_spray` | settings | 0 | 0 | 0 |
| `reverse_rows` | settings | 0 | 0 | 1 |
| `city_lift` | settings | 0 | 0 | 1 |
| `vegas_lift` | settings | 0 | 0 | 2 |
| `night_tone` | settings | 0 | 0 | 2 |
| `rain_lamps` | settings | 0 | 0 | 0 |
| `gauge_exterior` | settings | 0 | 0 | 0 |
| `lamp_pools` | settings | 0 | 0 | 1 |
| `brake_over_gas` | settings | 0 | 0 | 1 |

The profile does not set: `fullscreen`, `fov_adjust`, `laps`, `free_records`, `psx_dir`, `psx_time`, `weather`, `render_bands`, `camera_kind`, and extras' `mirror`, `rally`, `warrior`, `vegas`, the cheats, `lunar_*`, `reverse`, `units`, `difficulty`, `start`, `no_movies`, `no_assists`, `replay_cameras`, `car_markers`, `stats_overlay`, `dash_box`, `variant_time`, `car_paint`, `update_check`.

## Development-only variables

Not a setting: a way to drive the headless runs and the checks. They are not stable and none is written anywhere. Headless runs go with the dummy SDL drivers, `timeout -k`, and `--mods <scratch>`.

- **Place and time:** `TNFS_START_NODE=N` (start the player on node N), `TNFS_FRAMES=N`, `TNFS_SHOTS=prefix:every` (screenshots during a run), `TNFS_MIDDAY=1`, `TNFS_DRIVE_TIME`, `TNFS_RUN`, `TNFS_RALLY=1`, `TNFS_OPPONENT=<car>`, `TNFS_REPLAY_REQUEST`.
- **Cars and cameras:** `TNFS_FOLLOW_CAR=<slot>` (the chase follows that car, 2 is the police), `TNFS_POLICE_NEAR=N` (the police N nodes behind the player, chasing; race type 1), `TNFS_VIEW`, `TNFS_LOOK_BACK_AT=from:to`, `TNFS_STEER=left|right`, `TNFS_SCRIPT=spin`, `TNFS_CAR=NAME` (`--track` stand-in).
- **Input scripts:** `TNFS_INPUT_SCRIPT="ms:KEY,..."`, `TNFS_INPUT_SETTLE`, `TNFS_F3_AT`, `TNFS_REPLAY_BUTTONS`, `TNFS_FAKE_PAD`, `TNFS_AUTOPILOT=1`.
- **Capture and dumps:** `TNFS_CAPTURE_DIR` with `TNFS_CAPTURE_EVERY` and `TNFS_CAPTURE_FROM` (every presented frame: set the interval, a capture of every frame fills the disk), `TNFS_AUDIO_DUMP=file.wav`, `TNFS_SCENERY_DUMP`, `TNFS_SCENERY_DUMP_NIGHT`, `TNFS_SLOT_DUMP`.
- **Traces:** `TNFS_VERBOSE=1` (or `--verbose`), `TNFS_TRACE_FAM`, `TNFS_TRACE_PLAYER=N`, `TNFS_FRAME_TIMES=1`, `TNFS_NOTICE=1|2`.
- **A/B overrides of automatic behaviour:** `TNFS_TWO_SIDED`, `TNFS_CLOSE_BOXES` (both automatic with a reversed track or the 360 camera), `TNFS_CAR_DRAW_DISTANCE=0` (the exe's distances at render scale 2 or 3), `TNFS_WIDE_BOARD=0`, `TNFS_EMISSIVE_LAMPS`, `TNFS_FOG_CURVE=line`, `TNFS_FOG_LIFT`, `TNFS_SKY_FILL_MODE`, `TNFS_STAND_IN`.
- **Data and folders:** `TNFS_PREF_DIR` (the user folder, for tests and modders), `TNFS_CAR_ENVMAP`, `TNFS_CAR_MASKS`.
- **Development builds only:** `TNFS_STATS_DRYRUN=<version>`, `TNFS_UPDATE_FAKE=9.9.9`; a release build ignores them.

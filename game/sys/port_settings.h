/* port: the port's own settings, by mode (port_profile.h), kept apart from the game's config.dat. */
#ifndef TNFS_PORT_SETTINGS_H
#define TNFS_PORT_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "game/platform/platform.h"

typedef enum { SCREEN_4_3 = 0, SCREEN_16_9 = 1, SCREEN_21_9 = 2 } ScreenFormat;

enum { PSX_DIR_MAX = 256 };   /* port: the longest psx_dir kept; a longer one is cut */

/* port: the PS1 time of day the light and its ambient colour come from (the letter of Z<TRACK><letter>.LGT / .LGS is 'B', 'A', 'C') */
enum { PSX_TIME_DAY = 0, PSX_TIME_DAWN = 1, PSX_TIME_DUSK = 2, PSX_TIME_NIGHT = 3 };   /* the night: the dusk's files and sky, under a darker light and a dark fog (drive_light.c) */

typedef struct {
    ScreenFormat format;     /* SCREEN_4_3 */
    int refresh_hz;          /* 60, 120 or 144; 60 */
    int scale_integer;       /* 0 fill, 1 whole multiples only; 0 */
    int fullscreen;          /* 0 */
    int fov_adjust;          /* degrees added to the vertical field, -10..+20; 0 */
    int view_ultra;          /* 1: the "ultra" view distance; 0 */
    int perspective;         /* 0/1: "Perspective Correction", the ex-EXTRAS Modern 3D */
    int texture_filter;      /* 0/1 */
    int fog;                 /* 0/1 */
    int f3_ultra;            /* 1: F3 rotates Far, Medium, Close and Ultra (port_f3_ultra); 0 the exe's three; 1 */
    int render_scale;        /* 1, 2 or 3: the 3D world drawn at that many times the frame's size (render-scale-design.md); 1 */
    int laps;                /* 0 the exe's laps, else 1..99 laps in every race (free_laps.h): `laps=N` */
    /* the folder of the PS1 CD's extracted car files (Z<CAR>.VSM / .WSM) for the mode's file's psx_liveries; "" none, and
     * the liveries stay off. Read at each race start. */
    char psx_dir[PSX_DIR_MAX];
    int psx_light;           /* 1: the PS1's baked light on bridges and tunnels, read from psx_dir (psx_light.h); 0 off */
    int psx_headlights;      /* with psx_light at night: 1 the night's second look, the road dark and lit by a pool ahead of each car (headlight_pool.h); 0 the first look */
    int psx_shadow;          /* 1: the PS1 look's soft shadow under each car, weaker in the dark (soft_shadow.h); 0 the exe's quad */
    int psx_time;            /* with psx_light: the PS1's time of day, PSX_TIME_DAWN (files A), PSX_TIME_DAY (B) or PSX_TIME_DUSK (C):
                             * `psx_time=dawn|day|dusk` (a, b, c also read) */
    int weather;             /* WEATHER_* (weather.h): `weather=clear|fog|rain|storm`, re/notes/weather-design.md; clear */
    /* the render threads: 0 off (the plain path, as the exe draws it), -1 "auto" (as many as the logical cores,
     * at most eight), or a thread count. The TNFS_BANDS environment variable wins over this one. */
    int render_bands;        /* -1 (auto) */
    int body_shine;          /* 1 subtle, 2 the default look, 3 strong: the horizon shine over the cars' paint (body_shine.h, re/TODO.md); 4 the envmap (car_envmap.h, Gran Turismo style streaks); 0 the exe's frames */
    int free_records;        /* 1: a free race (laps=N) of a lap count the track lacks keeps its own table of the ten best totals
                              * (free_records.h); 0: no total for it, as the exe has no table */
    int car_colours;         /* 1: the screen palette's slots the cars own are filled with the colours the cars show most (race_palette_fit);
                              * 0: the exe's own slots, whose nearest colours turn a body's light tones to grey */
    int fog_sample;          /* 1: the distance fog's colour from the horizon art's band around its line (modern_look.h); 0: from the
                              * art's last rows, as it was */
    int made_tunnels;        /* 1: with the Light Effects switch, a track whose light is made (no PS1 file: TR6, TR7) takes its tunnels from the road's property bit, so the
                              * tunnel tables (grey floor, warm cast) and the tunnel's light apply there as on the PS1 tracks; 0: as before */
    int psx_flares;          /* 1: with the Light Effects switch, a soft halo round a lit brake light in low light (dawn, dusk, night, a tunnel) and in fog
                              * (lamp_flare.h); 0: the exe's */
    int headlight_relight;   /* 1: with the night pools, the road's vertex light is asked again of the pools every frame, not only when the camera changes segment (the beam
                              * lagged and jumped every four nodes); 0: as before */
    int car_tunnel_light;    /* 1: with the PS1 light on a made-light track (TR6, TR7) that has the road's tunnel flags (made_tunnels), the car's light is the tunnel's alone, with a
                              * short ramp at the mouths: the terrain's shade (a canyon) no longer darkens it into the tunnel's light and lamps, and TR6's property-16 stretch after the
                              * mine is no longer taken for a hall (only TR7 has one); 0: as it was */
    int vegas_car_night;     /* 1: with the Light Effects switch and the night, Lost Vegas with no PS1 light takes its night tint under the same ceiling the CD's nights take,
                              * so the car is no longer lit as by day in its dark; 0: the fallback's night tint as it was */
    int rally_gravel_shoulder; /* 1: with the mode's file's rally_gravel, the rolling gravel scrubs with the shoulder's own tyre sound (race_sound_tyres, off-road variant, by speed)
                              * and no gravel strikes (those rang like drops); 0: as it was */
    int chase_height;        /* 1: with the mode's file's follow_camera, the chase eye behind the car keeps the flat chase's height above the road at the eye's own place on a climb or a
                              * descent (the exe's half lift left it low on climbs and high on descents); 0: as it was */
    int lamp_pool_tint;      /* 1: with the night pools, the road under a street lamp takes a warm yellow tint, strongest under the lamp and fading with the pool (psx_light_lamp_classes);
                              * 0: the pools only lift the light, as they did */
    int canyon_shade;        /* 1: with car_tunnel_light, the car is 30 % darker in Burnt Sienna's canyon (TR6, the property-16 stretch after the mine, nodes 547..585, where the walls
                              * shade the road); 0: full sun there, as it was */
    int tunnel_headlight;    /* 1: with the PS1 headlights at night, the beam lights a tunnel's walls and road (and a lit hall's) to 180 of 255, a soft light; 0: as it was */
    int parking_dark;    /* with car_tunnel_light, Transtropolis' parking (nodes 465..570, up to the exit tunnel) keeps the car at the dark tone of the lamps' alternation, steady; 0: the alternation */
    int forest_neutral;    /* with the PS1 light, TR3's forest road (nodes 852..943, the rows mark its shade like a tunnel) is drawn with the road's neutral light, not the tunnel's olive-orange cast (yellow/orange asphalt); 0: as it was */
    int cavern_dark;    /* with car_tunnel_light, Burnt Sienna's cavern (nodes 322..546, after the mine's tunnel) keeps the car at the dim tone, steady, no highway-lamp alternation; 0: the alternation */
    int vegas_objects;    /* with the PS1 light, Lost Vegas' buildings and 3D objects keep almost full light (245 of 255) while the road takes the dark and the headlights, so the lit windows and the neon stand out; 0: they take the road's dark */
    int close_houses;    /* TR3's village houses (model 4) and the long building (model 3) */
    int brake_over_gas;    /* 1: with split pedals (a pad's two triggers), a brake held past its dead zone is the pedal and the gas is ignored, so the car brakes and the brake light lights; 0: the exe's, the pedals summed (brake less gas) */
    int lamp_pools;    /* 1: with the PS1 headlights at night and the CD's own light loaded, the made street-lamp pools are laid over it too (the terrain lifted round each lamp, and with lamp_pool_tint the road's warm tint); 0: the CD's light as it is */
    int night_tone;   /* 0, 1, 2: the night's flat tint with no PS1 light (no CD .LGS): 0 the fallback's warm brown-green, 1 each track's CD tint, 2 one neutral cool blue-grey; the CD's own light is never touched */
    int rain_lamps;    /* 1: with weather=rain every car drives with its headlights on, day included: the lamp texels glow and the wet road shows their reflections; the night beam and pools stay the night's (no cone by day); 0: the exe's (the lamps follow the light only) */
    int gauge_exterior;    /* 1: with gauge_light the gauges are lit whenever the exterior lights are on (night, dusk, and rain with rain_lamps), not at night only; by day dry they stay unlit; 0: at night only, as now */
    int traffic_spray;    /* with weather=rain, the water a traffic or police car throws up is a short wisp (one particle a tick, a tenth of a second) not the long plume; the player's and the rivals' stay; 0 the exe's plume */
    int rally_dry_lamps;    /* with weather=rain in a rally (dirt) race the cars' lamps are not mirrored in the road (dirt is no mirror): the headlights' and tail lights' reflections are left out; 0 the exe's wet-asphalt reflections */
    int traffic_halo;    /* with the PS1 flares at night, the traffic's lit headlights and brake lights get their halo as the racers' do (the port draws it only for the racers and the police car); 0 as now */
    int police_halo;    /* the police car's roof lights' halo size: 0 the port's now, 1..3 progressively bigger, near and far */
    int analog_pad;    /* 1: a control bound to an axis (a pad's stick or trigger, a wheel, pedals) steers and works the pedals in proportion, one bound to a key or button keeps its full lock and full pedal (analog_pad.h); 0 the exe's key paths */
    int sign_glow;         /* 1: at night, with the PS1 headlights, the City's matrix sign (CY1: EXPRESS LANES CLOSED) has its amber bulbs lit like the windows (window_lights.h); 0 the exe's */
    int lamp_pool_bright;  /* 1: with the PS1 headlights at night, a street lamp's pool is not dimmed by the night's dark floor: under the lamp the road, its shoulders and the scenery take the lamp's light in full, fading with the pool (the headlights' gain no longer swallows it); 0: the pool lifts the light the gain then dims, as now */
    int headlight_cone;    /* 1: with the PS1 headlights at night, the car's beam is a cone from its position and heading, per vertex of the road and scenery (it follows the nose, a turn, a U-turn); 0: the pool along the track's nodes as it was */
    int lamp_car_shade;    /* 1: with the PS1 headlights at night, a car under a street lamp's pool is lit by it (half way to a warm light at the lamp, fading with the pool), CD light or made; 0: the car's shade as it was */
    int reverse_rows;    /* a reversed TR3 reads the tunnel rows in the order their polygon tables were made for, so the roof and walls keep their shape */
    int city_lift;    /* 0, 1, 2: with the PS1 light and headlights, the City nights (CY1..CY3) are brought under a higher ceiling and their dark floor is lifted (1 mild, 2 more); 0 the exe's darkness (ceiling .35) */
    int vegas_lift;    /* 0, 1, 2: the same for Lost Vegas' night (1 mild, 2 more); 0 the exe's */
    int rain_fx;    /* with weather=rain, the rain's streaks follow the car's speed (spokes from the vanishing point) and are lit by the headlights at night; 0 phase 1's fixed streaks */
    int rain_grip;    /* with weather=rain, the road's grip is reduced; records of a wet race are kept apart */
    int finish_gate_light;    /* 1: at night the stage tracks' CHECKPOINT / FINISH gate banner keeps its daylight look, lit against the dark (finish_gate_light.h); 0 the exe's */
    int gauge_light;    /* 0..4: at night the lit parts of the dash gauges take a colour (1 greenish, 2 reddish, 3 orange, 4 bluish); 0 the exe's untinted white */
    int trans_shadows;       /* 1: with the PS1 light, Transtropolis' made light has no shade of the walls beside the airport's open approach to the truss bridge
                              * (psx_light_synth_trans); 0: the made light as it was */
    int hall_light;          /* 1: with the PS1 light on a made-light track (TR6, TR7), the car and the soft shadow in TR7's foundry hall take the scenery's own
                              * light there, not the tunnel's dark; 0: the hall is a tunnel to the car, as it was */
    int sky_fill;            /* 1: the flat sky's art extended upward until it reaches the view's top (sky_stretch.h);
                              * 0: the exe's, a flat colour above the art's top */
    int night_sky_hue;       /* 1: the night's sky of the dusk's art takes a blue-black hue at its own luminance (psx_sky_draw_rehue); 0: the art's
                              * hues under the night's tint, as it was */
    int forest_dark;         /* 1: Vertigo Ridge's forest before the finish alternates two dark tones (psx_light_forest); 0: the made light as it was */
    int night_stretch_clean; /* 1: the night's sky carried up to the view's top does not carry a star up as a stripe (sky_stretch.h, SKY_FILL_COLUMN_QUIET); 0: as it was */
    int replay_no_ffb;       /* 1: a replay does not move the force-feedback stick or the pad's rumble (race_forces_replay_silent); 0: the exe's, it shakes as in the race */
    int last_camera;         /* 1: a race starts in the camera the player last used (the intro's fly-over lands on it, not on the dash): the game writes camera_kind itself; 0: the exe's, the dash */
    int camera_kind;         /* the chase kind last used live (0..5, race_chase.h), -1 none yet; read only with last_camera */
    int sky_fill_below;      /* 1: the flat sky's art extended downward until it reaches the bottom of the view, every column in the colour its last rows share (sky_stretch_below), 16:9 and 21:9 only; 0: the exe's, one flat colour from the art's last pixel */
    int radar_all_views;     /* 1: EXTRAS' police radar (Bumper Radar) shows in every view outside the car, not only the bumper's (drive_radar_view); 0: the bumper view only, as it was */
    int tach_dim;            /* 1: the R&T tachometer of the views outside the car takes the dash's half tint with the time of day (a little darker at night), its needle staying lit (tach_dim.h); 0: untinted, as it was */
    int gauge_wheel;         /* 1: at night the steering wheel in front of the gauges is the dash's (half tint) like the rest of the wheel, not lit with the dials; 0: lit, as it was */
    int police_glow;         /* 1: the police car's lights have a near-white core of the light's colour, and a stronger halo in poor light (psx_flares on); 0: the flat colour, as it was */
    int profile;             /* a Profile (port_profile.h): the mode in use, `profile=stock|restored|restomod` in port.cfg, Stock without the line. Last, so the positional initialisers keep their meaning. */
} PortSettings;

/* The scale the race renders at: TNFS_RENDER_SCALE (1..3) when set, else the mode's render_scale. */
int port_render_scale(void);

/* 1 when the race draws with the PS1's light: TNFS_PSX_LIGHT=0/1 when set, else the mode's psx_light. */
int port_psx_light(void);
/* 1 when the cars have the PS1 look's soft shadow: TNFS_PSX_SHADOW=0/1 when set, else the mode's psx_shadow. */
int port_psx_shadow(void);

/* 1 when the night is lit by the cars' headlight pools: TNFS_PSX_HEADLIGHTS=0/1 when set, else the mode's psx_headlights. */
int port_psx_headlights(void);
/* 1 when a free race's total goes in the per-lap-count tables (free_records.h): TNFS_FREE_RECORDS=0/1 when set, else the mode's free_records. */
int port_free_records(void);
/* 1 when the cars' palette slots are fitted to the colours they show (race_palette.h): TNFS_CAR_COLOURS=0/1 when set, else the mode's car_colours. */
int port_car_colours(void);
/* 1 when the distance fog takes its colour from the band around the horizon's line: TNFS_FOG_SAMPLE=0/1 when set, else the mode's fog_sample. */
int port_fog_sample(void);
/* the percent (0..100) of the way to white that band's colour is lifted: TNFS_FOG_LIFT when set, else FOG_SAMPLE_LIFT_DEFAULT. */
int port_fog_lift(void);
enum { FOG_SAMPLE_LIFT_DEFAULT = 15 };

/* 1 when the flat sky's art reaches the top of the view (sky_stretch.h): TNFS_SKY_FILL=0/1 when set, else the mode's sky_fill. */
int port_sky_fill(void);
/* 1 when the wheel in front of the gauges is not lit with them: TNFS_GAUGE_WHEEL=0/1 when set, else the mode's gauge_wheel. */
int port_gauge_wheel(void);
/* 1 when the police lights have a bright core and a stronger halo in poor light: TNFS_POLICE_GLOW=0/1 when set, else the mode's police_glow. */
int port_police_glow(void);
/* 1 when the outside views' tachometer is tinted: TNFS_TACH_DIM=0/1 when set, else the mode's tach_dim. */
int port_tach_dim(void);
/* 1 when the flat sky's art reaches the bottom of the view: TNFS_SKY_FILL_BELOW=0/1 when set, else the mode's sky_fill_below; never in 4:3 (sky_fill_below_applies). */
int port_sky_fill_below(void);
/* the flag `s` has, and the format it is for: on, and not 4:3, where the exe's fill stays whatever the flag says */
int sky_fill_below_applies(const PortSettings *s);
/* 1 when the police radar shows in every outside view: TNFS_RADAR_ALL_VIEWS=0/1 when set, else the mode's radar_all_views. */
int port_radar_all_views(void);
/* 1 when the night's sky is re-hued blue-black (psx_sky_draw.h): TNFS_NIGHT_SKY_HUE=0/1 when set, else the mode's night_sky_hue. */
int port_night_sky_hue(void);
/* 1 when Vertigo Ridge's forest is sombre (psx_light_make.h): TNFS_FOREST_DARK=0/1 when set, else the mode's forest_dark. */
int port_forest_dark(void);
/* 1 when the night's stretched sky leaves its stars out of the fill: TNFS_NIGHT_STRETCH_CLEAN=0/1 when set, else the mode's night_stretch_clean. */
int port_night_stretch_clean(void);
/* 1 when a replay leaves the force feedback still: TNFS_REPLAY_NO_FFB=0/1 when set, else the mode's replay_no_ffb. */
int port_replay_no_ffb(void);
/* 1 when a race starts in the last camera used: TNFS_LAST_CAMERA=0/1 when set, else the mode's last_camera. */
int port_last_camera(void);
/* 1 when a lit brake light has its halo (lamp_flare.h): TNFS_PSX_FLARES=0/1 when set, else the mode's psx_flares; never without port_psx_light. */
int port_psx_flares(void);

/* 1 when a made light marks its tunnels from the road (made_tunnels): TNFS_MADE_TUNNELS=0/1 when set, else the mode's made_tunnels; never without port_psx_light. */
int port_made_tunnels(void);
/* 1 when the scenery's open models draw from behind too: TNFS_TWO_SIDED=0/1 (dev) when set, else `automatic`: the race has a reversed track or the 360 camera
 * on (both show the scenery from behind), decided when the track's models are made. No port.cfg key (an old two_sided_scenery line is ignored). */
int port_two_sided_scenery(int automatic);
/* 1 when the L buildings (the two-sided boards) are closed into boxes (track_object_draw.h): TNFS_CLOSE_BOXES=0/1 (dev, for A/B) when set, else `automatic`, the same
 * condition as the open models drawn from behind: a reversed track or the 360 camera. No port.cfg key, no menu. */
int port_close_boxes(int automatic);
/* 1 when the foundry hall's car and shadow take the scenery's light (psx_light_synth.h): TNFS_HALL_LIGHT=0/1 when set, else the mode's hall_light; never without port_psx_light. */
int port_hall_light(void);

/* 1 when the road's light is asked again each frame (headlight_relight): TNFS_HEADLIGHT_RELIGHT=0/1 when set, else the mode's; never without port_psx_headlights. */
int port_headlight_relight(void);

/* 1 when the car's light of a made light is the tunnel's alone (car_tunnel_light): TNFS_CAR_TUNNEL_LIGHT=0/1 when set, else the mode's; never without port_psx_light. */
int port_car_tunnel_light(void);
/* 1 when Lost Vegas' made night has the darker tint (vegas_car_night): TNFS_VEGAS_CAR_NIGHT=0/1 when set, else the mode's; never without port_psx_light. */
int port_vegas_car_night(void);
/* 1 when the rolling rally gravel strikes as the shoulder does (rally_gravel_shoulder): TNFS_RALLY_GRAVEL_SHOULDER=0/1 when set, else the mode's. */
int port_rally_gravel_shoulder(void);
/* 1 when every chase eye keeps its height above the road (chase_height): TNFS_CHASE_HEIGHT=0/1 when set, else the mode's; acts only with follow_camera. */
int port_chase_height(void);
/* 1 when a street lamp's pool tints the road warm (lamp_pool_tint): TNFS_LAMP_POOL_TINT=0/1 when set, else the mode's; never without port_psx_headlights. */
int port_lamp_pool_tint(void);
/* 1 when Transtropolis' made light is corrected (trans_shadows): TNFS_TRANS_SHADOWS=0/1 when set, else the mode's; never without port_psx_light. */
int port_trans_shadows(void);

/* 1 when the car is shaded in the canyon (canyon_shade): TNFS_CANYON_SHADE=0/1 when set, else the mode's; never without port_car_tunnel_light. */
int port_canyon_shade(void);
/* 180 when the beam lights tunnels (tunnel_headlight): TNFS_TUNNEL_HEADLIGHT=0/1 when set, else the mode's; never without port_psx_headlights. 0 off. */
int port_tunnel_headlight(void);
/* 1 when parking_dark is on: TNFS_PARKING_DARK=0/1 when set, else the mode's; never without port_car_tunnel_light. */
int port_parking_dark(void);
/* 1 when forest_neutral is on: TNFS_FOREST_NEUTRAL=0/1 when set, else the mode's; never without port_psx_light. */
int port_forest_neutral(void);
/* 1 when cavern_dark is on: TNFS_CAVERN_DARK=0/1 when set, else the mode's; never without port_car_tunnel_light. */
int port_cavern_dark(void);
/* 1 when vegas_objects is on: TNFS_VEGAS_OBJECTS=0/1 when set, else the mode's; never without port_psx_light. */
int port_vegas_objects(void);
/* 1 when close_houses is on: TNFS_CLOSE_HOUSES=0/1 when set, else the mode's; never without port_psx_light. */
int port_close_houses(void);
/* 1 when lamp_car_shade: TNFS_LAMP_CAR_SHADE=0/1 when set, else the mode's; never without port_psx_headlights(). */
int port_lamp_car_shade(void);
/* 1 when headlight_cone: TNFS_HEADLIGHT_CONE=0/1 when set, else the mode's; never without port_psx_headlights(). */
int port_headlight_cone(void);
/* 1 when rain_fx is on: TNFS_RAIN_FX=0/1 when set, else the mode's. */
int port_rain_fx(void);
/* 1 when rain_grip is on: TNFS_RAIN_GRIP=0/1 when set, else the mode's. */
int port_rain_grip(void);
/* 1 when gauge_light is on: TNFS_GAUGE_LIGHT=0/1 when set, else the mode's. */
int port_gauge_light(void);

/* 1 when finish_gate_light is on: TNFS_FINISH_GATE_LIGHT=0/1 when set, else the mode's; only acts at night with the PS1 light (psx_headlights). */
int port_finish_gate_light(void);
/* 1 when lamp_pool_bright: TNFS_LAMP_POOL_BRIGHT=0/1 when set, else the mode's; never without port_psx_headlights(). */
int port_lamp_pool_bright(void);
/* 0, 1 or 2 (night_tone): TNFS_NIGHT_TONE=0/1/2 when set, else the mode's. */
int port_night_tone(void);
/* 1 when rain_lamps: TNFS_RAIN_LAMPS=0/1 when set, else the mode's; acts with weather=rain or storm. */
int port_rain_lamps(void);
/* 1 when gauge_exterior: TNFS_GAUGE_EXTERIOR=0/1 when set, else the mode's. */
int port_gauge_exterior(void);
/* 1 when sign_glow: TNFS_SIGN_GLOW=0/1 when set, else the mode's; never without port_psx_headlights(). */
int port_sign_glow(void);

/* 1 when analog_pad: TNFS_ANALOG_PAD=0..1 when set, else the mode's. */
int port_analog_pad(void);

/* the police roof halo's size 0..3: TNFS_POLICE_HALO=0..3 when set, else the mode's. */
int port_police_halo(void);

/* 1 when traffic_halo: TNFS_TRAFFIC_HALO=0..1 when set, else the mode's. */
int port_traffic_halo(void);

/* 1 when rally_dry_lamps: TNFS_RALLY_DRY_LAMPS=0..1 when set, else the mode's. */
int port_rally_dry_lamps(void);

/* 1 when traffic_spray: the traffic's and the police's spray is short: TNFS_TRAFFIC_SPRAY=0..1 when set, else the mode's. */
int port_traffic_spray(void);
/* 1 for a wet race: rain_grip on and the weather rain or storm (the grip is reduced and the race's records are not kept; track_gravity.h). */
int port_rain_grip_active(void);
/* 1 when reverse_rows is on: TNFS_REVERSE_ROWS=0/1 when set, else the mode's; never without port_psx_light. */
int port_reverse_rows(void);
/* The city_lift level (0..2): TNFS_CITY_LIFT=0..2 when set, else the mode's; 0 without port_psx_light. */
int port_city_lift(void);
/* The vegas_lift level (0..2): TNFS_VEGAS_LIFT=0..2 when set, else the mode's; 0 without port_psx_light. */
int port_vegas_lift(void);
/* 1 when lamp_pools: TNFS_LAMP_POOLS=0/1 when set, else the mode's; never without port_psx_headlights(). */
int port_lamp_pools(void);
/* 1 when brake_over_gas: TNFS_BRAKE_OVER_GAS=0/1 when set, else the mode's. */
int port_brake_over_gas(void);

/* how sky_fill fills the rows above the art (sky_stretch.h's SKY_FILL_*): TNFS_SKY_FILL_MODE=a|b (or 1|2) when set, else the default. */
enum { SKY_FILL_MODE_DEFAULT = 1 };
int port_sky_fill_mode(void);
/* The time of day (PSX_TIME_*): TNFS_PSX_TIME=dawn|day|dusk when set, else the mode's psx_time. */
int port_psx_time(void);
/* 1 when the masked lamp texels glow in low light (lamp_emissive.h): the Graphics Light Effects switch (port_psx_light), unless TNFS_EMISSIVE_LAMPS=0/1 is set. */
int port_emissive_lamps(void);

/* the cars' body shine strength, 0 (the exe's frames) to 4: TNFS_BODY_SHINE=0..4 when set, else the mode's body_shine. */
int port_body_shine(void);
/* The weather of the race: TNFS_WEATHER=clear|fog|rain|storm when set, else the mode's weather (a WEATHER_* of weather.h). */
int port_weather(void);

/* The mode port.cfg names (Stock without the line or the file). */
int port_settings_profile(void);
/* true when port.cfg names a mode: the player has chosen one */
bool port_settings_profile_chosen(void);
/* Makes `p` (a Profile) the mode in use, its own changes coming back with it: only the mode's name is written. False when the file does not take it. */
bool port_profile_select(int p);
/* Back to mode `p`'s values: its file emptied. False when the file does not take it. */
bool port_profile_reset(int p);
/* Where the display's size comes from (the program sets platform_desktop_size; without one, or when it cannot tell, port_monitor_format gives its fallback). */
void port_settings_set_display_probe(bool (*probe)(int *width, int *height));
/* The widescreen format of the display the window is on (4:3 on a 4:3 one); `fallback` when it cannot be told. */
ScreenFormat port_monitor_format(ScreenFormat fallback);

PortSettings port_settings_defaults(void);
/* "key=value" per line; unknown keys and out-of-range values are ignored (the default stays). */
PortSettings port_settings_parse(const char *text, size_t size);
/* Writes the text form, NUL-terminated; returns its length, or 0 when it does not fit. */
/* the room the mode's text needs: every key, a psx_dir of PSX_DIR_MAX bytes, and space for the keys still to come */
enum { PORT_CFG_TEXT_MAX = 2048 };
size_t port_settings_format(const PortSettings *s, char *out, size_t size);
/* The settings of the mode in use: its table's values, with what its file says for what is free in it. */
/* what port_old_files.c builds a mode's file from: mode `p`'s values with the machine's folder, one key=value over a PortSettings, and port.cfg's two lines */
PortSettings port_mode_values(int p, const char *psx_dir);
void port_settings_take(void *to, const char *key, const char *value);
bool port_machine_save(int profile, const char *psx_dir);
PortSettings port_settings_load(void);
/* the mode's name and folder as `s` has them, and in the file of `s`'s mode what differs from its values and is free in it. */
bool port_settings_save(const PortSettings *s);
/* The platform's config for the saved settings: the format's width by 480 lines, scaling, full screen, refresh. */
PlatformConfig port_platform_config(const char *title);

#endif

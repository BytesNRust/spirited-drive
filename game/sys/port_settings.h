/* port: the port's own settings, by mode (port_profile.h), kept apart from the game's config.dat. */
#ifndef TNFS_PORT_SETTINGS_H
#define TNFS_PORT_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "game/platform/platform.h"

typedef enum { SCREEN_4_3 = 0, SCREEN_16_9 = 1, SCREEN_21_9 = 2 } ScreenFormat;

enum { PSX_DIR_MAX = 256 };   /* port: the longest psx_dir kept; a longer one is cut */

/* port: the console version time of day the light and its ambient colour come from (the letter of Z<TRACK><letter>.LGT / .LGS is 'B', 'A', 'C') */
enum { LIGHT_TIME_DAY = 0, LIGHT_TIME_DAWN = 1, LIGHT_TIME_DUSK = 2, LIGHT_TIME_NIGHT = 3 };   /* the night: the dusk's files and sky, under a darker light and a dark fog (drive_light.c) */

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
    /* the folder of the PS1 CD's extracted car files (Z<CAR>.VSM / .WSM) for the mode's file's extra_liveries; "" none, and
     * the liveries stay off. Read at each race start. */
    char psx_dir[PSX_DIR_MAX];
    int light_effects;           /* 1: the the console version's baked light on bridges and tunnels, read from psx_dir (track_node_light.h); 0 off */
    int car_soft_shadow;          /* 1: the light effects' soft shadow under each car, weaker in the dark (soft_shadow.h); 0 the exe's quad */
    int light_time;            /* with light_effects: the the console version's time of day, LIGHT_TIME_DAWN (files A), LIGHT_TIME_DAY (B) or LIGHT_TIME_DUSK (C):
                             * `light_time=dawn|day|dusk` (a, b, c also read) */
    int weather;             /* WEATHER_* (weather.h): `weather=clear|fog|rain|storm`; clear */
    /* the render threads: 0 off (the plain path, as the exe draws it), -1 "auto" (as many as the logical cores,
     * at most eight), or a thread count. The TNFS_BANDS environment variable wins over this one. */
    int render_bands;        /* -1 (auto) */
    int body_shine;          /* 4 the envmap over the cars' paint (car_envmap.h, Gran Turismo style streaks); 0 the exe's frames; a file's 1..3 are read as 4 */
    int free_records;        /* 1: a free race (laps=N) of a lap count the track lacks keeps its own table of the ten best totals
                              * (free_records.h); 0: no total for it, as the exe has no table */
    int car_colours;         /* 1: the screen palette's slots the cars own are filled with the colours the cars show most (race_palette_fit);
                              * 0: the exe's own slots, whose nearest colours turn a body's light tones to grey */
    int fog_sample;          /* 1: the distance fog's colour from the horizon art's band around its line (modern_look.h); 0: from the
                              * art's last rows, as it was */
    int lamp_flares;          /* 1: with the Light Effects switch, a soft halo round a lit brake light in low light (dawn, dusk, night, a tunnel) and in fog
                              * (lamp_flare.h); 0: the exe's */
    int rally_gravel_shoulder; /* 1: with the mode's file's rally_gravel, the rolling gravel scrubs with the shoulder's own tyre sound (race_sound_tyres, off-road variant, by speed)
                              * and no gravel strikes (those rang like drops); 0: as it was */
    int close_houses;    /* TR3's village houses (model 4) and the long building (model 3) */
    int brake_over_gas;    /* 1: with split pedals (a pad's two triggers), a brake held past its dead zone is the pedal and the gas is ignored, so the car brakes and the brake light lights; 0: the exe's, the pedals summed (brake less gas) */
    int rain_lamps;    /* 1: with weather=rain every car drives with its headlights on, day included: the lamp texels glow and the wet road shows their reflections; the night beam and pools stay the night's (no cone by day); 0: the exe's (the lamps follow the light only) */
    int gauge_exterior;    /* 1: with gauge_light the gauges are lit whenever the exterior lights are on (night, dusk, and rain with rain_lamps), not at night only; by day dry they stay unlit; 0: at night only, as now */
    int rally_dry_lamps;    /* with weather=rain in a rally (dirt) race the cars' lamps are not mirrored in the road (dirt is no mirror): the headlights' and tail lights' reflections are left out; 0 the exe's wet-asphalt reflections */
    int traffic_halo;    /* with the console version flares at night, the traffic's lit headlights and brake lights get their halo as the racers' do (the port draws it only for the racers and the police car); 0 as now */
    int rain_v2;        /* 1: the rain is lighter (half the streaks), the storm is the old rain a little heavier with thunder after each lightning, the wet road's lamp reflections stay on the carriageway, the streaks fall from higher, and the traffic's and police's spray wisp is longer with a minimum at any speed; 0: as it was */
    int snow_weather;     /* 1: on a track whose places file says `snow 1` (Alpine 3) the rain is snow: flakes for streaks, no lightning or thunder in the storm, and the rally's road and shoulder are covered in snow (rally_gravel.h); 0 the rain as it was */
    int rain_tyre_roll;   /* 1: in rain or a storm (rain_v2) the rain round the followed car, every car's tyre spray and puddle splashes (rain_roll.h, rain_cars.h); 0: none */
    int analog_pad;    /* 1: a control bound to an axis (a pad's stick or trigger, a wheel, pedals) steers and works the pedals in proportion, one bound to a key or button keeps its full lock and full pedal (analog_pad.h); 0 the exe's key paths */
    int sign_glow;         /* 1: at night, with the headlight beams, the City's matrix sign (CY1: EXPRESS LANES CLOSED) has its amber bulbs lit like the windows (window_lights.h); 0 the exe's */
    int reverse_rows;    /* a reversed TR3 reads the tunnel rows in the order their polygon tables were made for, so the roof and walls keep their shape */
    int city_lift;    /* 1: with the baked light and headlights, the City nights (CY1..CY3) are brought under a higher ceiling (.42); an older file's higher steps read as 1; 0 the exe's darkness (ceiling .35) */
    int vegas_lift;    /* 1: the same for Lost Vegas' night (ceiling .80, its dark floor lifted); an older file's higher steps read as 1; 0 the exe's */
    int rain_fx;    /* with weather=rain, the rain's streaks follow the car's speed (spokes from the vanishing point) and are lit by the headlights at night; 0 phase 1's fixed streaks */
    int rain_grip;    /* with weather=rain, the road's grip is reduced; records of a wet race are kept apart */
    int finish_gate_light;    /* 1: at night the stage tracks' CHECKPOINT / FINISH gate banner keeps its daylight look, lit against the dark (finish_gate_light.h); 0 the exe's */
    int gauge_light;    /* 0..5: at night the lit parts of the dash gauges take a colour (1 greenish, 2 reddish, 3 orange, 4 bluish, 5 the car's: the colours painted in the dash's dash_color.png, orange where none); 0 the exe's untinted white */
    int sky_fill;            /* 1: the flat sky's art extended upward until it reaches the view's top (sky_stretch.h);
                              * 0: the exe's, a flat colour above the art's top */
    int replay_no_ffb;       /* 1: a replay does not move the force-feedback stick or the pad's rumble (race_forces_replay_silent); 0: the exe's, it shakes as in the race */
    int last_camera;         /* 1: a race starts in the camera the player last used (the intro's fly-over lands on it, not on the dash): the game writes camera_kind itself; 0: the exe's, the dash */
    int camera_kind;         /* the chase kind last used live (0..5, race_chase.h), -1 none yet; read only with last_camera */
    int sky_fill_below;      /* 1: the flat sky's art extended downward until it reaches the bottom of the view, every column in the colour its last rows share (sky_stretch_below), 16:9 and 21:9 only; 0: the exe's, one flat colour from the art's last pixel */
    int radar_all_views;     /* 1: EXTRAS' police radar (Bumper Radar) shows in every view outside the car, not only the bumper's (drive_radar_view); 0: the bumper view only, as it was */
    int tach_dim;            /* 1: the R&T tachometer of the views outside the car takes the dash's half tint with the time of day (a little darker at night), its needle staying lit (tach_dim.h); 0: untinted, as it was */
    int gauge_wheel;         /* 1: at night the steering wheel in front of the gauges is the dash's (half tint) like the rest of the wheel, not lit with the dials; 0: lit, as it was */
    int police_glow;         /* 1: the police car's lights have a near-white core of the light's colour, and a stronger halo in poor light (lamp_flares on); 0: the flat colour, as it was */
    int reflectors;    /* 1: at night or in a dark tunnel, with the headlight beams, the texels a track mask marks reflective shine in the player's lamp cone out to 200 m (reflector.h); 0 the exe's */
    int handbrake_lamps;   /* 1: a brake that is the handbrake's alone (the rally's handbrake is a full brake, car_physics.c) does not light the brake lamps; 0 the exe's */
    int rally_spray;    /* 1: in a rally with rain_fx and rain or snow, every car (the player's too) throws the road's spray, tinted by the road it is on (gravel grey, brown dirt, snow white) and darker than water; 0 the exe's */
    int weather_fog;    /* 1: the weather's fog in snow and in the rain storm (modern 3D): snow fog whiter, closer and with the exponential curve (heavier in the snow storm), the rain storm's denser than the rain's, and no dark rain mood over the snow; an older file's 2 reads as 1; 0 the exe's */
    int spray_v4;      /* 1: the wheels' spray and dust as the generated sprites of each material (water drops and a flat mist, the desert's cottony dust, the snow's powder and lumps, gravel stones and a fine dust), thrown by physics with gravity and landing on the road, born each simulation tick at the wheel's place; replaces rain_fx's spray and the rally's dust; 0 as before */
    int menu_pad_cancel;   /* 0..1: the gamepad's second button (B by default, the exe's Enter) is Escape in the menus and the name box (back / exit / cancel); the first (A) keeps accepting; 0 the exe's: both accept */
    int wrong_way;      /* 0..1: a centred blinking WRONG WAY (FALSCHE RICHTUNG in German) when the player's car drives forwards with its nose more than a quarter turn from the road, above 30 km/h, for 1.5 s (wrong_way.h); reversing never; 0 the exe's: none */
    int light_edit;     /* 0..1, the machine's (port.cfg, as profile and psx_dir; no mode file holds it): the live light editor in a race (light_edit.h); 0 off in every mode */
    int tunnel_lamps;   /* 1 (day and night; an older file's 2 reads as 1): the lamps of the tunnels and caverns (tunnel_lamp_textures.c's table: CY1/2/3, TR3, TR4) light the walls and the road in a pool under each, ADDED over the tunnel's own light (tunnel_tone), so a dark tunnel still has bright pools; 0 the exe's */
    int wipers;        /* 1: in the dash view in rain and snow a wiper blade sweeps the glass (the pace follows what falls), clears the lens drops and snow marks it crosses and hides the blades some dash art draws (a red dash mask colour); 0 as before */
    int snow_fx;    /* 1: with snow_weather, the falling snow has more depth: flakes less regular and soft, leaning with the car's speed (a smaller angle than the rain's), a snowstorm whose wind swings from side to side, and white marks on the lens (fewer than the rain's drops); 0 as before */
    int lamp_halos;    /* 1: at night the street lamps (the lamp boards' emissive entries) get a soft halo like the brake lights' (never the windows'); 0 as before */
    int profile;             /* a Profile (port_profile.h): the mode in use, `profile=stock|restored|restomod` in port.cfg, Stock without the line. Last, so the positional initialisers keep their meaning. */
} PortSettings;

/* The scale the race renders at: TNFS_RENDER_SCALE (1..3) when set, else the mode's render_scale. */
int port_render_scale(void);

/* 1 when the race draws with the the console version's light: TNFS_LIGHT_EFFECTS=0/1 when set, else the mode's track_node_light. */
int port_light_effects(void);
/* 1 when the cars have the light effects' soft shadow: TNFS_CAR_SOFT_SHADOW=0/1 when set, else the mode's car_soft_shadow. */
int port_car_soft_shadow(void);


/* 1 when the headlight beam is a cone from the car's position and heading (it follows the nose, a turn, a U-turn): the headlight beams with Perspective Correction (Modern 3D); without it the
 * pool along the track's nodes. Not a setting of its own. */
int port_headlight_cone(void);
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
/* port (online): 0 while a link race runs under a Stock or Restored host's rules (session_rules.h): the police radar and the 360 camera are off for everyone then, whatever
 * each player's own file says; 1 otherwise (a Restomod host, no link race, an older host). Never saved, nothing in the files changes; the rules end with the race. */
int port_host_allows_radar_and_camera(void);

/* 1 when the police radar shows in every outside view: TNFS_RADAR_ALL_VIEWS=0/1 when set, else the mode's radar_all_views. */
int port_radar_all_views(void);
/* 1 when a replay leaves the force feedback still: TNFS_REPLAY_NO_FFB=0/1 when set, else the mode's replay_no_ffb. */
int port_replay_no_ffb(void);
/* 1 when a race starts in the last camera used: TNFS_LAST_CAMERA=0/1 when set, else the mode's last_camera. */
int port_last_camera(void);
/* 1 when a lit brake light has its halo (lamp_flare.h): TNFS_LAMP_FLARES=0/1 when set, else the mode's lamp_flares; never without port_light_effects. */
int port_lamp_flares(void);

/* 1 when the scenery's open models draw from behind too: TNFS_TWO_SIDED=0/1 (dev) when set, else `automatic`: the race has a reversed track or the 360 camera
 * on (both show the scenery from behind), decided when the track's models are made. No port.cfg key (an old two_sided_scenery line is ignored). */
int port_two_sided_scenery(int automatic);
/* 1 when the L buildings (the two-sided boards) are closed into boxes (track_object_draw.h): TNFS_CLOSE_BOXES=0/1 (dev, for A/B) when set, else `automatic`, the same
 * condition as the open models drawn from behind: a reversed track or the 360 camera. No port.cfg key, no menu. */
int port_close_boxes(int automatic);

/* 1 when the rolling rally gravel strikes as the shoulder does (rally_gravel_shoulder): TNFS_RALLY_GRAVEL_SHOULDER=0/1 when set, else the mode's. */
int port_rally_gravel_shoulder(void);

/* 1 when close_houses is on: TNFS_CLOSE_HOUSES=0/1 when set, else the mode's; never without port_light_effects. */
int port_close_houses(void);
/* 1 when rain_fx is on: TNFS_RAIN_FX=0/1 when set, else the mode's. */
int port_rain_fx(void);
/* 1 when rain_grip is on: TNFS_RAIN_GRIP=0/1 when set, else the mode's. */
int port_rain_grip(void);
/* 1 when gauge_light is on: TNFS_GAUGE_LIGHT=0/1 when set, else the mode's. */
int port_gauge_light(void);

/* 1 when finish_gate_light is on: TNFS_FINISH_GATE_LIGHT=0/1 when set, else the mode's; only acts at night with the baked light (the light effects, the headlight beams with them). */
int port_finish_gate_light(void);
/* 1 when rain_lamps: TNFS_RAIN_LAMPS=0/1 when set, else the mode's; acts with weather=rain or storm. */
int port_rain_lamps(void);
/* 1 when gauge_exterior: TNFS_GAUGE_EXTERIOR=0/1 when set, else the mode's. */
int port_gauge_exterior(void);
/* 1 when sign_glow: TNFS_SIGN_GLOW=0/1 when set, else the mode's; never without port_light_effects(). */
int port_sign_glow(void);

/* 1 when analog_pad: TNFS_ANALOG_PAD=0..1 when set, else the mode's. 1 in every mode, Stock too
   (an analog pad is wanted there); 0 is the exe's key paths. */
int port_analog_pad(void);

/* 1 when rain_v2: TNFS_RAIN_V2=0..1 when set, else the mode's. */
int port_rain_v2(void);


/* 1 when rain_tyre_roll: TNFS_RAIN_TYRE_ROLL=0..1 when set, else the mode's. */
int port_rain_tyre_roll(void);
/* 1 when snow_weather: TNFS_SNOW_WEATHER=0..1 when set, else the mode's. */
int port_snow_weather(void);
/* 1 when reflectors: TNFS_REFLECTORS=0/1 when set, else the mode's; never without port_light_effects(). */
int port_reflectors(void);

/* 1 when rally_spray: TNFS_RALLY_SPRAY=0/1 when set, else the mode's. */
int port_rally_spray(void);

/* 1 when wipers: TNFS_WIPERS=0/1 when set, else the mode's. */
int port_wipers(void);

/* 0..2 weather_fog: TNFS_WEATHER_FOG=0..2 when set, else the mode's. */
int port_weather_fog(void);

/* 0/1 spray_v4: TNFS_SPRAY_V4=0/1 when set, else the mode's. */
int port_spray_v4(void);


/* 0..1 menu_pad_cancel: TNFS_MENU_PAD_CANCEL=0 or 1 when set, else the mode's. */
int port_menu_pad_cancel(void);
/* 0..2 tunnel_lamps: TNFS_TUNNEL_LAMPS=0, 1 or 2 when set, else the mode's; 1 at night only, 2 by day too. */
int port_tunnel_lamps(void);
/* 0..1 wrong_way: TNFS_WRONG_WAY=0 or 1 when set, else the mode's. */
int port_wrong_way(void);
/* 1 when snow_fx: TNFS_SNOW_FX=0/1 when set, else the mode's. */
int port_snow_fx(void);
/* 1 when lamp_halos: TNFS_LAMP_HALOS=0/1 when set, else the mode's. */
int port_lamp_halos(void);

/* 1 when handbrake_lamps: TNFS_HANDBRAKE_LAMPS=0/1 when set, else the mode's. The lamps only: the physics are the exe's either way. */
int port_handbrake_lamps(void);

/* port (replay conditions): while `on`, port_light_time() and port_weather() answer these (a replay plays with the conditions it was recorded with, not the settings'); the TNFS_LIGHT_TIME / TNFS_WEATHER variables still win. 0 gives them back. */
void port_conditions_force(int on, int light_time, int weather);


/* 1 when traffic_halo: TNFS_TRAFFIC_HALO=0..1 when set, else the mode's. */
int port_traffic_halo(void);

/* 1 when rally_dry_lamps: TNFS_RALLY_DRY_LAMPS=0..1 when set, else the mode's. */
int port_rally_dry_lamps(void);

/* 1 for a wet race: rain_grip on and the weather rain or storm (the grip is reduced and the race's records are not kept; track_gravity.h). */
int port_rain_grip_active(void);
/* 1 when reverse_rows is on: TNFS_REVERSE_ROWS=0/1 when set, else the mode's; never without port_light_effects. */
int port_reverse_rows(void);
/* The city_lift level (0..2): TNFS_CITY_LIFT=0..2 when set, else the mode's; 0 without port_light_effects. */
int port_city_lift(void);
/* The vegas_lift level (0..2): TNFS_VEGAS_LIFT=0..2 when set, else the mode's; 0 without port_light_effects. */
int port_vegas_lift(void);
/* 1 when brake_over_gas: TNFS_BRAKE_OVER_GAS=0/1 when set, else the mode's. */
int port_brake_over_gas(void);

/* how sky_fill fills the rows above the art (sky_stretch.h's SKY_FILL_*): TNFS_SKY_FILL_MODE=a|b (or 1|2) when set, else the default. */
enum { SKY_FILL_MODE_DEFAULT = 1 };
int port_sky_fill_mode(void);
/* The time of day (LIGHT_TIME_*): TNFS_LIGHT_TIME=dawn|day|dusk when set, else the mode's light_time. */
int port_light_time(void);
/* 1 when the masked lamp texels glow in low light (lamp_emissive.h): the Graphics Light Effects switch (port_light_effects), unless TNFS_EMISSIVE_LAMPS=0/1 is set. */
int port_emissive_lamps(void);

/* the cars' body shine, 0 (the exe's frames) or the envmap: TNFS_BODY_SHINE=0..4 when set (1..3 read as the envmap, as a file's), else the mode's body_shine. */
int port_body_shine(void);
/* The weather of the race: TNFS_WEATHER=clear|fog|rain|storm when set, else the mode's weather (a WEATHER_* of weather.h). */
int port_weather(void);

/* The mode port.cfg names (Stock without the line or the file). */
int port_settings_profile(void);
/* true when port.cfg names a mode: the player has chosen one */
bool port_settings_profile_chosen(void);
/* Makes `p` (a Profile) the mode in use, its own changes coming back with it: only the mode's name is written. False when the file does not take it. */
bool port_profile_select(int p);
/* 1 when `key` is one of the modes' settings or extras (a key the mode files hold), 0 for the machine's (profile, psx_dir, light_edit) and a key the game does not know */
int port_settings_mode_key(const char *key);
/* 0..1 light_edit: TNFS_LIGHT_EDIT (anything but "" or 0 is on) when set, else port.cfg's. */
int port_light_edit(void);
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
/* port: the keys that are a whole number in a range (name, where the int field is in PortSettings, lowest and highest value), for the test that walks them all */
typedef struct {
    const char *key;
    size_t offset;
    int min, max;
} PortSettingsKey;
const PortSettingsKey *port_settings_int_keys(int *count);

bool port_machine_save(int profile, const char *psx_dir);
/* the settings the mode `profile` would have (its values and its file's), whichever is in use: what applying it would change */
PortSettings port_settings_of_profile(int profile);
/* port (frame budget): the settings are read from the files once and kept; the writers here drop what is kept. A test that changes what the platform reads calls this. */
void port_settings_forget(void);
PortSettings port_settings_load(void);
/* the mode's name and folder as `s` has them, and in the file of `s`'s mode what differs from its values and is free in it. */
bool port_settings_save(const PortSettings *s);
/* The platform's config for the saved settings: the format's width by 480 lines, scaling, full screen, refresh. */
PlatformConfig port_platform_config(const char *title);

#endif

/* `tnfs --drive`: the race the menus and the command line start, on the ported race loop: the cars and specs loaded, the
 * race started (start_drive) and ended (drive_end), and the entries track_viewer.h gives the menus. Its frame is
 * drive_draw.c, the wide screen drive_wide.c, the bumper view drive_bumper_draw.c, its loop, window and screenshots
 * drive_run.c (drive_session.h holds what they share); `--track`'s fly-through is track_fly.c and a track's loading
 * track_load.c. */
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "game/race/car_load.h"
#include "game/race/single_rival.h"
#include "game/race/race_chase.h"
#include "game/race/race_detail.h"
#include "game/race/race_draw_setup.h"
#include "game/sys/port_extras.h"
#include "game/race/weather.h"
#include "game/race/race_replay_image.h"
#include "viewer/drive_slide.h"
#include "viewer/drive_joy.h"
#include "viewer/drive_smoke.h"
#include "game/sys/port_log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* port: the menus' (or the replay's) time of day for the track raced, 004fbae0[track], kept in a replay */
static int32_t drive_time_of_day = 1;

/* Drive mode (`tnfs --drive NAME [--car N]`): the ported race loop with
 * the follow camera (race_chase) on the player's car. Keys as the game's
 * defaults: arrows steer and work the pedals, A and Z change gear (an
 * automatic starts in neutral: A for drive, Z for reverse; the gears work
 * after 5 seconds), Space and H the 0x8000 / 0x4000 bits. The race's key
 * commands (004381f8, drive_keys): C the next view kind (0040594c; the race
 * turns to 13 while the car does not face the road), Alt + Ctrl + 1..8
 * another car, F1 the next view row, F2 the window size, F3 the detail,
 * F4 / F6 their flags, F5 the sound and music off and on. F9, a
 * development key standing in for the menus: the director's replay, and
 * during it back to the race (drive_replay.h). Headless, TNFS_REPLAY_AT and
 * TNFS_REPLAY_BACK press it on those frames. */
enum {
    DRIVE_VIEW = 5            /* the kind the record takes attached to a car (004f8020) */
};

RaceSim drive_sim;
DriveInput drive_input;
TrackViewerLink drive_link;  /* a link race's, taken by the next drive */
int drive_link_on;

DriveKeys drive_keys;
DriveReplay drive_replay;

DrivePause drive_pause;
DriveMenu drive_menu;

DriveCars drive_cars;
DriveHud drive_hud;
DriveBumper drive_bumper;  /* port: EXTRAS' Bumper Mirror / Radar, F1 / F2 in a race */
DriveMirror bumper_mirror;  /* port: the bumper view's mirror, the same for every car (drive_mirror_start_own) */
static DriveSmoke drive_smoke;

/* 00405210 for the race (race->select_view): the camera takes its view's
 * layout for the window size. */
static void select_view(void *user, RaceCamera *camera, int32_t view, int32_t size)
{
    race_view_select(user, camera, view, size);
}

/* port: an environment switch for the checks (1 / 0), else `otherwise` */
static int env_switch(const char *name, int otherwise)
{
    const char *v = getenv(name);
    return v != NULL ? atoi(v) != 0 : otherwise;
}

/* port: the menu's Ultra, TNFS_VIEW_ULTRA overriding port.cfg's view_ultra for the checks */
static int view_ultra_on(const PortSettings *ps) { return env_switch("TNFS_VIEW_ULTRA", ps->view_ultra); }

/* port: the race about to start: its 640 x 480 rows without the strip in every format, the field's adjustment, and
 * 640 wide until drive_wide_size */
static void drive_wide_setup(void)
{
    const PortSettings ps = port_settings_load();
    screen_layouts(SCREEN_W, drive_views.layouts[HIGH_SCREEN]);
    drive_wide.fov_adjust = ps.fov_adjust;
    /* port: the "ultra" view distance (TNFS_VIEW_ULTRA=1 / 0 overrides port.cfg for the checks) and F3's rotation */
    const int ultra = view_ultra_on(&ps);
    const int f3 = env_switch("TNFS_F3_ULTRA", ps.f3_ultra);
    drive_keys_port(f3, ultra);
    ported_view_set_reach(ultra ? DRAW_SEGMENTS_ULTRA : 0);
    /* port: Perspective Correction, Texture Filter and Fog live on the Graphics screen now and are read from port.cfg
     * (port_settings_load still falls back to extras.cfg for a file written before they moved). The TNFS_MODERN_3D,
     * TNFS_TEXTURE_FILTER and TNFS_FOG env vars override them for the checks. */
    const PortExtras extras = port_extras_load();
    const int weather = port_weather();   /* port: weather=fog needs the per-pixel fog of Modern 3D, so it turns Modern 3D on */
    ported_view_set_modern(env_switch("TNFS_MODERN_3D", ps.perspective) || weather == WEATHER_FOG);
    ported_view_set_weather(weather);
    ported_view_set_look(env_switch("TNFS_TEXTURE_FILTER", ps.texture_filter), env_switch("TNFS_FOG", ps.fog));
    drive_bumper_start(&drive_bumper, env_switch("TNFS_BUMPER_MIRROR", extras.bumper_mirror),
                       env_switch("TNFS_BUMPER_RADAR", extras.bumper_radar));
    drive_mirror_free(&bumper_mirror);  /* the last track's horizon with it */
    drive_mirror_start_own(&bumper_mirror, DRIVE_BUMPER_MIRROR_PERCENT);
    race_detail_set_ceiling(ultra || f3 ? DRAW_SEGMENTS_ULTRA : 0);
    drive_wide.width = SCREEN_W;
    drive_wide.ready = 0;
}

enum {
    HEAD_TO_HEAD = 1,       /* RaceField.race_type */
    SINGLE_RACE = 2,
    DRIVE_SEED = 0x2b67,    /* 004fb7f4's stand-in for --drive without the menus */
    REPLAY_KIND = 1,        /* 00524e9f for a replay played back (0042c498's 1..3): any set */
    WARRIOR_CAR = 11        /* the secret car (CONCEPTCAR), "Traffc" */
};

static int32_t le32_at(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static void car_stem(int model, char stem[16])
{
    memset(stem, 0, 16);
    for (int i = 0; car_model_names[model][i] != '\0' && i < 15; i++)
        stem[i] = (char)toupper((unsigned char)car_model_names[model][i]);
}

/* A file the original names in mixed case ("TSupra.cfm"); the CD's are
 * upper case, so a case-sensitive file system gets a second try. */
static uint8_t *read_named(const char *path, size_t dir_length, size_t *size)
{
    uint8_t *file = platform_read_file(path, size);
    if (file != NULL) return file;
    char upper[CAR_LOAD_PATH];
    snprintf(upper, sizeof upper, "%s", path);
    for (size_t i = dir_length; upper[i] != '\0'; i++) upper[i] = (char)toupper((unsigned char)upper[i]);
    return platform_read_file(upper, size);
}

static int load_pdn(const char *path, size_t dir_length, CarPdn *pdn)
{
    size_t size = 0;
    uint8_t *file = read_named(path, dir_length, &size);
    const int ok = file != NULL && car_pdn_read(pdn, file, size);
    platform_free_file(file);
    if (!ok) port_log("track_viewer: no body %s\n", path);
    return ok;
}

/* RaceField.load: 00457d40 for the slot (the CFM its model names, read
 * here), its objects to the view, then its PDN. */
/* 004571e8's picture on the front end's screen */
static void drive_slide(void *user, int32_t picture, int32_t car)
{
    (void)user;
    drive_slide_show(car, picture);
}

static const CarPdn *drive_load(void *user, Car *car, RaceState *race)
{
    (void)user;
    const int slot = car->index;
    if (slot < 0 || slot >= RACE_CARS) return NULL;
    char dir[CAR_LOAD_PATH], path[CAR_LOAD_PATH], stem[16];
    snprintf(dir, sizeof dir, "%s/SIMDATA/CARFAMS/", drive_cars.data_root);
    if (!car_load_path(car, race, dir, path, stem)) return NULL;
    size_t size = 0;
    drive_cars.file[slot] = read_named(path, strlen(dir), &size);
    const CarLoadResult r = drive_cars.file[slot] == NULL ? CAR_LOAD_FILE
        : car_load_setup(car, race, &drive_cars.tint, NULL, &drive_cars.load[slot], drive_cars.file[slot], size);
    drive_cars.loaded[slot] = r == CAR_LOAD_OK;
    if (!drive_cars.loaded[slot]) {
        port_log("track_viewer: car %s not loaded (%d)\n", path, (int)r);
        return NULL;
    }
    car->body_object = (uint32_t)slot;  /* bind_wheels' token for the slot's object */
    ported_view_set_car_objects(slot, &drive_cars.load[slot].object[0], &drive_cars.load[slot].object[1]);
    car_load_pdn_path(dir, stem, path);
    return load_pdn(path, strlen(dir), &drive_cars.pdns[slot]) ? &drive_cars.pdns[slot] : NULL;
}

/* Slot 0 the human's `model`, then the other race models (0..7) as
 * computer racers (a single race, as test_race runs it); racers 1 drives
 * alone. race_type 0..3: the cars 0042702c sets for it, the police and
 * traffic (slots past the racers) by their drawn models. */
TrackViewerMenus drive_menus;
int drive_menus_on;
static const ReplayRace *replay_of;   /* the replay played back, its file replay_file; NULL a race */
static const uint8_t *replay_file;
static int replay_request;            /* 0044f534's: 4 or 5 */
TrackViewerResults drive_results;

TrackViewerResults track_viewer_drive_results(void) { return drive_results; }

void track_viewer_drive_menus(const TrackViewerMenus *m)
{
    drive_menus = *m;
    drive_menus_on = 1;
}

void track_viewer_drive_link(const TrackViewerLink *l)
{
    drive_link = *l;
    drive_link_on = l->humans >= 1 && l->humans <= RACE_PLAYERS && l->slot >= 0 && l->slot < l->humans;
}

/* 004275f8 for a replay: 0052b316, 0044f528 and 0044f534(4); the cars' models 00457870 drew against the file's */
static void replay_begin(void)
{
    enum { MODELS_AT = 0x2d8 };
    RaceState *race = &drive_sim.race;
    drive_replay.file = replay_file;
    race->replay = 1;
    race_replay_end_clear(&drive_replay.dir);
    race_replay_request(&drive_replay.dir, replay_request);
    for (int i = 0; i < RACE_CARS; i++) {
        const uint8_t *m = replay_file + MODELS_AT + 4 * i;
        const int32_t model = (int32_t)(m[0] | m[1] << 8 | m[2] << 16 | (uint32_t)m[3] << 24);
        if (race->models[i] != model)
            port_log("track_viewer: the replay's slot %d model %d, drawn %d\n", i, model, race->models[i]);
    }
}

/* A car model's specs (its PBS) */
static int load_specs(const char *data_root, int model, CarSpecs *specs)
{
    char stem[16];
    car_stem(model, stem);
    char path[512];
    snprintf(path, sizeof path, "%s/SIMDATA/CARSPECS/%s.PBS", data_root, stem);
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    const int ok = file != NULL && car_specs_read(specs, file, size);
    platform_free_file(file);
    if (!ok) port_log("track_viewer: no specs %s\n", path);
    return ok;
}

/* 00440afc's 0041019c: a link player still on the start line is stood in */
static void race_stand_in(void *user, int32_t player) { (void)user, drive_link_stand_in((int16_t)player); }

int32_t link_wait_ticks;  /* 004fbac8: 00410d80's, a link race's queue wait */

/* 004273d0 in a link game: this machine's entry (its car and aids, as a solo race's first) out, every slot's back */
static const uint8_t *link_entries(int model, uint32_t *seed, uint32_t *flags)
{
    RaceEntry own;
    memset(&own, 0, sizeof own);
    own.car = model, own.automatic = 1;
    if (drive_menus_on) own.automatic = drive_menus.automatic, own.anti_lock = drive_menus.anti_lock, own.traction = drive_menus.traction;
    return drive_link.exchange(&own, drive_menus_on ? drive_menus.game_state : NULL, &link_wait_ticks, seed, flags);
}

static int start_drive(Track *t, const char *data_root, const char *name, int model, int racers, int race_type)
{
    static CarSpecs human_specs[RACE_PLAYERS];  /* a link race's other humans' */
    /* 004fb7f4: 004275f8's time(0), the race's random seed (00428fe0); --drive alone keeps a fixed one */
    uint32_t seed = drive_menus_on ? (uint32_t)time(NULL) : DRIVE_SEED;
    uint32_t flags = drive_menus_on ? drive_menus.track_flags : 0;  /* 004fb7d8: 0042702c's copy of 0052b306 */
    const uint8_t *entries = drive_link_on ? link_entries(model, &seed, &flags) : NULL;  /* the host's setup's, 004fb7fa */
    if (entries != NULL) memcpy(&model, entries + offsetof(RaceEntry, car), sizeof model);  /* slot 0's specs: the host's car's */
    CarSpecs specs;
    if (!load_specs(data_root, model, &specs)) return 0;
    int own = model;  /* the dash (0041f9a0) is this machine's car's: in a link race its slot's in the setup */
    if (entries != NULL) memcpy(&own, entries + sizeof(RaceEntry) * (size_t)drive_link.slot + offsetof(RaceEntry, car), sizeof own);
    char stem[16];
    car_stem(own, stem);
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = racers;
    field.race_type = racers > 1 ? SINGLE_RACE : 0;
    if (race_type >= 0) {
        field.race_type = race_type;
        race_field_counts(&field, race_sim_track_index(name), flags, drive_link_on);  /* 0052b312: no police in a link game */
        racers = field.racers;
    }
    field.seed = seed;
    field.track_flags = flags;
    field.mirrored = drive_menus_on ? drive_menus.variant_m != 0 : 0;  /* 004fbad8 / 004fbadc: the cheats' and track select's */
    int arcade = drive_menus_on ? drive_menus.variant_r != 0 : 0;
    field.load = drive_load;
    field.slide = drive_menus_on ? drive_slide : NULL;  /* the front end's screen is up only with the menus */
    drive_cars.data_root = data_root;
    drive_cars.tint.tint = (RacePaletteTint){ 0x10000, 0x100, 0, 0x100, 0x100 };
    drive_cars.tint.tint_ready = 1;
    for (int i = 0, next = 0; i < racers; i++) {
        if (i > 0 && next == model) next++;
        const int m = i == 0 ? model : next++;  /* the others: the 8 race models in order */
        if (i == 0) snprintf(field.entries[i].name, sizeof field.entries[i].name, "%s",
                             drive_menus_on && drive_menus.name != NULL ? drive_menus.name : "Player");
        else snprintf(field.entries[i].name, sizeof field.entries[i].name, "%s", car_model_names[m]);
        field.entries[i].car = m;
        field.entries[i].automatic = 1;
    }
    if (drive_menus_on) {
        field.entries[0].automatic = drive_menus.automatic, field.entries[0].anti_lock = drive_menus.anti_lock;
        field.entries[0].traction = drive_menus.traction;
        if (field.race_type == HEAD_TO_HEAD && racers > 1) field.entries[1].car = drive_menus.opponent;
    }
    /* port, for the checks: TNFS_OPPONENT=<car 0..7, 11> the head to head opponent without the menus */
    const char *opponent = getenv("TNFS_OPPONENT");
    if (!drive_menus_on && opponent != NULL && field.race_type == HEAD_TO_HEAD && racers > 1) {
        const int car = atoi(opponent);
        if ((car >= 0 && car <= 7) || car == WARRIOR_CAR) field.entries[1].car = car;
    }
    /* A single race with the other entry's car 8 gets 00457870's field of eight different cars (004c5628); any other
     * car there races seven copies of it. The menus' pick (0042702c's second record) is that entry; --drive without
     * them stands in with the whole field. */
    if (field.race_type == SINGLE_RACE && racers > 1 && !drive_link_on)
        field.entries[1].car = single_rival_car(drive_menus_on, drive_menus.opponent);
    for (int i = 0; entries != NULL && i < drive_link.humans && i < racers; i++) {  /* the machines' entries */
        memcpy(&field.entries[i], entries + sizeof(RaceEntry) * (size_t)i, sizeof field.entries[i]);
        if (i > 0 && !load_specs(data_root, field.entries[i].car, &human_specs[i])) return 0;
        field.specs[i] = i > 0 ? &human_specs[i] : NULL;
    }
    if (drive_link_on) field.humans = drive_link.humans;
    if (replay_of != NULL) {  /* the replay's own field (0044d3b0's setup), with its seed and its humans' specs */
        field = replay_of->field;
        field.track_flags = replay_of->track_flags;
        arcade = replay_of->arcade;
        field.load = drive_load;
        field.slide = drive_menus_on ? drive_slide : NULL;
        for (int i = 1; i < field.humans && i < RACE_PLAYERS; i++) {
            if (!load_specs(data_root, field.entries[i].car, &human_specs[i])) return 0;
            field.specs[i] = &human_specs[i];
        }
    }
    const int laps = replay_of != NULL ? replay_of->laps : drive_menus_on && drive_menus.laps > 0 ? drive_menus.laps : 2;
    race_sim_start_field(&drive_sim, &t->tri, name, &specs, &field, laps, arcade);
    if (drive_menus_on && replay_of == NULL) {  /* the cheats' words, shared with the menus in the original */
        drive_sim.race.unknown_4c5cd4 = drive_menus.explode;
        drive_sim.race.start_input = drive_menus.start_boost;
        drive_sim.race.port_replay_cameras = drive_menus.replay_cameras;
    }
    if (replay_of != NULL) race_replay_apply_cheats(&drive_sim.race, replay_of);  /* port: the recording's cheat, or the horn's spin-outs are missing */
    drive_sim.race.port_replay_launch = 1;  /* port: and the NITRO start is recorded in the replay and played back */
    drive_sim.race.port_nitro_start = 1;  /* port: the NITRO start takes N alone or the pad's key (start_keys.h) */
    drive_sim.race.port_pad_and_keys = 1;  /* port: the keys and the pad are both live (input_read.c), not one device per action */
    drive_sim.race.port_stop_replaced = 1;  /* port: a horn's looping voice is cut when a tyre or crash sound takes its channel */
    drive_sim.race.port_time_of_day = drive_time_of_day;  /* the replay's setup keeps it */
    /* config.dat's words the race reads in place: 00524e85 the track, 00524e9f the replay's kind (0 a race; set, the
     * results and records are left alone, 0045af80) with 00524ea1 the laps above it */
    drive_sim.race.frontend_track = drive_menus_on && replay_of == NULL ? drive_menus.track : race_sim_track_index(name);
    drive_sim.race.frontend_laps_word = (int32_t)((uint32_t)(uint16_t)laps << 16) | (replay_of != NULL ? REPLAY_KIND : 0);
    if (drive_link_on) {  /* 004275f8 in a link game: 0052b312 1, this machine's car followed and viewed, its side */
        RaceState *race = &drive_sim.race;
        race->playback = 1, race->player = race->viewed = drive_link.slot;
        race->unknown_4fbb0c = drive_link.host, race->unknown_4fb7d4 = 1;
    }
    drive_joy_install(&drive_sim.race);
    if (drive_menus_on && drive_menus.game_state != NULL)
        memcpy(drive_sim.race.game_state, drive_menus.game_state, sizeof drive_sim.race.game_state);
    drive_results = (TrackViewerResults){ 0 };
    drive_input_start(&drive_input, &drive_sim.race);
    /* 004275f8: the player's controls (00426ddc's, drive_input_start's above) go into its entry (0042702c's, at
     * local_39, which 004273d0 copies into the setup the replay keeps); then each human's come back out of the
     * entries, a replay's from its file (the CD's demos were driven on a pad). A link race's entries are the
     * machines' exchange, left as they were. */
    if (!drive_link_on) {
        if (replay_of == NULL) drive_sim.race.entries[0].controls = drive_sim.race.controls[0];
        race_controls_from_entries(&drive_sim.race);
    }
    drive_input.hud = &drive_hud.hud;
    drive_input.own_specs = &drive_sim.specs[drive_sim.race.player];  /* port: test harness only (drive_autopilot) */
    if (drive_link_on) {
        drive_input.link = &drive_link, drive_input.stand_in_block = drive_link.stand_ins;
        for (int i = 0; i < drive_link.humans; i++) drive_input.queues[i].active = 1;
        drive_link.racing(drive_input.queues, drive_sim.race.players, drive_link_stand_in);
        drive_sim.race.sounds.start_line = race_stand_in;
    }
    drive_keys_start(&drive_keys);
    drive_pause_start(&drive_pause);
    memcpy(drive_views.layouts, race_view_layouts, sizeof drive_views.layouts);
    memcpy(drive_views.rows, race_view_rows, sizeof drive_views.rows);
    drive_views.mode = HIGH_SCREEN;
    drive_wide_setup();
    drive_sim.race.select_view = select_view;
    drive_sim.race.view_user = &drive_views;
    drive_sim.race.window_size = 0;
    race_chase_follow(&drive_sim.race, &drive_sim.race.chase, drive_sim.race.cars[drive_link_on ? drive_link.slot : 0], DRIVE_VIEW);
    if (drive_link_on) {  /* a link game records none (0044edd8 with 0052b312 1): 004fbb0c stays the link's */
        drive_replay_start(&drive_replay, &drive_sim, &drive_views);
        drive_sim.race.unknown_4fbb0c = drive_link.host;
    } else if (!drive_replay_start(&drive_replay, &drive_sim, &drive_views)) {
        port_log("track_viewer: no replay recording\n");
    }
    drive_replay.seed = field.seed;
    if (replay_of != NULL) replay_begin();
    /* 0044cc74's words before any frame; race_run's 0044d2c4 (drive_frame_setup) sets the race's start on them */
    drive_frame = (FrameDraw){ 0 };
    if (drive_menus_on && drive_menus.options != NULL) track_viewer_options_from_settings(drive_menus.options);
    drive_frame.pending_segments = -1;
    drive_frame.mirror_count = DRAW_SETUP_MIRROR_COUNT;
    drive_cars.ended = 0;
    static const char *const CARPAL[] = { ".FSH", ".fsh", NULL };
    size_t carpal_size = 0;
    drive_cars.carpal = track_viewer_read_any(data_root, "CARFAMS", "CARPAL", CARPAL, &carpal_size);
    CarLoad *cars[RACE_CARS] = { 0 };
    for (int i = 0; i < RACE_CARS; i++) cars[i] = drive_cars.loaded[i] ? &drive_cars.load[i] : NULL;
    if (!drive_hud_read(&drive_hud, data_root)) port_log("track_viewer: no HUD\n");
    drive_hud.port_grab = drive_wide_map_grab;
    drive_hud.director = drive_replay.recording != NULL ? &drive_replay.dir : NULL;  /* the dash controls' words */
    drive_replay.hud = &drive_hud.hud;
    drive_replay.key_flags = drive_input.keys;  /* 0044f55c reads 004c693c */
    drive_replay.dash = &drive_hud.art;
    drive_replay.panel = &drive_hud.panel, drive_replay.replay_art = &drive_hud.replay;  /* 00471460 */
    /* 0041f9a0 from 004400f4, before 00455a5c builds the palette (0041f818 maps the dash onto it) */
    drive_hud_cockpit(&drive_hud, &drive_sim.race, data_root, stem);
    /* 00456a7c from 00455a5c (after the cars' load), its bank and 00456948 at 00447098's end */
    drive_smoke_setup(&drive_smoke, data_root, &drive_sim.race.trail);
    const DrivePaletteLate late = { &drive_smoke, drive_smoke_palette, drive_smoke_blend };
    if (!drive_palette_build(&drive_sim.race, &t->fam, cars, drive_cars.carpal, &drive_hud, &late, &t->tint, t->art.palette))
        port_log("track_viewer: the race palette was not built; the track's own is used\n");
    if (drive_smoke.ready) ported_view_set_puffs(&drive_smoke.art, &drive_smoke.blend, drive_draw_rand, NULL, &drive_sim.race.trail);
    return 1;
}

static void free_drive(void)
{
    for (int i = 0; i < RACE_CARS; i++) {
        ported_view_set_car_slot(i, NULL, NULL);
        car_load_free(&drive_cars.load[i]);
        drive_cars.loaded[i] = 0;
        platform_free_file(drive_cars.file[i]);
        drive_cars.file[i] = NULL;
    }
    platform_free_file(drive_cars.carpal);
    drive_cars.carpal = NULL;
    drive_hud.director = NULL;
    drive_replay_end(&drive_replay);
    drive_hud_end(&drive_hud, &drive_sim.race);
    drive_hud_free(&drive_hud);
    ported_view_set_puffs(NULL, NULL, NULL, NULL, NULL);
    drive_smoke_free(&drive_smoke);
}

/* The race's end (once): 004275f8 runs 0045af80 after race_run however it
 * stopped, a game given up from the pause menu too; the results printed. */
/* 004275f8's option bytes and the words they come from and go back to, a short each from 00524e71 */
enum { OPT_WINDOW, OPT_IMAGE, OPT_VIEW, OPT_CARS, OPT_HORIZON, OPTS };
enum { GAME_KEPT_OPTIONS = 0x520 };  /* 0052b264 less the game state 0052ad44 */

void track_viewer_options_from_settings(const uint8_t *options)
{
    drive_sim.race.window_size = options[2 * OPT_WINDOW];
    drive_keys.state.screen_kind = options[2 * OPT_IMAGE];
    drive_keys.state.detail = options[2 * OPT_VIEW];
    drive_frame.alternate = options[2 * OPT_CARS];
    drive_keys.state.w4fb786 = options[2 * OPT_HORIZON];
}

/* After the race, and when its menu opens: 004054cc's size, 004fb789, 785, 787 and 786 as shorts */
void track_viewer_options_to_settings(uint8_t *options)
{
    const uint8_t bytes[OPTS] = { (uint8_t)drive_sim.race.window_size, drive_keys.state.screen_kind,
                                  drive_keys.state.detail, drive_frame.alternate, drive_keys.state.w4fb786 };
    for (int k = 0; k < OPTS; k++) options[2 * k] = bytes[k], options[2 * k + 1] = 0;
}

/* port: the Graphics screen's settings the race takes when the pause menu's Graphics closes (drive_menu.c). Perspective
 * correction, texture filter and fog, the field, and the refresh rate are read here, and the view distance's segments
 * are asked for through the pause's pending detail, which the same frame takes (drive_run.c). Scaling, the format,
 * full screen and Ultra wait for the next race. `options` are the race's option bytes, as the menu writes them. */
void track_viewer_graphics_close(const uint8_t *options)
{
    const PortSettings ps = port_settings_load();
    drive_wide.fov_adjust = ps.fov_adjust;
    const int weather = port_weather();   /* port: weather=fog needs the per-pixel fog of Modern 3D, so it turns Modern 3D on */
    ported_view_set_modern(env_switch("TNFS_MODERN_3D", ps.perspective) || weather == WEATHER_FOG);
    ported_view_set_weather(weather);
    ported_view_set_look(env_switch("TNFS_TEXTURE_FILTER", ps.texture_filter), env_switch("TNFS_FOG", ps.fog));
    const char *hz = getenv("TNFS_REFRESH_HZ");  /* as the race's start reads it */
    drive_run_refresh_set(hz != NULL && atoi(hz) >= 30 && atoi(hz) <= 240 ? atoi(hz) : ps.refresh_hz);
    drive_keys.state.detail = options[2 * OPT_VIEW];
    /* the menu's Ultra is its View at Far with port.cfg's view_ultra on: the race takes it at once */
    drive_keys.state.ultra = view_ultra_on(&ps) && drive_keys.state.detail == 0;
    const int32_t segments = drive_keys.state.ultra ? DRAW_SEGMENTS_ULTRA : race_draw_detail_segments(drive_keys.state.detail);
    if (segments != -1) race_detail_request(segments, &drive_keys.detail_pending);
}

void drive_end(void)
{
    if (drive_cars.ended) return;
    race_sim_end(&drive_sim);
    drive_cars.ended = 1;
    const RaceState *race = &drive_sim.race;
    drive_results = (TrackViewerResults){ 1, (int16_t)race->end_request, race->results_player_rank, race->results_rival_rank,
                                    race->results_rival, race->results_rival_name, { 0 }, race->results_player_place,
                                    race->results_shown_place, race->racers, race->player, race->end_state };
    memcpy(drive_results.dropped, race->unknown_4f82a0, sizeof drive_results.dropped);
    if (drive_menus_on && drive_menus.game_state != NULL)
        memcpy(drive_menus.game_state, race->game_state, sizeof race->game_state);
    if (drive_menus_on && drive_menus.options != NULL) track_viewer_options_to_settings(drive_menus.options);
    /* 004275f8 hands them back at 0052b264.., the kept copy of the settings in the game state (+0x4a8, their +0x78),
     * which the menus' race puts back over the image (00427ae0) */
    if (drive_menus_on && drive_menus.game_state != NULL) track_viewer_options_to_settings(drive_menus.game_state + GAME_KEPT_OPTIONS);
    port_out("race over (%d):", drive_sim.race.end_request);
    for (int i = 0, place = 0; i < drive_sim.count; i++) {  /* the racers only, not the traffic or police */
        const int slot = drive_sim.race.results_order[i];
        if (slot >= drive_sim.race.racers) continue;
        const Car *c = slot >= 0 ? drive_sim.race.cars[slot] : NULL;
        port_out(" %d.%s", ++place, c == NULL ? "?" : slot == 0 ? "Player" : car_model_names[c->model & 7]);
    }
    port_out("\n");
    /* port, for the checks: each racer's model and finish (the tick - 300, 0 still racing; 00440c30) */
    port_out("finish:");
    for (int slot = 0; slot < drive_sim.race.racers && slot < RACE_PLAYERS; slot++) {
        const Car *c = drive_sim.race.cars[slot];
        port_out(" %d=%s:%d", slot, c != NULL && c->model >= 0 && c->model < CAR_MODELS ? car_model_names[c->model] : "?",
               drive_sim.race.players[slot].finish);
    }
    port_out("\n");
}

int track_viewer_drive(const char *data_root, const char *name, int model, int racers, int race_type, uint32_t ticks,
                       const char *screenshot)
{
    drive_time_of_day = 1;
    static Track t;
    /* 004278c0: 004fa0ec 1 with a variant, else the track's time of day (004fbae0[004fb7c2]): a replay's from its
     * setup (004fbad8 +0x31c, 004fbadc +0x320, 004fbae0 +0x324, 004fb7c2 +6), the menus'; else 0 and no variant */
    TrackFamInputs look = { .time_of_day = 0, .variant_m = 0, .variant_r = 0, .track = race_sim_track_index(name),
                            .stem = NULL, .dir = NULL, .dir_midday = NULL, .port_both_variants = 0 };
    look.port_lunar = port_extras_load().lunar_springs;   /* port: the moon's textures for the rally on Rusty Springs */
    if (replay_file != NULL) {
        const int32_t track = le32_at(replay_file + 6);
        look.variant_m = le32_at(replay_file + 0x31c), look.variant_r = le32_at(replay_file + 0x320), look.track = track;
        /* port: the replay's art as its race had it: both variants together, and the time of day with a variant
         * (extras.cfg's variant_time), each where the exe takes the mirror's art and midday */
        look.port_both_variants = look.variant_m != 0 && look.variant_r != 0;
        const int32_t time = track >= 0 && track < 9 ? le32_at(replay_file + 0x324 + 4 * track) : 1;  /* port guard */
        drive_time_of_day = time;
        look.time_of_day = track_fam_time_of(track, look.variant_m, look.variant_r, port_extras_load().variant_time,
                                             time);
    } else if (drive_menus_on) {
        look.variant_m = drive_menus.variant_m, look.variant_r = drive_menus.variant_r;
        /* port: the menus' own pair is the combined state when both are on — the race reads the rally's FAM, the
         * mirror keeping the geometry (variant_rule.h's rule). Asking extras.cfg instead would miss a race the
         * frontend reached with both on but no extras.cfg on disk, which is every --drive NAME mr run.
         * A replay takes the same rule on its own pair (above). */
        look.port_both_variants = drive_menus.variant_m != 0 && drive_menus.variant_r != 0;
        /* port: extras.cfg's variant_time keeps the time of day with a variant (fam_mirror.h makes its art) */
        look.time_of_day = track_fam_time_of(drive_menus.track, drive_menus.variant_m, drive_menus.variant_r,
                                             port_extras_load().variant_time, drive_menus.time_of_day);
        drive_time_of_day = drive_menus.time_of_day;
    } else if (track_viewer_fam_missing(data_root, name, &look)) {
        look.time_of_day = 1;
    }
    int ok = model >= 0 && model < CAR_MODELS && racers >= 1 && racers <= RACE_PLAYERS && race_type <= 3 &&
             track_viewer_load(&t, data_root, name, &look);
    ok = ok && start_drive(&t, data_root, name, model, racers, race_type);
    uint8_t *pixels = ok ? malloc(SCREEN_W * SCREEN_H) : NULL;
    ok = ok && pixels != NULL;
    if (ok && screenshot != NULL && getenv("TNFS_RUN") != NULL)
        ok = drive_run_screenshot(&t, pixels, ticks, screenshot);
    else if (ok && screenshot != NULL)
        ok = (race_sim_begin(&drive_sim), drive_screenshot(&t, pixels, ticks, screenshot));
    else if (ok)
        ok = drive_window(&t, pixels);
    free(pixels);
    free_drive();
    track_viewer_free(&t);
    if (drive_link_on) drive_link.racing(NULL, NULL, NULL);
    drive_menus_on = 0, drive_link_on = 0;
    drive_input.link = NULL, drive_input.stand_in_block = drive_input.own_stand_ins;
    return ok;
}

int track_viewer_drive_replay_data(const char *data_root, const uint8_t *file, size_t size, int request)
{
    static ReplayRace race;
    if (file == NULL || size != REPLAY_FILE_SIZE || !race_replay_image_race(file, &race)) {
        port_log("track_viewer: not a replay\n");
        drive_menus_on = 0;
        return 0;
    }
    replay_of = &race, replay_file = file, replay_request = request;
    const int ok = track_viewer_drive(data_root, race.stem, race.field.entries[0].car, race.field.racers, -1, 0, NULL);
    replay_of = NULL, replay_file = NULL;
    return ok;
}

int track_viewer_drive_replay(const char *data_root, const char *path)
{
    enum { FROM_START = 4 };
    /* port, for the checks: TNFS_REPLAY_REQUEST=5 / 8 plays it as the menus' director (8 the intro's) does */
    const char *asked = getenv("TNFS_REPLAY_REQUEST");
    const int request = asked != NULL && (atoi(asked) == 5 || atoi(asked) == 8) ? atoi(asked) : FROM_START;
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    const int ok = track_viewer_drive_replay_data(data_root, file, size, request);
    platform_free_file(file);
    return ok;
}

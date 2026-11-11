/* `tnfs --drive`'s cars: the car files read and made up for their slot (livery, paint, lamps, shine), the specs, the rivals' colours and the player's dash paint. Split from track_viewer.c. */
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "viewer/shine_class.h"
#include "game/race/car_load.h"
#include "game/race/single_rival.h"
#include "game/race/race_chase.h"
#include "game/race/race_detail.h"
#include "game/race/race_draw_setup.h"
#include "game/sys/port_extras.h"
#include "game/sys/free_laps.h"
#include "game/race/track_wall.h"
#include "game/sys/reverse_track.h"
#include "game/race/race_schedule.h"
#include "game/sys/port_settings.h"
#include "game/race/car_livery.h"
#include "game/race/car_paint.h"
#include "viewer/drive_livery.h"
#include "viewer/drive_replay.h"
#include "viewer/drive_light.h"
#include "viewer/drive_lamps.h"
#include "game/race/weather.h"
#include "game/sys/bands.h"
#include "game/race/race_replay_image.h"
#include "viewer/drive_slide.h"
#include "viewer/drive_joy.h"
#include "viewer/drive_smoke.h"
#include "game/sys/port_log.h"
#include "game/race/lunar_springs.h"
#include "game/race/session_rules.h"
#include "game/sys/extras_marks.h"
#include "game/race/difficulty.h"
#include "game/race/rally_gravel.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "game/viewer/scenery_dump.h"
#include "game/race/wet_zones.h"
#include "game/race/weather_snow.h"
#include <string.h>
#include <time.h>
#include "viewer/track_viewer_cars.h"

int32_t le32_at(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

void car_stem(int model, char stem[16])
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
void drive_slide(void *user, int32_t picture, int32_t car)
{
    (void)user;
    drive_slide_show(car, picture);
}

/* the draft's class of each texel of a body texture, kept by the address of the texels the car's object will read */
static void shine_register(void *user, int level, const char name[4], int width, int height, const uint8_t *pixels, const uint8_t *keep,
                           const uint8_t rgb[768])
{
    (void)level, (void)name, (void)rgb;
    shine_class_add(*(const int *)user, pixels, width, height, keep);
}

/* port: the slot's car file as the livery, paint and lamps masks leave it, before the loader reads it. */
static void drive_load_textures(const RaceState *race, int slot, const char *stem, size_t size)
{
    shine_class_drop_slot(slot);
    if (drive_cars.file[slot] != NULL && port_body_shine()) {   /* port (body shine): the paint texels, from the colours as the file has them */
        int owner = slot;
        drive_livery_draft(stem, drive_cars.file[slot], size, shine_register, &owner);   /* (the file's own stem: drive_cars.model is only set with extra_liveries) */
    }
    if (drive_cars.file[slot] != NULL && drive_cars.fixed_black[slot]) {   /* port: a mask's fixed black texels take the car's black, before any paint or livery */
        const CarPaint *black = car_paint_black_of(stem);
        if (black != NULL) {
            const int n = drive_livery_black(stem, drive_cars.file[slot], size, black->rgb);
            if (n > 0) port_log_detail("car_paint: slot %d %s fixed black, %d texels\n", slot, stem, n);
        }
    }
    if (drive_cars.file[slot] != NULL && drive_cars.livery[slot]) {  /* port: the rival's console livery, in its own CFM */
        const int player = slot == race_slot(race->player);
        uint8_t main_rgb[3] = { 0 };
        const int count = drive_livery_car(drive_cars.psx_dir, stem, drive_cars.file[slot], size, player ? main_rgb : NULL);
        if (player && count > 0) memcpy(drive_cars.dash_rgb, main_rgb, 3), drive_cars.dash_rgb_on = 1;
        if (count > 0) port_log("extra_liveries: slot %d %s in the alternate livery (%d colours)\n", slot, stem, count);
    }
    if (drive_cars.file[slot] != NULL && drive_cars.paint_on[slot]) {  /* port: a catalogue paint, in the body textures only */
        const int n = drive_livery_paint(car_model_names[drive_cars.model[slot]], drive_cars.file[slot], size, drive_cars.paint[slot]);
        port_log_detail("car_paint: slot %d %s painted %d,%d,%d (%d body texels)\n", slot, stem, drive_cars.paint[slot][0],
                 drive_cars.paint[slot][1], drive_cars.paint[slot][2], n);
    }
    drive_lamps_clear(slot);   /* port (emissive lamps): the texels the masks mark as lamps, before the loader reads the file */
    if (drive_cars.file[slot] != NULL) drive_lamps_find(slot, stem, drive_cars.file[slot], size);
}

const CarPdn *drive_load(void *user, Car *car, RaceState *race)
{
    (void)user;
    const int slot = car->index;
    if (slot < 0 || slot >= RACE_CARS) return NULL;
    char dir[CAR_LOAD_PATH], path[CAR_LOAD_PATH], stem[16];
    snprintf(dir, sizeof dir, "%s/SIMDATA/CARFAMS/", drive_cars.data_root);
    if (!car_load_path(car, race, dir, path, stem)) return NULL;
    size_t size = 0;
    drive_cars.file[slot] = read_named(path, strlen(dir), &size);
    if (drive_cars.colours.on && slot != drive_cars.colours.player_slot && slot < drive_cars.colours.racers) {   /* port: a rival's colours, by the car its file is */
        const int32_t m = race->models[slot];
        const int real = car_paint_race_car(m);
        if (real >= 0 && real < CAR_MODELS) slot_colours(slot, real);
    }
    drive_load_textures(race, slot, stem, size);
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

/* A car model's specs (its PBS) */
int load_specs(const char *data_root, int model, CarSpecs *specs)
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
void race_stand_in(void *user, int32_t player) { (void)user, drive_link_stand_in((int16_t)player); }

int32_t link_wait_ticks;  /* 004fbac8: 00410d80's, a link race's queue wait */

/* port (online): the extras this race runs under: the machine's own, but in a link game the host's rules (session_rules.h) where they decide the race or the cars' colours
 * (a guest without them, from an older host, keeps its own as before) */
PortExtras race_extras(void)
{
    PortExtras e = port_extras_load();
    const SessionRules *r = drive_link_on ? session_rules_active() : NULL;
    if (r == NULL) return e;
    e.difficulty = r->difficulty, e.lunar_springs = r->lunar_springs, e.lunar_gravity = r->lunar_gravity, e.rally_gravel = r->rally_gravel;
    e.extra_liveries = r->liveries, e.reverse = r->reverse;
    e.nitro = (r->cheats & EXTRAS_MARK_NITRO) != 0, e.explode = (r->cheats & EXTRAS_MARK_EXPLODE) != 0, e.turbo = (r->cheats & EXTRAS_MARK_TURBO) != 0;
    return e;
}

/* port: the car colours for this race (the mode's file extra_liveries, the CUSTOMIZE switch, and car_paint, the car showcase's Color box):
 * the player's car wears the paint the box picked, whether the console version livery (needs port.cfg's psx_dir files) or a catalogue paint
 * (car_paint.h); every rival draws its own from the race's seed, so a replay of the seed shows the same colours, and is never the
 * player's when it drives the same car. Off, nobody is repainted.
 * A rival's car is not the field's entry's: race_models.c picks the opponents' cars apart from the entries (a single race's are all the
 * first rival's, a pack's three by the player's), so its colours are decided when its file is read (drive_load, slot_colours) from the model
 * that file is, and its livery, paint and masks are that car's own. */
static int rival_paint_choice(uint32_t paint_seed, int slot, int car, int player_car, int player_paint, const char *psx_dir)
{
    uint16_t swatch[2];
    const int second = psx_dir[0] != '\0' && car >= 0 && car < CAR_PAINT_MODELS && drive_livery_swatch(psx_dir, car_model_names[car], swatch);
    return car_paint_pick(paint_seed, slot, car, player_car, player_paint, car_paint_count(car), second);
}

static uint8_t race_paint[RACE_PLAYERS];   /* port (replay colours): the paint each slot wears this race, written with its replay (drive_replay_set_paints) */

/* the colours of `slot`, which drives `car` (a race model, car_model_names' order). In a replay whose file recorded them they are the race's own: the seed a stage set keeps, the player's choice
 * and the PS1 files may be other now, and a draw again would give other colours. */
void slot_colours(int slot, int car)
{
    const int is_player = slot == drive_cars.colours.player_slot;
    const int shared = drive_cars.colours.shared;
    int paint;
    if (is_player) {
        paint = drive_cars.colours.player_paint;
    } else if (shared && slot < drive_cars.colours.humans && slot < 8 && drive_cars.colours.paint[slot] != 0xff) {   /* port (online): a human's own pick, as its machine sent it */
        uint16_t swatch[2];
        const int second = drive_cars.psx_dir[0] != '\0' && car >= 0 && car < CAR_PAINT_MODELS && drive_livery_swatch(drive_cars.psx_dir, car_model_names[car], swatch);
        paint = car_paint_replayed(drive_cars.colours.paint[slot], car, second);
    } else {   /* a rival: the race's draw; in a link game the one car it must differ from is the host's, the same on every machine */
        paint = rival_paint_choice(drive_cars.colours.seed, slot, car, shared ? drive_cars.colours.host_car : drive_cars.colours.player_car,
                                   shared ? drive_cars.colours.host_paint : drive_cars.colours.player_paint, drive_cars.psx_dir);
    }
    if (replay_of != NULL && replay_of->paint_known && slot < RACE_PLAYERS) {
        uint16_t swatch[2];
        const int second = drive_cars.psx_dir[0] != '\0' && car >= 0 && car < CAR_PAINT_MODELS && drive_livery_swatch(drive_cars.psx_dir, car_model_names[car], swatch);
        const int saved = car_paint_replayed(replay_of->paint[slot], car, second);
        if (saved >= 0) paint = saved;
    }
    if (slot < RACE_PLAYERS) {
        race_paint[slot] = (uint8_t)paint;
        drive_replay_set_paints(race_paint);
    }
    drive_cars.model[slot] = car;
    drive_cars.livery[slot] = paint == PAINT_SECOND;
    drive_cars.fixed_black[slot] = 1;
    drive_cars.paint_on[slot] = 0;
    if (paint >= PAINT_CATALOGUE && car_paint(car, paint - PAINT_CATALOGUE) != NULL) {
        memcpy(drive_cars.paint[slot], car_paint(car, paint - PAINT_CATALOGUE)->rgb, 3);
        drive_cars.paint_on[slot] = 1;
    }
    port_log_detail("car colours: slot %d (car %d) takes paint %d%s\n", slot, car, paint, is_player ? " (the player's)" : "");
}

void rival_liveries(const RaceField *field, int player, const char *track, int replay)
{
    static CarPaintSeedKeep seed_keep;   /* port: the rivals' colours hold from one stage of a set to the next (car_paint_seed) */
    const uint32_t paint_seed = replay ? field->seed : car_paint_seed(&seed_keep, track, field->seed);
    const PortExtras extras = race_extras();
    const PortSettings settings = port_settings_load();
    memset(drive_cars.livery, 0, sizeof drive_cars.livery);
    memset(drive_cars.paint_on, 0, sizeof drive_cars.paint_on);
    memset(drive_cars.fixed_black, 0, sizeof drive_cars.fixed_black);
    memset(&drive_cars.colours, 0, sizeof drive_cars.colours);
    memset(race_paint, REPLAY_PAINT_NONE, sizeof race_paint);
    drive_replay_set_paints(race_paint);   /* none until a slot's colours are decided (the colours off: none) */
    drive_cars.dash_rgb_on = 0;
    snprintf(drive_cars.psx_dir, sizeof drive_cars.psx_dir, "%s", settings.psx_dir);
    port_log_detail("car colours: flag %d, psx_dir \"%s\"\n", extras.extra_liveries, settings.psx_dir);
    if (!extras.extra_liveries) return;
    const int player_car = field->entries[player].car;
    int player_paint = player_car >= 0 && player_car < CAR_PAINT_SLOTS ? extras.car_paint[player_car] : PAINT_OWN;
    if ((player_paint == PAINT_SECOND && settings.psx_dir[0] == '\0') || player_paint >= PAINT_CATALOGUE + car_paint_count(player_car))
        player_paint = PAINT_OWN;   /* a choice the files or the table no longer have */
    drive_cars.colours.on = 1, drive_cars.colours.seed = paint_seed, drive_cars.colours.player_slot = player;
    drive_cars.colours.player_car = player_car, drive_cars.colours.player_paint = player_paint, drive_cars.colours.racers = field->racers;
    const SessionRules *host = drive_link_on ? session_rules_active() : NULL;
    if (host != NULL) {   /* port (online): every machine paints the same slots the same */
        drive_cars.colours.shared = 1, drive_cars.colours.humans = drive_link.humans;
        drive_cars.colours.host_car = field->entries[0].car, drive_cars.colours.host_paint = host->paint[0] != 0xff ? host->paint[0] : PAINT_OWN;
        memcpy(drive_cars.colours.paint, host->paint, sizeof drive_cars.colours.paint);
    }
    slot_colours(player, player_car);   /* the player's own is known now: the dash art asks for it before the cars are read */
}

/* port: the player's livery or paint on the dash art's body, for the HUD's loads of the dash bank (drive_hud.dash_paint) */
int paint_dash(void *user, uint8_t *bank, size_t size)
{
    (void)user;
    const int slot = race_slot(drive_sim.race.player);
    if (slot < 0 || slot >= RACE_CARS) return 0;
    const char *stem = car_model_names[drive_cars.model[slot]];
    if (drive_cars.livery[slot] && drive_cars.dash_rgb_on) return drive_livery_dash(stem, bank, size, drive_cars.dash_rgb);
    if (drive_cars.paint_on[slot]) return drive_livery_dash(stem, bank, size, drive_cars.paint[slot]);
    return 0;
}

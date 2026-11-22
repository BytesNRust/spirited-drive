#include "car_race.h"

#include "formats/car_paint.h"
#include "formats/car_kit_paths.h"
#include "formats/car_piece.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define CAR_PAINT_DIGITS 12   /* the game's CAR_PAINT_SLOTS (game/sys/port_extras.h): Studio links no game header */

extern char **environ;

static const RaceTrack TRACKS[] = {
    { "CY1", "City 1", 1 }, { "CY2", "City 2", 1 }, { "CY3", "City 3", 1 },
    { "CL1", "Coastal 1", 1 }, { "CL2", "Coastal 2", 1 }, { "CL3", "Coastal 3", 1 },
    { "AL1", "Alpine 1", 1 }, { "AL2", "Alpine 2", 1 }, { "AL3", "Alpine 3", 1 },
    { "TR5", "Rusty Springs", 0 }, { "TR1", "TR1", 0 }, { "TR2", "Burnt Sienna", 0 }, { "TR6", "Vertigo Ridge", 0 },
    { "TR3", "Transtropolis", 0 }, { "TR7", "Lost Vegas (no night)", 0 }, { "TR4", "TR4", 0 },
};
enum { TRACK_COUNT = sizeof TRACKS / sizeof TRACKS[0] };

int car_race_track_count(void)
{
    return TRACK_COUNT;
}

const RaceTrack *car_race_track(int index)
{
    return index >= 0 && index < TRACK_COUNT ? &TRACKS[index] : NULL;
}

int car_race_track_find(const char *stem)
{
    for (int i = 0; stem != NULL && i < TRACK_COUNT; ++i)
        if (strcasecmp(TRACKS[i].stem, stem) == 0) return i;
    return -1;
}

const char *car_race_track_label(const char *stem)
{
    const int i = car_race_track_find(stem);
    return i >= 0 ? TRACKS[i].label : stem != NULL ? stem : "";
}

int car_race_photo_ticks(const ExportCar *car)
{
    return car == NULL || car->kind == EXPORT_DRIVABLE ? RACE_TICKS_DRIVEN : car->kind == EXPORT_POLICE ? RACE_TICKS_POLICE : RACE_TICKS_TRAFFIC;
}

int car_race_default_track(const ExportCar *car)
{
    return car_race_track_find(car != NULL && car->kind != EXPORT_DRIVABLE ? "CL1" : "TR5");
}

int car_race_menu_pick(int model)
{
    static const int MODEL_OF_PICK[8] = { 7, 6, 0, 2, 3, 5, 4, 1 };   /* 00426620: the menu's number to the model (game/race/race_models.c HEAD_TO_HEAD) */
    for (int pick = 0; pick < 8; ++pick)
        if (MODEL_OF_PICK[pick] == model) return pick;
    return -1;
}

const ExportCar *car_race_menu_car(int pick)
{
    for (int i = 0; pick >= 0 && pick < 8 && car_export_car(i) != NULL; ++i)
        if (car_export_car(i)->kind == EXPORT_DRIVABLE && car_race_menu_pick(car_export_car(i)->model) == pick) return car_export_car(i);
    return NULL;
}

const char *car_race_player_label(const ExportCar *car)
{
    if (car == NULL) return NULL;
    if (car->kind == EXPORT_DRIVABLE) return car->label;
    for (int i = 0; car_export_car(i) != NULL; ++i)
        if (car_export_car(i)->kind == EXPORT_DRIVABLE && car_export_car(i)->model == RACE_TRAFFIC_PLAYER_MODEL) return car_export_car(i)->label;
    return NULL;
}

void car_race_note(const ExportCar *car, const char *track, char *out, size_t cap)
{
    if (cap == 0) return;
    out[0] = '\0';
    const int ti = car_race_track_find(track);
    const char *driver = car_race_player_label(car);
    if (car == NULL || ti < 0 || driver == NULL) return;
    if (car->kind == EXPORT_DRIVABLE)
        snprintf(out, cap, "You drive the %s on %s, with this project's masks.", driver, TRACKS[ti].label);
    else if (car->kind == EXPORT_POLICE)
        snprintf(out, cap, "You drive the %s on %s: the %s cannot be driven (it has no specs of its own). It is the police car of this race with traffic.", driver,
                 TRACKS[ti].label, car->label);
    else
        snprintf(out, cap, "You drive the %s on %s: the %s cannot be driven (a traffic car has no specs of its own). It is one of the traffic's cars, which the game "
                 "draws at random for each race: look for it on the road, ahead of you or coming the other way. Some races do not have it; In-game photo finds one that does.",
                 driver, TRACKS[ti].label, car->label);
}

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list args;
        va_start(args, format);
        vsnprintf(why, cap, format, args);
        va_end(args);
    }
    return 0;
}

/* `word` made from the format, appended to `list` (`*count` of `cap`) */
static int add_word(char **list, int *count, int cap, const char *format, va_list args)
{
    char buf[RACE_WORD_MAX];
    const int n = vsnprintf(buf, sizeof buf, format, args);
    if (*count >= cap || n < 0 || n >= (int)sizeof buf) return 0;
    list[*count] = strdup(buf);
    if (list[*count] == NULL) return 0;
    list[++*count] = NULL;
    return 1;
}

static int add_arg(RaceCommand *c, const char *format, ...) __attribute__((format(printf, 2, 3)));
static int add_arg(RaceCommand *c, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int ok = add_word(c->argv, &c->argc, RACE_ARGS, format, args);
    va_end(args);
    return ok;
}

static int add_env(RaceCommand *c, const char *format, ...) __attribute__((format(printf, 2, 3)));
static int add_env(RaceCommand *c, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int ok = add_word(c->envp, &c->envc, RACE_ENVS, format, args);
    va_end(args);
    return ok;
}

void car_race_command_free(RaceCommand *c)
{
    for (int i = 0; i < c->argc; ++i) free(c->argv[i]);
    for (int i = 0; i < c->envc; ++i) free(c->envp[i]);
    memset(c, 0, sizeof *c);
}

/* a piece's id is a folder name: letters, digits, '-' and '_' */
int car_race_piece_id_ok(const char *id)
{
    if (id[0] == '\0' || strlen(id) >= sizeof ((RaceJob *)0)->piece) return 0;
    for (const char *c = id; *c != '\0'; ++c)
        if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_') return 0;
    return 1;
}

/* Test drive: the piece the run installed (--mods: the profile's mods are the scratch folder's, empty but for it), a time trial in the window with the menu's car */
static int drive_command(const RaceSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap)
{
    if (s->car == NULL || s->game == NULL || s->cd == NULL || s->cd[0] == '\0' || s->user == NULL) return refuse(why, why_cap, "the test drive is not set up (no game files)");
    if (!car_race_piece_id_ok(s->piece)) return refuse(why, why_cap, "%s is not a piece's id", s->piece);
    if (s->car->kind != EXPORT_DRIVABLE || car_race_menu_pick(s->car->model) < 0) return refuse(why, why_cap, "%s is not a car you drive", s->car->label);
    if (s->photo_ticks > 0) return refuse(why, why_cap, "a test drive has no photo");
    int ok = add_arg(out, "%s", s->game) && add_arg(out, "--data") && add_arg(out, "%s", s->cd) && add_arg(out, "--user") && add_arg(out, "%s", s->user) &&
             add_arg(out, "--mods") && add_arg(out, "%s/mods/%s", s->user, s->piece) && add_arg(out, "--drive") && add_arg(out, "%s", s->piece) &&
             add_arg(out, "--car") && add_arg(out, "%d", car_race_menu_pick(s->car->model)) && add_arg(out, "--type") && add_arg(out, "0");
    for (char *const *e = base; ok && e != NULL && *e != NULL; ++e)
        if (strncmp(*e, "TNFS_", 5) != 0) ok = add_env(out, "%s", *e);
    if (ok) return 1;
    car_race_command_free(out);
    return refuse(why, why_cap, "the command is too long");
}

int car_race_command(const RaceSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (s->piece != NULL) return drive_command(s, base, out, why, why_cap);
    const int traffic = s->car != NULL && s->car->kind != EXPORT_DRIVABLE;
    const int mod = s->track_mods != NULL && s->track_mods[0] != '\0';
    const int ti = mod ? -1 : car_race_track_find(s->track);
    if (s->car == NULL || s->game == NULL || s->cd == NULL || s->project == NULL || s->user == NULL || s->track == NULL) return refuse(why, why_cap, "the race is not set up");
    if (ti < 0 && !mod) return refuse(why, why_cap, "%s is not a track of the list", s->track);
    if (mod && !car_race_piece_id_ok(s->track)) return refuse(why, why_cap, "%s is not an installed track's id", s->track);
    if (traffic && mod) return refuse(why, why_cap, "an installed track has no traffic here: choose a City, Coastal or Alpine stage for a traffic or police car");
    if (traffic && !TRACKS[ti].stage) return refuse(why, why_cap, "%s has no traffic: choose a City, Coastal or Alpine stage for a traffic or police car", TRACKS[ti].label);
    if (s->photo_ticks > 0 && s->shot == NULL) return refuse(why, why_cap, "no file for the photo");
    int ok = add_arg(out, "%s", s->game) && add_arg(out, "--data") && add_arg(out, "%s", s->cd) && add_arg(out, "--user") && add_arg(out, "%s", s->user);
    if (ok && mod) ok = add_arg(out, "--mods") && add_arg(out, "%s", s->track_mods);   /* the installed track's folder: the game finds the track by its id there */
    ok = ok && add_arg(out, "--drive") && add_arg(out, "%s", mod ? s->track : TRACKS[ti].stem);
    const int model = traffic ? RACE_TRAFFIC_PLAYER_MODEL : s->car->model;   /* the player's car; for a traffic or police car the traffic is the test */
    const int number = s->photo_ticks > 0 ? model : car_race_menu_pick(model);   /* a photo takes the model, a race in a window the menu's number */
    if (number < 0) {
        car_race_command_free(out);
        return refuse(why, why_cap, "%s has no number in the game's car menu", s->car->label);
    }
    ok = ok && add_arg(out, "--car") && add_arg(out, "%d", number);
    if (traffic) ok = ok && add_arg(out, "--type") && add_arg(out, "1");
    else if (s->photo_ticks > 0) ok = ok && add_arg(out, "--racers") && add_arg(out, "1");
    else ok = ok && add_arg(out, "--type") && add_arg(out, "0");   /* T4: the window's race is Control Central's: a time trial has the car alone */
    if (s->photo_ticks > 0) ok = ok && add_arg(out, "--node") && add_arg(out, "%d", s->photo_ticks) && add_arg(out, "--screenshot") && add_arg(out, "%s", s->shot);
    for (char *const *e = base; ok && e != NULL && *e != NULL; ++e) {
        if (strncmp(*e, "TNFS_", 5) == 0 || (s->photo_ticks > 0 && strncmp(*e, "SDL_", 4) == 0)) continue;
        ok = add_env(out, "%s", *e);
    }
    ok = ok && add_env(out, "TNFS_CAR_MASKS=%s/car_masks", s->project);
    if (!traffic) ok = ok && add_env(out, "TNFS_DASH_MASKS=%s/dash_masks", s->project);
    if (s->photo_ticks > 0)   /* headless, the follow camera behind the car as the car card's picture has it */
        ok = ok && add_env(out, "SDL_VIDEO_DRIVER=dummy") && add_env(out, "SDL_RENDER_DRIVER=software") && add_env(out, "SDL_AUDIO_DRIVER=dummy") &&
             add_env(out, "SDL_AUDIODRIVER=dummy") && add_env(out, "TNFS_VIEW=%d", s->cockpit && !traffic ? 0 : 3);
    if (s->photo_ticks > 0 && !traffic && s->gauge_set) ok = ok && add_env(out, "TNFS_GAUGE_LIGHT=%d", s->gauge_light);
    if (s->car->kind == EXPORT_POLICE && s->photo_ticks > 0) ok = ok && add_env(out, "TNFS_POLICE_NEAR=%d", RACE_POLICE_NEAR);   /* chasing the player from behind: a camera on it sees it */
    if (s->slot > 0) ok = ok && add_env(out, "TNFS_FOLLOW_CAR=%d", s->slot);
    if (s->slot > 0 && s->photo_ticks > 0 && s->car->kind == EXPORT_TRAFFIC) ok = ok && add_env(out, "TNFS_FOLLOW_SETTLE=%d", RACE_FOLLOW_SETTLE);   /* parked until the player nears */
    if (s->seed != 0) ok = ok && add_env(out, "TNFS_DRIVE_SEED=%u", (unsigned)s->seed);
    if (ok) return 1;
    car_race_command_free(out);
    return refuse(why, why_cap, "the command is too long");
}

const char RACE_LOOK_ON[] = "body_shine=4\ncar_colours=1\ncar_soft_shadow=1\ntexture_filter=1\nlight_effects=1\nlamp_flares=1\nperspective=1\nfog=1\nfog_sample=1\n"
                           "sky_fill=1\nsky_fill_below=1\npolice_glow=1\ntraffic_halo=1\n";

RaceLook car_race_look_default(void)
{
    const RaceLook look = { 1, 1, 1, 1, 2, 0, 0, 0, 0, 0 };
    return look;
}

void car_race_look_off(const RaceLook *look, char *out, size_t cap)
{
    if (cap == 0) return;
    out[0] = '\0';
    const char *const names[4] = { "Reflections", "Car colours", "Soft shadow", "Texture filter" };
    const int on[4] = { look->reflections, look->car_colours, look->soft_shadow, look->texture_filter };
    size_t at = 0;
    for (int i = 0; i < 4; ++i) {
        if (on[i]) continue;
        const int n = snprintf(out + at, cap - at, "%s%s", at > 0 ? ", " : "", names[i]);
        if (n < 0 || (size_t)n >= cap - at) return;   /* cut: what fits is said */
        at += (size_t)n;
    }
    const int n = snprintf(out + at, cap - at, "%s%s", at > 0 ? " off" : "", look->scale == 1 ? (at > 0 ? ", 640x480" : "640x480") : "");
    (void)n;   /* a cut text still ends within cap */
}

/* the look's lines of restomod.cfg: RACE_LOOK_ON with the four switches as `look` has them, then the render scale */
static int look_lines(const RaceLook *look, char *out, size_t cap)
{
    const int n = snprintf(out, cap, "%s", RACE_LOOK_ON);
    if (n < 0 || (size_t)n >= cap) return 0;
    struct { const char *key; int on, value; } sw[4] = { { "body_shine=", look->reflections, 4 }, { "car_colours=", look->car_colours, 1 },
                                                        { "car_soft_shadow=", look->soft_shadow, 1 }, { "texture_filter=", look->texture_filter, 1 } };
    for (int i = 0; i < 4; ++i) {   /* the key's line is in RACE_LOOK_ON at its on value: written over with 0 when the switch is off (same length: one digit) */
        char *line = strstr(out, sw[i].key);
        if (line != NULL && !sw[i].on) line[strlen(sw[i].key)] = '0';
    }
    const size_t used = strlen(out);
    const int m = snprintf(out + used, cap - used, "render_scale=%d\nfullscreen=0\n", look->scale == 1 ? 1 : 2);   /* every run the Studio starts is in a window, never full screen over the Studio (the profile's default is full screen) */
    return m > 0 && (size_t)m < cap - used;
}

int car_race_paint_digit(const ExportCar *car, const uint8_t *rgb)
{
    if (car == NULL || rgb == NULL || car->model < 0 || car->model >= CAR_PAINT_DIGITS) return 0;
    for (int k = 0; k < car_paint_count(car->model) && 2 + k <= 15; ++k) {
        const CarPaint *p = car_paint(car->model, k);
        if (p != NULL && memcmp(p->rgb, rgb, 3) == 0) return 2 + k;
    }
    return 0;
}

int car_race_user_files(const char *user, int night, const RaceLook *look, char *why, size_t why_cap)
{
    char path[EXPORT_PATH + 32], mode[1024], lines[600];
    static const char PORT[] = "profile=restomod\nfullscreen=0\n";   /* a window, never full screen over the Studio: the key is free in every mode, so the game reads it from either file; both say it */
    const RaceLook fallback = car_race_look_default();
    if (!look_lines(look != NULL ? look : &fallback, lines, sizeof lines)) return refuse(why, why_cap, "the look's settings are too long");
    const RaceLook *l = look != NULL ? look : &fallback;
    char paint[CAR_PAINT_DIGITS + 16] = "";   /* Preview's colour: the game honours car_paint only with extra_liveries on */
    if (l->paint > 0) {
        if (l->paint_model < 0 || l->paint_model >= CAR_PAINT_DIGITS || l->paint > 15) return refuse(why, why_cap, "the paint %d of car %d is not one the game has", l->paint, l->paint_model);
        memset(paint, '0', (size_t)l->paint_model + 1);
        paint[l->paint_model] = "0123456789abcdef"[l->paint];
        snprintf(paint + l->paint_model + 1, sizeof paint - (size_t)l->paint_model - 1, "\n");
    }
    const int n = l->paint > 0 ? snprintf(mode, sizeof mode, "%sextra_liveries=1\ncar_paint=%slight_time=%s\n", lines, paint, night ? "night" : "day")
                               : snprintf(mode, sizeof mode, "%sextra_liveries=0\nlight_time=%s\n", lines, night ? "night" : "day");
    if (n < 0 || (size_t)n >= sizeof mode) return refuse(why, why_cap, "the look's settings are too long");
    const char *const parts[2] = { PORT, mode };
    const char *const names[2] = { "port.cfg", "restomod.cfg" };
    for (int i = 0; i < 2; ++i) {
        if (snprintf(path, sizeof path, "%s/%s", user, names[i]) >= (int)sizeof path) return refuse(why, why_cap, "the path is too long: %s", user);
        FILE *f = fopen(path, "wb");
        const size_t n = strlen(parts[i]);
        const int ok = f != NULL && fwrite(parts[i], 1, n, f) == n;
        if (f != NULL && fclose(f) != 0) return refuse(why, why_cap, "cannot write %s: %s", path, strerror(errno));
        if (!ok) return refuse(why, why_cap, "cannot write %s: %s", path, strerror(errno));
    }
    return 1;
}

int car_race_temp(char *temp, size_t temp_cap, char *user, size_t user_cap, char *why, size_t why_cap)
{
    const char *base = getenv("TMPDIR");
    char path[EXPORT_PATH];
    const int n = snprintf(path, sizeof path, "%s/studio-race-XXXXXX", base != NULL && base[0] != '\0' ? base : "/tmp");
    if (n < 0 || (size_t)n >= sizeof path || mkdtemp(path) == NULL) return refuse(why, why_cap, "cannot make a scratch folder: %s", strerror(errno));
    if (snprintf(temp, temp_cap, "%s", path) >= (int)temp_cap || snprintf(user, user_cap, "%s/user", path) >= (int)user_cap || mkdir(user, 0777) != 0) {
        const int err = errno;
        rmdir(path);
        return refuse(why, why_cap, "cannot make a scratch folder: %s", strerror(err != 0 ? err : ENAMETOOLONG));
    }
    return 1;
}

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

int car_race_temp_remove(const char *temp)
{
    const char *slash = temp != NULL ? strrchr(temp, '/') : NULL;
    if (slash == NULL || strncmp(slash + 1, "studio-race-", 12) != 0) return 0;   /* only what car_race_temp made */
    struct stat st;
    if (lstat(temp, &st) != 0 || !S_ISDIR(st.st_mode)) return lstat(temp, &st) != 0;
    return nftw(temp, remove_entry, 16, FTW_DEPTH | FTW_PHYS) == 0;
}

int car_race_find_slot(const char *log, const char *stem)
{
    static const char KEY[] = "race car: slot ";
    for (const char *p = log; p != NULL && (p = strstr(p, KEY)) != NULL; p += sizeof KEY - 1) {
        char name[32];
        int slot = -1;
        if (sscanf(p + sizeof KEY - 1, "%d is %31s", &slot, name) == 2 && strcasecmp(name, stem) == 0) return slot;
    }
    return -1;
}

static int has_word(const char *line, size_t len, const char *word)
{
    const size_t w = strlen(word);
    for (size_t i = 0; i + w <= len; ++i)
        if (strncasecmp(line + i, word, w) == 0) return 1;
    return 0;
}

RaceLineKind car_race_line_kind(const char *line, size_t len)
{
    static const char *const BAD[] = { "error", "fatal", "cannot", "failed", "no such", "not found", "bad ", "crash", "abort", "assert", "unsupported", NULL };
    if (len >= 9 && strncmp(line, "car mask:", 9) == 0 && !has_word(line, len, "cannot")) return RACE_LINE_MASK;
    if (len >= 22 && strncmp(line, "platform: cannot read ", 22) == 0 && has_word(line, len, "_mask.png: Couldn't open")) return RACE_LINE_NOTE;
    for (int i = 0; BAD[i] != NULL; ++i)
        if (has_word(line, len, BAD[i])) return RACE_LINE_ERROR;
    return RACE_LINE_PLAIN;
}

int car_race_count_notes(const char *text, size_t len)
{
    int count = 0;
    for (const char *p = text, *end = text + len; p != NULL && p < end;) {
        const char *nl = memchr(p, '\n', (size_t)(end - p)), *line_end = nl != NULL ? nl : end;
        count += car_race_line_kind(p, (size_t)(line_end - p)) == RACE_LINE_NOTE;
        p = nl != NULL ? nl + 1 : NULL;
    }
    return count;
}

int car_race_read_ppm(const char *path, uint8_t **rgba, int *w, int *h, char *why, size_t why_cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return refuse(why, why_cap, "the game wrote no picture (%s)", path);
    int width = 0, height = 0, max = 0, used = 0;
    char head[64] = { 0 };
    const size_t got = fread(head, 1, sizeof head - 1, f);
    const int parsed = got >= 12 && sscanf(head, "P6 %d %d %d%n", &width, &height, &max, &used) == 3 && max == 255;
    if (!parsed || !((width == RACE_PHOTO_W && height == RACE_PHOTO_H) || (width == 2 * RACE_PHOTO_W && height == 2 * RACE_PHOTO_H))) {
        fclose(f);
        return refuse(why, why_cap, "the game's picture is not a %dx%d or %dx%d PPM", RACE_PHOTO_W, RACE_PHOTO_H, 2 * RACE_PHOTO_W, 2 * RACE_PHOTO_H);
    }
    const size_t pixels = (size_t)width * height;
    uint8_t *rgb = malloc(pixels * 3), *out = malloc(pixels * 4);
    if (rgb == NULL || out == NULL) {
        free(rgb), free(out), fclose(f);
        return refuse(why, why_cap, "out of memory");
    }
    const int ok = fseek(f, (long)used + 1, SEEK_SET) == 0 && fread(rgb, 1, pixels * 3, f) == pixels * 3;
    fclose(f);
    if (!ok) {
        free(rgb), free(out);
        return refuse(why, why_cap, "the game's picture is cut short");
    }
    for (size_t i = 0; i < pixels; ++i) out[4 * i] = rgb[3 * i], out[4 * i + 1] = rgb[3 * i + 1], out[4 * i + 2] = rgb[3 * i + 2], out[4 * i + 3] = 255;
    free(rgb);
    *rgba = out, *w = width, *h = height;
    return 1;
}

const ExportCar *car_race_original_of(const CarProject *c)
{
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        const ExportCar *car = car_export_car(i);
        if (c->masks_only ? strcasecmp(c->folder, car->folder) == 0 : strcasecmp(c->info.stem, car->file) == 0) return car;   /* a kit of an original (any kind) or a piece folder of one */
    }
    return NULL;
}

/* ---- the job ---- */

static void job_log(RaceJob *j, const char *text, size_t n)
{
    if (j->log == NULL) return;
    if (n > RACE_LOG_MAX - 1) text += n - (RACE_LOG_MAX - 1), n = RACE_LOG_MAX - 1;
    if (j->log_len + n > RACE_LOG_MAX - 1) {   /* the start goes */
        const size_t drop = j->log_len + n - (RACE_LOG_MAX - 1);
        memmove(j->log, j->log + drop, j->log_len - drop);
        j->log_len -= drop;
    }
    memcpy(j->log + j->log_len, text, n);
    j->log_len += n;
    j->log[j->log_len] = '\0';
}

static void job_say(RaceJob *j, const char *format, ...) __attribute__((format(printf, 2, 3)));
static void job_say(RaceJob *j, const char *format, ...)
{
    char line[600];
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (n > 0) job_log(j, line, (size_t)(n < (int)sizeof line ? n : (int)sizeof line - 1));
}

/* the game's own log of the run (its user folder's spirited-drive.log), taken into the job's and removed, so the next run's is its own */
static char *take_game_log(const RaceJob *j, size_t *len)
{
    char path[EXPORT_PATH + 32];
    *len = 0;
    if (snprintf(path, sizeof path, "%s/spirited-drive.log", j->own[3]) >= (int)sizeof path) return NULL;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    char *text = malloc(PROC_LOG_MAX + 1);
    if (text != NULL) {
        *len = fread(text, 1, PROC_LOG_MAX, f);
        text[*len] = '\0';
    }
    fclose(f);
    unlink(path);
    return text;
}

static int launch(RaceJob *j, char *why, size_t why_cap)
{
    RaceSetup *s = &j->setup;
    s->game = j->own[0], s->cd = j->own[1], s->project = j->own[2], s->user = j->own[3], s->shot = j->own[4], s->track = j->track, s->track_mods = j->mods;
    s->piece = j->piece[0] != '\0' ? j->piece : NULL;
    char shot[EXPORT_PATH + 32];
    snprintf(shot, sizeof shot, "%s/frame.ppm", j->temp);
    snprintf(j->own[4], sizeof j->own[4], "%s", shot);
    car_race_command_free(&j->command);   /* a probe's words, before the next run's */
    if (!car_race_command(s, environ, &j->command, why, why_cap)) return 0;
    return proc_start(&j->run, j->command.argv, j->command.envp, NULL, why, why_cap);
}

/* nothing running and a game where `game` says */
static int job_can_start(const RaceJob *j, const char *game, char *why, size_t why_cap)
{
    if (j->state != RACE_JOB_IDLE || j->run.running) return refuse(why, why_cap, "a race is already running");
    struct stat st;
    if (game == NULL || stat(game, &st) != 0 || !S_ISREG(st.st_mode)) return refuse(why, why_cap, "the game's program is not where Studio looked (%s): use Locate game", game != NULL ? game : "nowhere");
    return 1;
}

/* a fresh job (the photo serial kept) with its log and scratch folder: own[3] is the scratch player folder; 0 with `why` and nothing left */
static int job_begin(RaceJob *j, const char *game, const char *cd, const char *project, const RaceLook *look, char *why, size_t why_cap)
{
    proc_free(&j->run);
    install_job_free(&j->install);
    car_race_command_free(&j->command);
    free(j->photo), free(j->log);
    const int serial = j->photo_serial;
    memset(j, 0, sizeof *j);
    j->run.fd = -1;
    install_job_init(&j->install);
    j->photo_serial = serial;
    j->look = look != NULL ? *look : car_race_look_default();
    j->log = malloc(RACE_LOG_MAX);
    if (j->log == NULL) return refuse(why, why_cap, "out of memory");
    j->log[0] = '\0';
    char user[EXPORT_PATH];
    if (!car_race_temp(j->temp, sizeof j->temp, user, sizeof user, why, why_cap)) {
        free(j->log), j->log = NULL;
        return 0;
    }
    snprintf(j->own[0], sizeof j->own[0], "%s", game), snprintf(j->own[1], sizeof j->own[1], "%s", cd), snprintf(j->own[2], sizeof j->own[2], "%s", project);
    snprintf(j->own[3], sizeof j->own[3], "%s", user);
    return 1;
}

/* a job that could not start: its scratch folder and log go */
static int job_abandon(RaceJob *j)
{
    car_race_temp_remove(j->temp);
    j->temp[0] = '\0';
    free(j->log), j->log = NULL;
    j->state = RACE_JOB_IDLE;
    return 0;
}

/* 1 when `dir` is a directory */
static int is_dir_at(const char *dir)
{
    struct stat st;
    return stat(dir, &st) == 0 && S_ISDIR(st.st_mode);
}

/* A kit keeps its masks flat in art-generated/car_masks and art-generated/dash_masks; the game reads TNFS_CAR_MASKS/<stem>/<tex>_mask.png (and the same for the dash).
 * For a project that is a kit (`project` has art-generated/car_masks): a folder in the job's scratch folder with that stem level, each stem a link to the kit's folder, and
 * the job's project becomes it, so the run sees the masks as they are now and the kit is never written. Any other project is left as it is. 0 with `why` when a link fails. */
static int stage_kit_masks(RaceJob *j, const ExportCar *car, const char *project, char *why, size_t why_cap)
{
    char from[EXPORT_PATH + 64], dir[EXPORT_PATH + 64], to[EXPORT_PATH + 128];
    static const struct { CarKitPath source; const char *folder; } KINDS[] = { { CAR_KIT_GEN_CAR_MASKS, "car_masks" }, { CAR_KIT_GEN_DASH_MASKS, "dash_masks" } };
    if (snprintf(from, sizeof from, "%s/%s", project, car_kit_path(CAR_KIT_GEN_CAR_MASKS)) >= (int)sizeof from || !is_dir_at(from)) return 1;
    if (snprintf(dir, sizeof dir, "%s/masks", j->temp) >= (int)sizeof dir || mkdir(dir, 0777) != 0) return refuse(why, why_cap, "cannot make a folder for the masks: %s", strerror(errno));
    for (size_t k = 0; k < sizeof KINDS / sizeof KINDS[0]; ++k) {
        if (snprintf(from, sizeof from, "%s/%s", project, car_kit_path(KINDS[k].source)) >= (int)sizeof from || !is_dir_at(from)) continue;
        if (snprintf(to, sizeof to, "%s/masks/%s", j->temp, KINDS[k].folder) >= (int)sizeof to || mkdir(to, 0777) != 0)
            return refuse(why, why_cap, "cannot make a folder for the masks: %s", strerror(errno));
        if (snprintf(to, sizeof to, "%s/masks/%s/%s", j->temp, KINDS[k].folder, car->file) >= (int)sizeof to || symlink(from, to) != 0)
            return refuse(why, why_cap, "cannot link the kit's masks: %s", strerror(errno));
    }
    snprintf(j->own[2], sizeof j->own[2], "%s/masks", j->temp);
    return 1;
}

static int start_common(RaceJob *j, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, const char *mods, int night,
                        const RaceLook *look, RaceJobState state, char *why, size_t why_cap)
{
    if (!job_can_start(j, game, why, why_cap)) return 0;
    if (car == NULL) return refuse(why, why_cap, "this car is not one of the game's: Test drive is for the cars Export original car makes");
    if (mods == NULL && car_race_track_find(track) < 0) return refuse(why, why_cap, "no such track");
    if (mods != NULL && (!car_race_piece_id_ok(track) || strlen(mods) >= EXPORT_PATH)) return refuse(why, why_cap, "%s is not an installed track's id", track);
    if (strlen(game) >= EXPORT_PATH || strlen(cd) >= EXPORT_PATH || strlen(project) >= EXPORT_PATH) return refuse(why, why_cap, "a path is too long");
    if (!job_begin(j, game, cd, project, look, why, why_cap)) return 0;
    const char *user = j->own[3];
    snprintf(j->track, sizeof j->track, "%s", track);
    snprintf(j->mods, sizeof j->mods, "%s", mods != NULL ? mods : "");
    j->setup.cockpit = look != NULL && look->cockpit, j->setup.gauge_set = look != NULL && look->gauge_set, j->setup.gauge_light = look != NULL ? look->gauge_light : 0;
    j->setup.car = car, j->setup.night = night, j->setup.photo_ticks = state == RACE_JOB_RACE ? 0 : state == RACE_JOB_PROBE ? 1 : car_race_photo_ticks(car);
    j->state = state;
    if (!stage_kit_masks(j, car, project, why, why_cap) || !car_race_user_files(user, night, &j->look, why, why_cap) || !launch(j, why, why_cap)) return job_abandon(j);
    return 1;
}

int car_race_start_drive(RaceJob *job, const char *game, const char *cd, const char *zip, const char *piece, const ExportCar *car, const RaceLook *look,
                         char *why, size_t why_cap)
{
    if (!job_can_start(job, game, why, why_cap)) return 0;
    if (car == NULL || car->kind != EXPORT_DRIVABLE || car_race_menu_pick(car->model) < 0)
        return refuse(why, why_cap, "%s is not a car you drive", car != NULL ? car->label : "no car");
    if (piece == NULL || !car_race_piece_id_ok(piece)) return refuse(why, why_cap, "%s is not a piece's id", piece != NULL ? piece : "nothing");
    if (zip == NULL) return refuse(why, why_cap, "the test drive is not set up");
    if (strlen(game) >= EXPORT_PATH || strlen(cd) >= EXPORT_PATH) return refuse(why, why_cap, "a path is too long");
    if (!job_begin(job, game, cd, "", look, why, why_cap)) return 0;
    snprintf(job->piece, sizeof job->piece, "%s", piece);
    job->setup.car = car, job->setup.piece = job->piece;
    job->state = RACE_JOB_INSTALL;
    if (!car_race_user_files(job->own[3], 0, &job->look, why, why_cap) || !install_job_start(&job->install, game, zip, job->own[3], why, why_cap)) return job_abandon(job);
    job_say(job, "Test drive of %s: you drive the %s, a time trial. A scratch player folder: installing the track.\n", piece, car->label);
    return 1;
}

int car_race_start_race(RaceJob *job, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, int night, const RaceLook *look,
                        char *why, size_t why_cap)
{
    const int ok = start_common(job, game, cd, project, car, track, NULL, night, look, RACE_JOB_RACE, why, why_cap);
    if (ok) {
        char note[500];
        car_race_note(car, track, note, sizeof note);
        job_say(job, "Test drive, %s. %s A scratch player folder.\n", night ? "night" : "day", note);
    }
    return ok;
}

int car_race_start_race_mod(RaceJob *job, const char *game, const char *cd, const char *project, const ExportCar *car, const char *id, const char *mods_dir, int night,
                            const RaceLook *look, char *why, size_t why_cap)
{
    if (mods_dir == NULL || mods_dir[0] == '\0') return refuse(why, why_cap, "the installed track has no folder");
    if (car != NULL && car->kind != EXPORT_DRIVABLE) return refuse(why, why_cap, "an installed track has no traffic here: choose a City, Coastal or Alpine stage for a traffic or police car");
    const int ok = start_common(job, game, cd, project, car, id, mods_dir, night, look, RACE_JOB_RACE, why, why_cap);
    if (ok) job_say(job, "Test drive on the installed track %s, %s. A scratch player folder.\n", id, night ? "night" : "day");
    return ok;
}

int car_race_start_photo(RaceJob *job, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, int night, const RaceLook *look,
                         char *why, size_t why_cap)
{
    const int traffic = car != NULL && car->kind != EXPORT_DRIVABLE;
    const int ok = start_common(job, game, cd, project, car, track, NULL, night, look, traffic ? RACE_JOB_PROBE : RACE_JOB_SHOOT, why, why_cap);
    if (ok) job_say(job, "In-game photo: %s on %s, %s.\n", car->label, car_race_track_label(track), night ? "night" : "day");
    return ok;
}

static void finish(RaceJob *j, int ok, const char *format, ...) __attribute__((format(printf, 3, 4)));
static void finish(RaceJob *j, int ok, const char *format, ...)
{
    char off[120], tail[124] = "";   /* T6: what was switched off, so a picture is not taken for the whole look; it always fits, the message gives way */
    car_race_look_off(&j->look, off, sizeof off);
    if (off[0] != '\0') (void)snprintf(tail, sizeof tail, " (%s)", off);   /* off < 120 bytes: tail holds it whole */
    const size_t room = sizeof j->summary - strlen(tail);
    va_list args;
    va_start(args, format);
    vsnprintf(j->summary, room, format, args);
    va_end(args);
    memcpy(j->summary + strlen(j->summary), tail, strlen(tail) + 1);
    j->ok = ok;
    j->finished = 1;
    j->state = RACE_JOB_IDLE;
    car_race_temp_remove(j->temp);
    j->temp[0] = '\0';
    job_say(j, "%s\n", j->summary);
}

/* the game's output and own log of a run that ended, into the job's log */
static char *collect(RaceJob *j, size_t *game_len)
{
    if (j->run.log != NULL && j->run.log_len > 0) job_log(j, j->run.log, j->run.log_len);
    char *text = take_game_log(j, game_len);
    if (text != NULL && *game_len > 0) job_log(j, text, *game_len);
    return text;
}

/* the game's line on the followed car (drive_run_shot.c): 1 on the road at the frame (`frame` the tick), 0 parked off the road, -1 no line (a game from before it) */
static int follow_line(const char *log, unsigned *frame)
{
    const char *at = log != NULL ? strstr(log, "follow: slot ") : NULL;
    if (at == NULL) return -1;
    const char *nl = strchr(at, '\n'), *frame_at = strstr(at, "the frame at tick ");
    if (strstr(at, "is parked off the road") != NULL && (nl == NULL || strstr(at, "is parked off the road") < nl)) return 0;
    if (frame_at == NULL || (nl != NULL && frame_at > nl) || sscanf(frame_at, "the frame at tick %u", frame) != 1) return -1;
    return 1;
}

static void next_draw(RaceJob *j, const char *why_not);

static void photo_done(RaceJob *j)
{
    uint8_t *rgba = NULL;
    int w = 0, h = 0;
    char why[200];
    unsigned frame = 0;
    if (j->run.exit_code != 0) { finish(j, 0, "The game ended with exit code %d: no photo. Its output is in the log.", j->run.exit_code); return; }
    const int follow = j->setup.slot > 0 && j->setup.car->kind == EXPORT_TRAFFIC ? follow_line(j->run.log, &frame) : -1;   /* the wait is asked for a traffic car only */
    if (follow == 0) { next_draw(j, "the car stayed parked off the road (it waits there until the player comes near)"); return; }
    if (!car_race_read_ppm(j->own[4], &rgba, &w, &h, why, sizeof why)) { finish(j, 0, "No photo: %s.", why); return; }
    free(j->photo);
    j->photo = rgba;
    j->photo_w = w, j->photo_h = h;
    j->photo_serial++;
    j->photo_slot = j->setup.slot, j->photo_night = j->setup.night, j->photo_seed = j->setup.seed;
    snprintf(j->photo_track, sizeof j->photo_track, "%s", j->track);
    if (j->setup.car->kind == EXPORT_DRIVABLE) {
        char view[80] = "";   /* from the dash, and the gauge light it was given */
        if (j->setup.cockpit && j->setup.gauge_set) snprintf(view, sizeof view, ", from the cockpit, gauge light %d", j->setup.gauge_light);
        else if (j->setup.cockpit) snprintf(view, sizeof view, ", from the cockpit");
        finish(j, 1, "Photo: %s on %s, %s%s.", j->setup.car->label, car_race_track_label(j->track), j->setup.night ? "night" : "day", view);
    }
    else if (follow == 1) finish(j, 1, "Photo: the %s on the road (slot %d of the traffic draw %u) on %s, %s, %u ticks in.", j->setup.car->label, j->setup.slot,
                                 (unsigned)j->setup.seed, car_race_track_label(j->track), j->setup.night ? "night" : "day", frame);
    else finish(j, 1, "Photo: the camera follows the %s (slot %d of the traffic draw %u) on %s, %s, %d ticks in. Studio cannot see whether it is on screen: look.", j->setup.car->label,
                j->setup.slot, (unsigned)j->setup.seed, car_race_track_label(j->track), j->setup.night ? "night" : "day", car_race_photo_ticks(j->setup.car));
}

/* a probe ended: is the car in the draw? */
static void probe_done(RaceJob *j, const char *game_log)
{
    if (j->run.exit_code != 0) { finish(j, 0, "The game ended with exit code %d while looking for the %s in the traffic. Its output is in the log.", j->run.exit_code, j->setup.car->label); return; }
    if (game_log == NULL || strstr(game_log, "race car: slot") == NULL) {   /* an older game: it does not say who is in which slot */
        finish(j, 0, "No photo: this game does not say which car is in which slot of the traffic (it is older than this Studio). Test drive (the window) still runs.");
        return;
    }
    const int slot = car_race_find_slot(game_log, j->setup.car->file);
    char why[300];
    if (slot < 0) {
        next_draw(j, "the car is not in it");
        return;
    }
    job_say(j, "Traffic draw %u: the car is in it.\n", (unsigned)j->setup.seed);
    j->setup.slot = slot;
    j->setup.photo_ticks = car_race_photo_ticks(j->setup.car);
    j->state = RACE_JOB_SHOOT;
    if (!launch(j, why, sizeof why)) finish(j, 0, "%s", why);
}

/* the draw tried has no photo of the car (`why_not`): a probe of the next one, or the end when every one was tried */
static void next_draw(RaceJob *j, const char *why_not)
{
    char why[300];
    job_say(j, "Traffic draw %u: %s.\n", (unsigned)j->setup.seed, why_not);
    if (j->setup.slot > 0) j->parked++;
    if (++j->probes > RACE_SEEDS || j->parked >= RACE_PARKED_DRAWS) {
        if (j->parked)
            finish(j, 0, "No photo: in the %d draws of the traffic that had it, the %s stayed parked off the road until the frame. Test drive (the window) may still show it.",
                   j->parked, j->setup.car->label);
        else
            finish(j, 0, "No photo: the %s was in none of %d draws of the traffic, so there is no tick where it is on screen. Test drive (the window) may still show it.",
                   j->setup.car->label, j->probes);
        return;
    }
    j->setup.seed = RACE_SEED_FIRST + (uint32_t)(j->probes - 1);
    j->setup.slot = 0;
    j->setup.photo_ticks = 1;
    j->state = RACE_JOB_PROBE;
    if (!launch(j, why, sizeof why)) finish(j, 0, "%s", why);
}

/* the game's line on the piece (track_pieces.c): "track piece: <id> is track N" or "track piece: <id> refused: ..."; kept in the job when the log has it */
/* `whole`: only a line the game has ended (while it runs, the last line may be half written: "track 1" of "track 12") */
static void piece_seen(RaceJob *j, const char *log, int whole)
{
    char head[100];
    if (log == NULL || j->piece_track != 0 || snprintf(head, sizeof head, "track piece: %s ", j->piece) >= (int)sizeof head) return;
    const char *at = strstr(log, head);
    if (at == NULL) return;
    const char *nl = strchr(at, '\n');
    if (whole && nl == NULL) return;
    const size_t len = nl != NULL ? (size_t)(nl - at) : strlen(at);
    snprintf(j->piece_line, sizeof j->piece_line, "%.*s", (int)(len < sizeof j->piece_line ? len : sizeof j->piece_line - 1), at);
    int track = 0;
    if (sscanf(at + strlen(head), "is track %d", &track) == 1 && track > 0) j->piece_track = track;
    else if (strncmp(at + strlen(head), "refused", 7) == 0) j->piece_track = -1;
}

/* while the game runs, its log now and then (it writes it line by line), until it says the piece's line */
static void piece_watch(RaceJob *j)
{
    if (j->piece[0] == '\0' || j->piece_track != 0 || ++j->piece_polls < RACE_PIECE_POLLS) return;
    j->piece_polls = 0;
    char path[EXPORT_PATH + 32];
    if (snprintf(path, sizeof path, "%s/spirited-drive.log", j->own[3]) >= (int)sizeof path) return;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return;
    char *text = malloc(PROC_LOG_MAX + 1);
    if (text != NULL) {
        const size_t n = fread(text, 1, PROC_LOG_MAX, f);
        text[n] = '\0';
        piece_seen(j, text, 1);
    }
    free(text);
    fclose(f);
}

/* a test drive ended: what the game said of the piece */
static void drive_done(RaceJob *j, const char *game_log)
{
    piece_seen(j, game_log, 0);
    const char *ended = j->run.stopped ? "Stopped" : j->run.exit_code == 0 ? "The game ended" : "The game ended with an error";
    if (j->piece_track > 0) finish(j, j->run.stopped || j->run.exit_code == 0, "%s. The game took %s as track %d, driven with the %s.", ended, j->piece, j->piece_track, j->setup.car->label);
    else if (j->piece_track < 0) finish(j, 0, "%s. The game says: %s.", ended, j->piece_line + strlen("track piece: "));
    else if (j->run.stopped) finish(j, 0, "Stopped before the game said whether it took %s.", j->piece);
    else finish(j, 0, "%s before the game said whether it took %s (exit code %d). Its output is in the log.", ended, j->piece, j->run.exit_code);
}

/* a test drive's install: while it runs nothing else; ended, its output into the log, then the game, or the end of the job saying why not */
static void install_step(RaceJob *j)
{
    char why[300];
    if (install_job_poll(&j->install)) return;
    if (j->install.run.log != NULL && j->install.run.log_len > 0) job_log(j, j->install.run.log, j->install.run.log_len);
    if (j->install.run.stopped) { finish(j, 0, "Stopped while the game installed %s.", j->piece); return; }
    if (!j->install.ok) { finish(j, 0, "The game did not install %s: %s. Its output is in the log.", j->piece, j->install.why); return; }
    j->state = RACE_JOB_RACE;
    if (!launch(j, why, sizeof why)) finish(j, 0, "%s", why);
}

int car_race_installing(const RaceJob *j)
{
    return j->state == RACE_JOB_INSTALL;
}

int car_race_poll(RaceJob *j)
{
    if (j->state == RACE_JOB_IDLE) return 0;
    if (j->state == RACE_JOB_INSTALL) {
        install_step(j);
        return j->state != RACE_JOB_IDLE;
    }
    if (proc_poll(&j->run)) {
        piece_watch(j);
        return 1;
    }
    size_t n = 0;
    char *game_log = collect(j, &n);
    const RaceJobState was = j->state;
    if (j->piece[0] != '\0') drive_done(j, game_log);
    else if (j->run.stopped && was != RACE_JOB_RACE) finish(j, 0, "Stopped.");
    else if (was == RACE_JOB_RACE) finish(j, j->run.exit_code == 0 || j->run.stopped, j->run.stopped ? "Stopped." : j->run.exit_code == 0 ? "The game ended." : "The game ended with exit code %d.", j->run.exit_code);
    else if (was == RACE_JOB_PROBE) probe_done(j, game_log);
    else photo_done(j);
    free(game_log);
    return j->state != RACE_JOB_IDLE;
}

void car_race_stop(RaceJob *j)
{
    if (j->state == RACE_JOB_INSTALL) install_job_stop(&j->install);
    else proc_stop(&j->run);
}

int car_race_busy(const RaceJob *j)
{
    return j->state != RACE_JOB_IDLE;
}

void car_race_free(RaceJob *j)
{
    proc_free(&j->run);
    install_job_free(&j->install);
    car_race_command_free(&j->command);
    if (j->temp[0] != '\0') car_race_temp_remove(j->temp);
    free(j->photo);
    free(j->log);
    memset(j, 0, sizeof *j);
    j->run.fd = -1;
    install_job_init(&j->install);
}

#define CARD_MAX (64 * 1024)   /* a card is a few lines: more is not one */

static int read_card(const char *dir, ModInfo *info)
{
    char path[EXPORT_PATH + 16];
    if (snprintf(path, sizeof path, "%s/mod.ini", dir) >= (int)sizeof path) return 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    uint8_t *text = malloc(CARD_MAX);
    const size_t n = text != NULL ? fread(text, 1, CARD_MAX, f) : 0;
    fclose(f);
    char why[200];
    const int ok = text != NULL && n > 0 && n < CARD_MAX && mod_ini_parse(text, n, info, why, sizeof why);
    free(text);
    return ok;
}

static int by_label(const void *a, const void *b)
{
    const RaceModTrack *x = a, *y = b;
    const int c = strcasecmp(x->label, y->label);
    return c != 0 ? c : strcmp(x->id, y->id);
}

int race_mods_scan(const char *user, RaceModTrack *out, int cap)
{
    char mods[EXPORT_PATH];
    if (user == NULL || user[0] == '\0' || snprintf(mods, sizeof mods, "%s/mods", user) >= (int)sizeof mods) return 0;
    DIR *d = opendir(mods);
    if (d == NULL) return 0;
    int n = 0;
    for (struct dirent *e; n < cap && (e = readdir(d)) != NULL;) {
        if (e->d_name[0] == '.') continue;
        RaceModTrack *t = &out[n];
        ModInfo info;
        memset(&info, 0, sizeof info);
        if (snprintf(t->dir, sizeof t->dir, "%s/%s", mods, e->d_name) >= (int)sizeof t->dir || !read_card(t->dir, &info) || info.type != MOD_TYPE_TRACK) continue;
        if (strcmp(info.id, e->d_name) != 0) continue;   /* the game finds an installed mod by its folder's name: a card that says another id is not that */
        snprintf(t->id, sizeof t->id, "%s", info.id);
        snprintf(t->label, sizeof t->label, "%s", info.name[0] != '\0' ? info.name : info.id);
        n++;
    }
    closedir(d);
    qsort(out, (size_t)n, sizeof *out, by_label);
    return n;
}

#include "car_race.h"

#include "formats/car_piece.h"

#include <ctype.h>
#include <errno.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

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
                 "draws at random for each race: look for it on the road, ahead of you or coming the other way. Some races do not have it; Race photo finds one that does.",
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

int car_race_command(const RaceSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    const int traffic = s->car != NULL && s->car->kind != EXPORT_DRIVABLE;
    const int ti = car_race_track_find(s->track);
    if (s->car == NULL || s->game == NULL || s->cd == NULL || s->project == NULL || s->user == NULL || s->track == NULL) return refuse(why, why_cap, "the race is not set up");
    if (ti < 0) return refuse(why, why_cap, "%s is not a track of the list", s->track);
    if (traffic && !TRACKS[ti].stage) return refuse(why, why_cap, "%s has no traffic: choose a City, Coastal or Alpine stage for a traffic or police car", TRACKS[ti].label);
    if (s->photo_ticks > 0 && s->shot == NULL) return refuse(why, why_cap, "no file for the photo");
    int ok = add_arg(out, "%s", s->game) && add_arg(out, "--data") && add_arg(out, "%s", s->cd) && add_arg(out, "--user") && add_arg(out, "%s", s->user) &&
             add_arg(out, "--drive") && add_arg(out, "%s", TRACKS[ti].stem);
    const int model = traffic ? RACE_TRAFFIC_PLAYER_MODEL : s->car->model;   /* the player's car; for a traffic or police car the traffic is the test */
    const int number = s->photo_ticks > 0 ? model : car_race_menu_pick(model);   /* a photo takes the model, a race in a window the menu's number */
    if (number < 0) {
        car_race_command_free(out);
        return refuse(why, why_cap, "%s has no number in the game's car menu", s->car->label);
    }
    ok = ok && add_arg(out, "--car") && add_arg(out, "%d", number);
    if (traffic) ok = ok && add_arg(out, "--type") && add_arg(out, "1");
    else ok = ok && add_arg(out, "--racers") && add_arg(out, "1");
    if (s->photo_ticks > 0) ok = ok && add_arg(out, "--node") && add_arg(out, "%d", s->photo_ticks) && add_arg(out, "--screenshot") && add_arg(out, "%s", s->shot);
    for (char *const *e = base; ok && e != NULL && *e != NULL; ++e) {
        if (strncmp(*e, "TNFS_", 5) == 0 || (s->photo_ticks > 0 && strncmp(*e, "SDL_", 4) == 0)) continue;
        ok = add_env(out, "%s", *e);
    }
    ok = ok && add_env(out, "TNFS_CAR_MASKS=%s/car_masks", s->project);
    if (!traffic) ok = ok && add_env(out, "TNFS_DASH_MASKS=%s/dash_masks", s->project);
    if (s->photo_ticks > 0)   /* headless, the follow camera behind the car as the car card's picture has it */
        ok = ok && add_env(out, "SDL_VIDEO_DRIVER=dummy") && add_env(out, "SDL_RENDER_DRIVER=software") && add_env(out, "SDL_AUDIO_DRIVER=dummy") &&
             add_env(out, "SDL_AUDIODRIVER=dummy") && add_env(out, "TNFS_VIEW=3");
    if (s->car->kind == EXPORT_POLICE && s->photo_ticks > 0) ok = ok && add_env(out, "TNFS_POLICE_NEAR=%d", RACE_POLICE_NEAR);   /* chasing the player from behind: a camera on it sees it */
    if (s->slot > 0) ok = ok && add_env(out, "TNFS_FOLLOW_CAR=%d", s->slot);
    if (s->slot > 0 && s->photo_ticks > 0 && s->car->kind == EXPORT_TRAFFIC) ok = ok && add_env(out, "TNFS_FOLLOW_SETTLE=%d", RACE_FOLLOW_SETTLE);   /* parked until the player nears */
    if (s->seed != 0) ok = ok && add_env(out, "TNFS_DRIVE_SEED=%u", (unsigned)s->seed);
    if (ok) return 1;
    car_race_command_free(out);
    return refuse(why, why_cap, "the command is too long");
}

int car_race_user_files(const char *user, int night, char *why, size_t why_cap)
{
    char path[EXPORT_PATH + 32];
    static const char PORT[] = "profile=restomod\n";
    const char *const parts[2] = { PORT, night ? "extra_liveries=0\nrender_scale=1\nlight_time=night\nlight_effects=1\n" : "extra_liveries=0\nrender_scale=1\nlight_time=day\n" };
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
    if (got < 12 || sscanf(head, "P6 %d %d %d%n", &width, &height, &max, &used) != 3 || max != 255 || width != RACE_PHOTO_W || height != RACE_PHOTO_H) {
        fclose(f);
        return refuse(why, why_cap, "the game's picture is not a %dx%d PPM", RACE_PHOTO_W, RACE_PHOTO_H);
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
        if (c->masks_only ? strcasecmp(c->folder, car->folder) == 0 : (car->kind == EXPORT_DRIVABLE && strcasecmp(c->info.stem, car->file) == 0)) return car;
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
    s->game = j->own[0], s->cd = j->own[1], s->project = j->own[2], s->user = j->own[3], s->shot = j->own[4], s->track = j->track;
    char shot[EXPORT_PATH + 32];
    snprintf(shot, sizeof shot, "%s/frame.ppm", j->temp);
    snprintf(j->own[4], sizeof j->own[4], "%s", shot);
    car_race_command_free(&j->command);   /* a probe's words, before the next run's */
    if (!car_race_command(s, environ, &j->command, why, why_cap)) return 0;
    return proc_start(&j->run, j->command.argv, j->command.envp, NULL, why, why_cap);
}

static int start_common(RaceJob *j, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, int night, RaceJobState state,
                        char *why, size_t why_cap)
{
    if (j->state != RACE_JOB_IDLE || j->run.running) return refuse(why, why_cap, "a race is already running");
    struct stat st;
    if (game == NULL || stat(game, &st) != 0 || !S_ISREG(st.st_mode)) return refuse(why, why_cap, "the game's program is not where Studio looked (%s): use Locate game", game != NULL ? game : "nowhere");
    if (car == NULL) return refuse(why, why_cap, "this car is not one of the game's: Test in race is for the cars Export original car makes");
    if (car_race_track_find(track) < 0) return refuse(why, why_cap, "no such track");
    if (strlen(game) >= EXPORT_PATH || strlen(cd) >= EXPORT_PATH || strlen(project) >= EXPORT_PATH) return refuse(why, why_cap, "a path is too long");
    proc_free(&j->run);
    car_race_command_free(&j->command);
    free(j->photo), free(j->log);
    const int serial = j->photo_serial;
    memset(j, 0, sizeof *j);
    j->run.fd = -1;
    j->photo_serial = serial;
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
    snprintf(j->track, sizeof j->track, "%s", track);
    j->setup.car = car, j->setup.night = night, j->setup.photo_ticks = state == RACE_JOB_RACE ? 0 : state == RACE_JOB_PROBE ? 1 : car_race_photo_ticks(car);
    j->state = state;
    if (!car_race_user_files(user, night, why, why_cap) || !launch(j, why, why_cap)) {
        car_race_temp_remove(j->temp);
        free(j->log), j->log = NULL;
        j->state = RACE_JOB_IDLE;
        return 0;
    }
    return 1;
}

int car_race_start_race(RaceJob *job, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, int night, char *why, size_t why_cap)
{
    const int ok = start_common(job, game, cd, project, car, track, night, RACE_JOB_RACE, why, why_cap);
    if (ok) {
        char note[500];
        car_race_note(car, track, note, sizeof note);
        job_say(job, "Test in race, %s. %s A scratch player folder.\n", night ? "night" : "day", note);
    }
    return ok;
}

int car_race_start_photo(RaceJob *job, const char *game, const char *cd, const char *project, const ExportCar *car, const char *track, int night, char *why, size_t why_cap)
{
    const int traffic = car != NULL && car->kind != EXPORT_DRIVABLE;
    const int ok = start_common(job, game, cd, project, car, track, night, traffic ? RACE_JOB_PROBE : RACE_JOB_SHOOT, why, why_cap);
    if (ok) job_say(job, "Race photo: %s on %s, %s.\n", car->label, track, night ? "night" : "day");
    return ok;
}

static void finish(RaceJob *j, int ok, const char *format, ...) __attribute__((format(printf, 3, 4)));
static void finish(RaceJob *j, int ok, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(j->summary, sizeof j->summary, format, args);
    va_end(args);
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
    const int follow = j->setup.slot > 0 ? follow_line(j->run.log, &frame) : -1;
    if (follow == 0) { next_draw(j, "the car stayed parked off the road (it waits there until the player comes near)"); return; }
    if (!car_race_read_ppm(j->own[4], &rgba, &w, &h, why, sizeof why)) { finish(j, 0, "No photo: %s.", why); return; }
    free(j->photo);
    j->photo = rgba;
    j->photo_serial++;
    j->photo_slot = j->setup.slot, j->photo_night = j->setup.night, j->photo_seed = j->setup.seed;
    snprintf(j->photo_track, sizeof j->photo_track, "%s", j->track);
    if (j->setup.car->kind == EXPORT_DRIVABLE) finish(j, 1, "Photo: %s on %s, %s.", j->setup.car->label, j->track, j->setup.night ? "night" : "day");
    else if (follow == 1) finish(j, 1, "Photo: the %s on the road (slot %d of the traffic draw %u) on %s, %s, %u ticks in.", j->setup.car->label, j->setup.slot,
                                 (unsigned)j->setup.seed, j->track, j->setup.night ? "night" : "day", frame);
    else finish(j, 1, "Photo: the camera follows the %s (slot %d of the traffic draw %u) on %s, %s, %d ticks in. Studio cannot see whether it is on screen: look.", j->setup.car->label,
                j->setup.slot, (unsigned)j->setup.seed, j->track, j->setup.night ? "night" : "day", car_race_photo_ticks(j->setup.car));
}

/* a probe ended: is the car in the draw? */
static void probe_done(RaceJob *j, const char *game_log)
{
    if (j->run.exit_code != 0) { finish(j, 0, "The game ended with exit code %d while looking for the %s in the traffic. Its output is in the log.", j->run.exit_code, j->setup.car->label); return; }
    if (game_log == NULL || strstr(game_log, "race car: slot") == NULL) {   /* an older game: it does not say who is in which slot */
        finish(j, 0, "No photo: this game does not say which car is in which slot of the traffic (it is older than this Studio). Test in race (the window) still runs.");
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
    if (j->setup.slot > 0) j->parked = 1;
    if (++j->probes > RACE_SEEDS) {
        if (j->parked)
            finish(j, 0, "No photo: in every draw of the traffic that had it, the %s stayed parked off the road until the frame. Test in race (the window) may still show it.",
                   j->setup.car->label);
        else
            finish(j, 0, "No photo: the %s was in none of %d draws of the traffic, so there is no tick where it is on screen. Test in race (the window) may still show it.",
                   j->setup.car->label, j->probes);
        return;
    }
    j->setup.seed = RACE_SEED_FIRST + (uint32_t)(j->probes - 1);
    j->setup.slot = 0;
    j->setup.photo_ticks = 1;
    j->state = RACE_JOB_PROBE;
    if (!launch(j, why, sizeof why)) finish(j, 0, "%s", why);
}

int car_race_poll(RaceJob *j)
{
    if (j->state == RACE_JOB_IDLE) return 0;
    if (proc_poll(&j->run)) return 1;
    size_t n = 0;
    char *game_log = collect(j, &n);
    const RaceJobState was = j->state;
    if (j->run.stopped && was != RACE_JOB_RACE) finish(j, 0, "Stopped.");
    else if (was == RACE_JOB_RACE) finish(j, j->run.exit_code == 0 || j->run.stopped, j->run.stopped ? "Stopped." : j->run.exit_code == 0 ? "The game ended." : "The game ended with exit code %d.", j->run.exit_code);
    else if (was == RACE_JOB_PROBE) probe_done(j, game_log);
    else photo_done(j);
    free(game_log);
    return j->state != RACE_JOB_IDLE;
}

void car_race_stop(RaceJob *j)
{
    proc_stop(&j->run);
}

int car_race_busy(const RaceJob *j)
{
    return j->state != RACE_JOB_IDLE;
}

void car_race_free(RaceJob *j)
{
    proc_free(&j->run);
    car_race_command_free(&j->command);
    if (j->temp[0] != '\0') car_race_temp_remove(j->temp);
    free(j->photo);
    free(j->log);
    memset(j, 0, sizeof *j);
    j->run.fd = -1;
}

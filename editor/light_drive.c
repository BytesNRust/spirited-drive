/* The Track light window's test drive job (light_drive.h). */
#include "light_drive.h"

#include "light_photo.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern char **environ;

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

static const char *const WORDS[LIGHT_DRIVE_WEATHERS] = { "clear", "fog", "rain", "storm" };
static const char *const NAMES[LIGHT_DRIVE_WEATHERS] = { "Clear", "Fog", "Rain", "Storm" };

const char *light_drive_weather_word(int weather) { return weather >= 0 && weather < LIGHT_DRIVE_WEATHERS ? WORDS[weather] : NULL; }
const char *light_drive_weather_name(int weather) { return weather >= 0 && weather < LIGHT_DRIVE_WEATHERS ? NAMES[weather] : NULL; }
int light_drive_weather_clamp(int weather) { return weather >= 0 && weather < LIGHT_DRIVE_WEATHERS ? weather : 0; }

static int g_weather_pref;
void light_drive_weather_pref_set(int weather) { g_weather_pref = light_drive_weather_clamp(weather); }
int light_drive_weather_pref(void) { return g_weather_pref; }

static int stem_ok(const char *track)
{
    return track != NULL && track[0] >= 'A' && track[0] <= 'Z' && track[1] >= 'A' && track[1] <= 'Z' && track[2] >= '0' && track[2] <= '9' && track[3] == '\0';
}

static int push(char **list, int *count, int cap, const char *format, ...) __attribute__((format(printf, 4, 5)));
static int push(char **list, int *count, int cap, const char *format, ...)
{
    char buf[RACE_WORD_MAX];
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    if (*count >= cap || n < 0 || n >= (int)sizeof buf) return 0;
    list[*count] = strdup(buf);
    if (list[*count] == NULL) return 0;
    list[++*count] = NULL;
    return 1;
}

static int check_setup(const char *track, char letter, int weather, char *why, size_t cap)
{
    if (!stem_ok(track)) return refuse(why, cap, "%s is not a CD track's stem such as CY1 or TR7", track != NULL ? track : "(none)");
    if (light_photo_time_word(letter) == NULL) return refuse(why, cap, "the time of day is A, B, C or N");
    if (light_drive_weather_word(weather) == NULL) return refuse(why, cap, "the weather is 0 to %d", LIGHT_DRIVE_WEATHERS - 1);
    return 1;
}

int light_drive_command(const LightDriveSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (s->game == NULL || s->game[0] == '\0' || s->cd == NULL || s->cd[0] == '\0' || s->user == NULL || s->user[0] == '\0' || s->mods == NULL || s->mods[0] == '\0')
        return refuse(why, why_cap, "the test drive is not set up (the game, its files or the scratch folders are missing)");
    if (!check_setup(s->track, s->letter, s->weather, why, why_cap)) return 0;
    char **const a = out->argv, **const e = out->envp;
    int *const ac = &out->argc, *const ec = &out->envc;
    int ok = push(a, ac, RACE_ARGS, "%s", s->game) && push(a, ac, RACE_ARGS, "--data") && push(a, ac, RACE_ARGS, "%s", s->cd) && push(a, ac, RACE_ARGS, "--user") &&
             push(a, ac, RACE_ARGS, "%s", s->user) && push(a, ac, RACE_ARGS, "--mods") && push(a, ac, RACE_ARGS, "%s", s->mods) && push(a, ac, RACE_ARGS, "--drive") &&
             push(a, ac, RACE_ARGS, "%s", s->track);
    for (char *const *v = base; ok && v != NULL && *v != NULL; ++v)
        if (strncmp(*v, "TNFS_", 5) != 0 && strncmp(*v, "SDL_", 4) != 0) ok = push(e, ec, RACE_ENVS, "%s", *v);
    ok = ok && push(e, ec, RACE_ENVS, "TNFS_LIGHT_EDIT=1") && push(e, ec, RACE_ENVS, "TNFS_WEATHER=%s", WORDS[s->weather]) && push(e, ec, RACE_ENVS, "TNFS_LIGHT_TIME=%s", light_photo_time_word(s->letter)) &&
         push(e, ec, RACE_ENVS, "TNFS_START_NODE=%u", s->node) && (s->masks == NULL || s->masks[0] == '\0' || push(e, ec, RACE_ENVS, "TNFS_TRACK_MASKS=%s", s->masks)) &&
         (s->window == NULL || s->window[0] == '\0' || push(e, ec, RACE_ENVS, "TNFS_WINDOW_POS=%s", s->window));
    if (ok) return 1;
    car_race_command_free(out);
    return refuse(why, why_cap, "the command is too long");
}

static void finish(LightDriveJob *job)
{
    proc_free(&job->run);
    car_race_command_free(&job->command);
    if (job->temp[0] != '\0') car_race_temp_remove(job->temp);
    job->temp[0] = '\0';
}

static void last_line(const ProcRun *run, char *out, size_t cap)
{
    out[0] = '\0';
    if (run->log == NULL || run->log_len == 0 || cap == 0) return;
    size_t end = run->log_len;
    while (end > 0 && (run->log[end - 1] == '\n' || run->log[end - 1] == '\r')) --end;
    size_t start = end;
    while (start > 0 && run->log[start - 1] != '\n') --start;
    size_t n = end - start;
    if (n >= cap) n = cap - 1;
    memcpy(out, run->log + start, n);
    out[n] = '\0';
}

/* `text` as `path`: a temporary file next to it, then a rename, so a reader sees the old file or the new one and never half of one */
static int write_whole(const char *path, const char *text, size_t size, char *why, size_t cap)
{
    char tmp[EXPORT_PATH + 100];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return refuse(why, cap, "the path is too long: %s", path);
    FILE *f = fopen(tmp, "wb");
    const int wrote = f != NULL && fwrite(text, 1, size, f) == size;
    const int closed = f != NULL && fclose(f) == 0;
    if (!wrote || !closed || rename(tmp, path) != 0) {
        const int err = errno;
        unlink(tmp);
        return refuse(why, cap, "cannot write %s: %s", path, strerror(err));
    }
    return 1;
}

static int mods_file(const LightDriveJob *job, char light[EXPORT_PATH + 100])
{
    return snprintf(light, EXPORT_PATH + 100, "%s/track_light/%s.txt", job->mods, job->track) < EXPORT_PATH + 100;
}

int light_drive_start(LightDriveJob *job, const char *game, const char *cd, const char *track, char letter, unsigned node, int weather, const char *file_text, size_t file_size,
                      char *why, size_t why_cap)
{
    if (job->state == LIGHT_DRIVE_RUNNING) light_drive_stop(job);
    job->state = LIGHT_DRIVE_FAILED;
    job->why[0] = '\0';
    char local[LIGHT_DRIVE_WHY];
    if (why == NULL) why = local, why_cap = sizeof local;
    why[0] = '\0';
    int ok = 0;
    if (file_text == NULL || file_size == 0) {
        refuse(why, why_cap, "there is no light file to drive with");
    } else if (game == NULL || access(game, X_OK) != 0) {
        refuse(why, why_cap, "the game's program is not there: %s", game != NULL ? game : "(none)");
    } else if (check_setup(track, letter, weather, why, why_cap)) {
        char user[EXPORT_PATH], path[EXPORT_PATH + 64];
        job->temp[0] = '\0';
        if (car_race_temp(job->temp, sizeof job->temp, user, sizeof user, why, why_cap)) {
            snprintf(job->mods, sizeof job->mods, "%s/mods", job->temp);
            snprintf(job->track, sizeof job->track, "%s", track);
            job->letter = letter;
            char light[EXPORT_PATH + 100];
            if (snprintf(path, sizeof path, "%s/track_light", job->mods) >= (int)sizeof path || mkdir(job->mods, 0777) != 0 || mkdir(path, 0777) != 0 || !mods_file(job, light)) {
                refuse(why, why_cap, "cannot make the scratch folders: %s", strerror(errno));
            } else {
                LightDriveSetup s = { game, cd, user, job->mods, { 0 }, letter, node, weather, NULL, job->window };
                snprintf(s.track, sizeof s.track, "%s", track);
                ok = write_whole(light, file_text, file_size, why, why_cap);
                char masks[EXPORT_PATH + 16];
                masks[0] = '\0';
                if (ok && job->masks[0] != '\0') {   // the track's masks of the workspace in front of the game's: <scratch>/masks/<TRACK>/<name>.png
                    snprintf(masks, sizeof masks, "%s/masks", job->temp);
                    ok = light_photo_copy_masks(job->masks, masks, track, why, why_cap);
                    s.masks = masks;
                }
                ok = ok && car_race_user_files(user, letter == 'N', NULL, why, why_cap) && car_race_player_options(user, why, why_cap) && light_drive_command(&s, environ, &job->command, why, why_cap) &&
                     proc_start(&job->run, job->command.argv, job->command.envp, NULL, why, why_cap);
            }
        }
    }
    if (!ok) {
        finish(job);
        snprintf(job->why, sizeof job->why, "%s", why);
        return 0;
    }
    job->node = node;
    job->weather = weather;
    job->state = LIGHT_DRIVE_RUNNING;
    return 1;
}

int light_drive_update(LightDriveJob *job, const char *file_text, size_t file_size, char *why, size_t why_cap)
{
    if (job->state != LIGHT_DRIVE_RUNNING || job->temp[0] == '\0') return refuse(why, why_cap, "no test drive is running");
    if (file_text == NULL || file_size == 0) return refuse(why, why_cap, "there is no light file to drive with");
    char light[EXPORT_PATH + 100];
    if (!mods_file(job, light)) return refuse(why, why_cap, "the path is too long");
    return write_whole(light, file_text, file_size, why, why_cap);
}

int light_drive_poll(LightDriveJob *job)
{
    if (job->state != LIGHT_DRIVE_RUNNING) return 0;
    if (proc_poll(&job->run)) return 1;
    char last[160];
    last_line(&job->run, last, sizeof last);
    const int code = job->run.exit_code;
    job->why[0] = '\0';
    if (code != 0) {
        snprintf(job->why, sizeof job->why, "the game ended with code %d%s%s", code, last[0] != '\0' ? ": " : "", last);
        job->state = LIGHT_DRIVE_FAILED;
    } else {
        job->state = LIGHT_DRIVE_ENDED;
    }
    finish(job);
    return 0;
}

void light_drive_stop(LightDriveJob *job)
{
    if (job->state != LIGHT_DRIVE_RUNNING) return;
    finish(job);
    snprintf(job->why, sizeof job->why, "stopped");
    job->state = LIGHT_DRIVE_FAILED;
}

void light_drive_free(LightDriveJob *job)
{
    light_drive_stop(job);
    finish(job);
    memset(job, 0, sizeof *job);
}

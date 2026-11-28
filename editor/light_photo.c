/* The Track light window's photo job (light_photo.h). */
#include "light_photo.h"

#include <dirent.h>
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

static const struct { const char *name; int kind; } VIEWS[LIGHT_VIEWS] = {
    { "Bumper", 1 }, { "Dash", 0 }, { "Chase near", 2 }, { "Chase mid", 3 }, { "Chase far", 4 }, { "High", 12 },
};

const char *light_photo_view_name(int view) { return view >= 0 && view < LIGHT_VIEWS ? VIEWS[view].name : NULL; }
int light_photo_view_kind(int view) { return view >= 0 && view < LIGHT_VIEWS ? VIEWS[view].kind : -1; }
int light_photo_view_ticks(int view) { return view >= 2 && view < LIGHT_VIEWS ? LIGHT_PHOTO_TICKS_CHASE : LIGHT_PHOTO_TICKS_DEFAULT; }
int light_photo_view_clamp(int view) { return view >= 0 && view < LIGHT_VIEWS ? view : LIGHT_VIEW_DEFAULT; }

static int g_view_pref = LIGHT_VIEW_DEFAULT;
void light_photo_view_pref_set(int view) { g_view_pref = light_photo_view_clamp(view); }
int light_photo_view_pref(void) { return g_view_pref; }

static int g_hud_pref;
void light_photo_hud_pref_set(int hud) { g_hud_pref = hud != 0; }
int light_photo_hud_pref(void) { return g_hud_pref; }

const char *light_photo_time_word(char letter)
{
    switch (letter) {
    case 'A': return "dawn";
    case 'B': return "day";
    case 'C': return "dusk";
    case 'N': return "night";
    default: return NULL;
    }
}

/* a CD track's stem as the light files are named: two capitals and a digit */
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

static int check_setup(const char *track, char letter, int view, int ticks, char *why, size_t cap)
{
    if (!stem_ok(track)) return refuse(why, cap, "%s is not a CD track's stem such as CY1 or TR7", track != NULL ? track : "(none)");
    if (light_photo_time_word(letter) == NULL) return refuse(why, cap, "the time of day is A, B, C or N");
    if (light_photo_view_kind(view) < 0) return refuse(why, cap, "the photo's view is 0 to %d", LIGHT_VIEWS - 1);
    if (ticks < 1 || ticks > LIGHT_PHOTO_TICKS_MAX) return refuse(why, cap, "the photo's ticks are 1 to %d", LIGHT_PHOTO_TICKS_MAX);
    return 1;
}

int light_photo_command(const LightPhotoSetup *s, char *const *base, RaceCommand *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (s->game == NULL || s->game[0] == '\0' || s->cd == NULL || s->cd[0] == '\0' || s->user == NULL || s->user[0] == '\0' || s->mods == NULL || s->mods[0] == '\0' ||
        s->shot == NULL || s->shot[0] == '\0')
        return refuse(why, why_cap, "the photo is not set up (the game, its files, the scratch folders or the picture file are missing)");
    if (!check_setup(s->track, s->letter, s->view, s->ticks, why, why_cap)) return 0;
    char **const a = out->argv, **const e = out->envp;
    int *const ac = &out->argc, *const ec = &out->envc;
    int ok = push(a, ac, RACE_ARGS, "%s", s->game) && push(a, ac, RACE_ARGS, "--data") && push(a, ac, RACE_ARGS, "%s", s->cd) && push(a, ac, RACE_ARGS, "--user") &&
             push(a, ac, RACE_ARGS, "%s", s->user) && push(a, ac, RACE_ARGS, "--mods") && push(a, ac, RACE_ARGS, "%s", s->mods) && push(a, ac, RACE_ARGS, "--drive") &&
             push(a, ac, RACE_ARGS, "%s", s->track) && push(a, ac, RACE_ARGS, "--node") && push(a, ac, RACE_ARGS, "%d", s->ticks) && push(a, ac, RACE_ARGS, "--screenshot") &&
             push(a, ac, RACE_ARGS, "%s", s->shot);
    for (char *const *v = base; ok && v != NULL && *v != NULL; ++v)
        if (strncmp(*v, "TNFS_", 5) != 0 && strncmp(*v, "SDL_", 4) != 0) ok = car_race_add_inherited(e, ec, RACE_ENVS, *v);
    ok = ok && push(e, ec, RACE_ENVS, "TNFS_AUTOPILOT=1") && push(e, ec, RACE_ENVS, "TNFS_FRAMES=1") && push(e, ec, RACE_ENVS, "TNFS_WEATHER=clear") && push(e, ec, RACE_ENVS, "TNFS_VIEW=%d", light_photo_view_kind(s->view)) && (s->hud || push(e, ec, RACE_ENVS, "TNFS_NO_HUD=1")) &&
         push(e, ec, RACE_ENVS, "TNFS_LIGHT_TIME=%s", light_photo_time_word(s->letter)) && push(e, ec, RACE_ENVS, "TNFS_START_NODE=%u", s->node) &&
         (s->masks == NULL || s->masks[0] == '\0' || push(e, ec, RACE_ENVS, "TNFS_TRACK_MASKS=%s", s->masks)) && push(e, ec, RACE_ENVS, "SDL_VIDEO_DRIVER=dummy") && push(e, ec, RACE_ENVS, "SDL_AUDIO_DRIVER=dummy") && push(e, ec, RACE_ENVS, "SDL_AUDIODRIVER=dummy");
    if (ok) return 1;
    car_race_command_free(out);
    return refuse(why, why_cap, "the command is too long");
}

/* what ends a run: the process's words, its command and its scratch folder */
static void finish(LightPhotoJob *job)
{
    proc_free(&job->run);
    car_race_command_free(&job->command);
    if (job->temp[0] != '\0') car_race_temp_remove(job->temp);
    job->temp[0] = '\0';
}

/* the last line of what the game wrote, cut to fit */
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

static int write_file(const char *path, const char *text, size_t size, char *why, size_t cap)
{
    FILE *f = fopen(path, "wb");
    const int ok = f != NULL && fwrite(text, 1, size, f) == size;
    if (f != NULL && fclose(f) != 0) return refuse(why, cap, "cannot write %s: %s", path, strerror(errno));
    return ok || refuse(why, cap, "cannot write %s: %s", path, strerror(errno));
}

/* the PNGs of `from` (a flat folder) copied to <to>/<track>/; a PNG that does not copy fails the photo, an empty or missing folder does not */
int light_photo_copy_masks(const char *from, const char *to, const char *track, char *why, size_t cap)
{
    char dir[EXPORT_PATH + 40];
    snprintf(dir, sizeof dir, "%s/%s", to, track);
    if (mkdir(to, 0777) != 0 || mkdir(dir, 0777) != 0) return refuse(why, cap, "cannot make the scratch folders: %s", strerror(errno));
    DIR *d = opendir(from);
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; e != NULL; e = readdir(d)) {
        const size_t n = strlen(e->d_name);
        if (n < 5 || e->d_name[0] == '.' || strcmp(e->d_name + n - 4, ".png") != 0) continue;
        char src[EXPORT_PATH + 300], dst[EXPORT_PATH + 340], buf[8192];
        snprintf(src, sizeof src, "%s/%s", from, e->d_name);
        snprintf(dst, sizeof dst, "%s/%s", dir, e->d_name);
        FILE *in = fopen(src, "rb"), *out = in != NULL ? fopen(dst, "wb") : NULL;
        size_t got = 0;
        int ok = in != NULL && out != NULL;
        while (ok && (got = fread(buf, 1, sizeof buf, in)) > 0) ok = fwrite(buf, 1, got, out) == got;
        if (in != NULL) fclose(in);
        if (out != NULL) ok = fclose(out) == 0 && ok;
        if (!ok) return closedir(d), refuse(why, cap, "cannot copy the track's masks: %s", e->d_name);
    }
    if (d != NULL) closedir(d);
    return 1;
}

int light_photo_start(LightPhotoJob *job, const char *game, const char *cd, const char *track, char letter, unsigned node, int view, int hud, int ticks,
                      const char *file_text, size_t file_size, char *why, size_t why_cap)
{
    if (job->state == LIGHT_PHOTO_RUNNING) light_photo_stop(job);
    job->state = LIGHT_PHOTO_FAILED;
    job->why[0] = '\0';
    char local[LIGHT_PHOTO_WHY];
    if (why == NULL) why = local, why_cap = sizeof local;
    why[0] = '\0';
    int ok = 0;
    if (file_text == NULL || file_size == 0) {
        refuse(why, why_cap, "there is no light file to photograph");
    } else if (game == NULL || access(game, X_OK) != 0) {
        refuse(why, why_cap, "the game's program is not there: %s", game != NULL ? game : "(none)");
    } else if (check_setup(track, letter, view, ticks, why, why_cap)) {
        char user[EXPORT_PATH], mods[EXPORT_PATH + 16], path[EXPORT_PATH + 64];
        job->temp[0] = '\0';
        if (car_race_temp(job->temp, sizeof job->temp, user, sizeof user, why, why_cap)) {
            snprintf(mods, sizeof mods, "%s/mods", job->temp);
            snprintf(job->shot, sizeof job->shot, "%s/photo.ppm", job->temp);
            const int dirs = snprintf(path, sizeof path, "%s/track_light", mods);
            if (dirs < 0 || (size_t)dirs >= sizeof path || mkdir(mods, 0777) != 0 || mkdir(path, 0777) != 0) {
                refuse(why, why_cap, "cannot make the scratch folders: %s", strerror(errno));
            } else {
                snprintf(path, sizeof path, "%s/track", mods);
                char file[EXPORT_PATH + 80];
                snprintf(file, sizeof file, "%s/track_light/%s.txt", mods, track);
                const LightPhotoSetup setup = { game, cd, user, mods, { 0 }, letter, node, view, hud, ticks, job->shot, NULL };
                LightPhotoSetup s = setup;
                snprintf(s.track, sizeof s.track, "%s", track);
                ok = mkdir(path, 0777) == 0 && write_file(file, file_text, file_size, why, why_cap);
                if (!ok && why[0] == '\0') refuse(why, why_cap, "cannot make the scratch folders: %s", strerror(errno));
                char masks[EXPORT_PATH + 16];
                masks[0] = '\0';
                if (ok && job->masks[0] != '\0') {   // the track's masks of the workspace in front of the game's: <scratch>/masks/<TRACK>/<name>.png
                    snprintf(masks, sizeof masks, "%s/masks", job->temp);
                    ok = light_photo_copy_masks(job->masks, masks, track, why, why_cap);
                    s.masks = masks;
                }
                ok = ok && car_race_user_files(user, letter == 'N', NULL, why, why_cap) && car_race_force_lines(user, "sun_shadow=1\n", why, why_cap) && light_photo_command(&s, environ, &job->command, why, why_cap) &&
                     proc_start(&job->run, job->command.argv, job->command.envp, NULL, why, why_cap);
            }
        }
    }
    if (!ok) {
        finish(job);
        snprintf(job->why, sizeof job->why, "%s", why);
        return 0;
    }
    snprintf(job->track, sizeof job->track, "%s", track);
    job->letter = letter;
    job->node = node;
    job->view = view;
    job->hud = hud;
    job->state = LIGHT_PHOTO_RUNNING;
    return 1;
}

int light_photo_poll(LightPhotoJob *job)
{
    if (job->state != LIGHT_PHOTO_RUNNING) return 0;
    if (proc_poll(&job->run)) return 1;
    char last[160];
    last_line(&job->run, last, sizeof last);
    const int code = job->run.exit_code;
    job->why[0] = '\0';
    uint8_t *rgba = NULL;
    int w = 0, h = 0;
    if (code != 0) {
        snprintf(job->why, sizeof job->why, "the game ended with code %d%s%s", code, last[0] != '\0' ? ": " : "", last);
        job->state = LIGHT_PHOTO_FAILED;
    } else if (!car_race_read_ppm(job->shot, &rgba, &w, &h, job->why, sizeof job->why)) {
        job->state = LIGHT_PHOTO_FAILED;
    } else {
        free(job->rgba);
        job->rgba = rgba;
        job->w = w;
        job->h = h;
        ++job->serial;
        job->state = LIGHT_PHOTO_DONE;
    }
    finish(job);
    return 0;
}

void light_photo_stop(LightPhotoJob *job)
{
    if (job->state != LIGHT_PHOTO_RUNNING) return;
    finish(job);
    snprintf(job->why, sizeof job->why, "stopped");
    job->state = LIGHT_PHOTO_FAILED;
}

void light_photo_free(LightPhotoJob *job)
{
    light_photo_stop(job);
    finish(job);
    free(job->rgba);
    memset(job, 0, sizeof *job);
}

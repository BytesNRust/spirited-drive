/* The Track light window's side of the App (app.h, light_ui.h): the mode's session, the photo and its auto-refresh. */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const TRACKS[LIGHT_UI_TRACKS] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7" };

const char *light_ui_track(int index)
{
    return index >= 0 && index < LIGHT_UI_TRACKS ? TRACKS[index] : NULL;
}

char light_ui_time_letter(int time)
{
    return time >= 0 && time < LIGHT_UI_TIMES ? "ABCN"[time] : '\0';
}

const char *light_ui_time_name(int time)
{
    static const char *const NAMES[LIGHT_UI_TIMES] = { "Dawn", "Day", "Dusk", "Night" };
    return time >= 0 && time < LIGHT_UI_TIMES ? NAMES[time] : "";
}

static void set_error(App *a, const char *text)
{
    snprintf(a->light.error, sizeof a->light.error, "%s", text);
    snprintf(a->status, sizeof a->status, "%s", text);
}

/* something the photo shows changed: it is due once the settle time passes */
static void changed(App *a)
{
    ++a->light.edit_serial;
    a->light.edit_at = a->light.now;
}

void app_set_light_dir(App *a, const char *dir)
{
    snprintf(a->light_dir, sizeof a->light_dir, "%s", dir != NULL ? dir : "");
}

/* opens (track, time) of the light folder into `ui`'s session, the old one closed only when this opens; 1, or 0 with `why` */
static int open_session(LightUi *ui, const char *dir, int track, int time, char *why, size_t cap)
{
    LightSession next;
    if (!light_session_open(&next, dir, light_ui_track(track), light_ui_time_letter(time), why, cap)) return 0;
    if (ui->open) light_session_close(&ui->session);
    ui->session = next;
    ui->open = 1;
    ui->track = track;
    ui->time = time;
    return 1;
}

int app_light_default_dir(const char *game, char *out, size_t cap)
{
    if (game == NULL || game[0] == '\0' || cap == 0) return 0;
    char base[APP_PATH];
    snprintf(base, sizeof base, "%s", game);
    char *slash = strrchr(base, '/');
    if (slash == NULL) return 0;
    *slash = '\0';   /* the program's folder */
    static const char *const WHERE[2] = { "%s/../Resources/data", "%s/data" };
    for (int i = 0; i < 2; ++i) {
        char dir[APP_PATH + 32], probe[APP_PATH + 48];
        snprintf(dir, sizeof dir, WHERE[i], base);
        snprintf(probe, sizeof probe, "%s/track_light", dir);
        struct stat st;
        if (stat(probe, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(out, cap, "%s", dir);
            return 1;
        }
    }
    return 0;
}

int app_light_set_folder(App *a, const char *dir)
{
    LightUi *ui = &a->light;
    if (a->kind != APP_KIND_LIGHT || dir == NULL || dir[0] == '\0') return 0;
    if (app_modified(a)) {
        set_error(a, "the file has unsaved changes: Save or Revert before another folder");
        return 0;
    }
    const int track = ui->open ? ui->track : 0, time = ui->open ? ui->time : 1;
    char why[APP_MSG];
    why[0] = '\0';
    if (!open_session(ui, dir, track, time, why, sizeof why)) {
        set_error(a, why);
        return 0;
    }
    snprintf(a->light_dir, sizeof a->light_dir, "%s", dir);
    ui->error[0] = '\0';
    snprintf(a->status, sizeof a->status, "Track light: %s", a->light_dir);
    app_light_set_node(a, ui->node);
    changed(a);
    return 1;
}

static const char *const NO_FOLDER = "No light folder is set: choose the folder that holds track_light/ (the game's data folder)";

int app_open_light(App *a)
{
    App *next = calloc(1, sizeof *next);
    if (next == NULL) {
        snprintf(a->open_error, sizeof a->open_error, "out of memory");
        return 0;
    }
    app_begin(next, a->game, a->user, a->generic[0] != '\0' ? a->generic : NULL);
    char why[APP_MSG];
    why[0] = '\0';
    if (a->light_dir[0] == '\0') app_light_default_dir(a->game, a->light_dir, sizeof a->light_dir);   /* the game's own light files, when it has them */
    if (a->light_dir[0] == '\0') {   /* no folder: the window opens to ask for one (Choose...) */
        snprintf(next->light.error, sizeof next->light.error, "%s", NO_FOLDER);
    } else if (!open_session(&next->light, a->light_dir, 0, 1, why, sizeof why)) {
        snprintf(a->open_error, sizeof a->open_error, "%s", why);
        snprintf(a->status, sizeof a->status, "%s", why);
        app_free(next);
        free(next);
        return 0;
    }
    next->kind = APP_KIND_LIGHT;
    app_carry_window(next, a);
    app_free(a);
    *a = *next;   /* the new state's session belongs to `a` now */
    free(next);
    snprintf(a->status, sizeof a->status, "Track light: %s", a->light_dir[0] != '\0' ? a->light_dir : "no folder set");
    changed(a);
    return 1;
}

int app_light_select(App *a, int track, int time)
{
    LightUi *ui = &a->light;
    ui->error[0] = '\0';
    if (!ui->open || light_ui_track(track) == NULL || light_ui_time_letter(time) == '\0') {
        set_error(a, "no such track or time of day");
        return 0;
    }
    if (app_modified(a)) {
        set_error(a, "the file has unsaved changes: Save or Revert before another track or time");
        return 0;
    }
    char why[APP_MSG];
    why[0] = '\0';
    if (!open_session(ui, a->light_dir, track, time, why, sizeof why)) {
        set_error(a, why);
        return 0;
    }
    app_light_set_node(a, ui->node);   /* the new file's road may be shorter */
    changed(a);
    return 1;
}

void app_light_set_node(App *a, long node)
{
    LightUi *ui = &a->light;
    const int now = (int)light_session_clamp_node(&ui->session, node);
    if (now != ui->node) {
        ui->node = now;
        changed(a);
    }
}

void app_light_touch(App *a)
{
    a->light.error[0] = '\0';
    changed(a);
}

void app_light_revert(App *a)
{
    light_session_revert(&a->light.session);
    a->light.error[0] = '\0';
    changed(a);
}

int app_light_save(App *a)
{
    char why[APP_MSG];
    why[0] = '\0';
    if (!light_session_save(&a->light.session, why, sizeof why)) {
        set_error(a, why);
        return 0;
    }
    a->light.error[0] = '\0';
    char path[LIGHT_SESSION_PATH];
    light_session_path(&a->light.session, path, sizeof path);
    snprintf(a->status, sizeof a->status, "saved %s", path);
    return 1;
}

int app_light_busy(const App *a)
{
    return a->light.photo.state == LIGHT_PHOTO_RUNNING;
}

int app_light_photo(App *a)
{
    LightUi *ui = &a->light;
    ui->photo_serial = ui->edit_serial;   /* tried: a refusal is not tried again every frame */
    if (!ui->open || !ui->session.has_file) {
        set_error(a, "there is no light file to photograph: this track's light is made in code");
        return 0;
    }
    if (a->game[0] == '\0') {
        set_error(a, "the game's program is not known: the photo is the game run headless");
        return 0;
    }
    if (a->export_src.cd[0] == '\0') {
        set_error(a, "the game's files (the CD data) are not known: the photo needs them");
        return 0;
    }
    size_t size = 0;
    char *text = light_session_text(&ui->session, &size);
    if (text == NULL) {
        set_error(a, "out of memory");
        return 0;
    }
    char why[LIGHT_PHOTO_WHY];
    const int ok = light_photo_start(&ui->photo, a->game, a->export_src.cd, ui->session.track, ui->session.letter, (unsigned)ui->node, LIGHT_PHOTO_TICKS_DEFAULT, text, size,
                                     NULL, 0, why, sizeof why);
    free(text);
    if (!ok) {
        set_error(a, why);
        return 0;
    }
    ui->error[0] = '\0';
    return 1;
}

int app_light_poll(App *a, double now)
{
    LightUi *ui = &a->light;
    ui->now = now;
    if (!ui->open) return 0;
    if (ui->photo.state == LIGHT_PHOTO_RUNNING && light_photo_poll(&ui->photo)) return 1;
    if (ui->photo.state == LIGHT_PHOTO_FAILED && ui->photo.why[0] != '\0' && strcmp(ui->photo.why, "stopped") != 0) {
        set_error(a, ui->photo.why);
        ui->photo.why[0] = '\0';   /* said once */
    }
    if (ui->session.has_file && ui->edit_serial != ui->photo_serial && now - ui->edit_at >= LIGHT_UI_SETTLE) return app_light_photo(a);
    return 0;
}

void app_light_free(App *a)
{
    light_photo_free(&a->light.photo);
    if (a->light.open) light_session_close(&a->light.session);
    memset(&a->light, 0, sizeof a->light);
}

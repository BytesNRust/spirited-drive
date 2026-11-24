/* The Track light window's side of the App (app.h, light_ui.h): the mode's session, the photo and its auto-refresh. */
#include "app.h"
#include "original_track_trace.h"

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

/* the folder of mask PNGs the photos of an original track show: <its folder>/art-generated/track_masks; "" for a plain light folder */
static void set_masks(App *a)
{
    char masks[APP_PATH + 32] = "";
    if (a->original_dir[0] != '\0') snprintf(masks, sizeof masks, "%s/art-generated/track_masks", a->original_dir);
    snprintf(a->light.photo.masks, sizeof a->light.photo.masks, "%s", masks);
    snprintf(a->light.compare.masks, sizeof a->light.compare.masks, "%s", masks);
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

/* opens the track's light file of the light folder into `ui`'s session (one file holds the four times; `time` is the one the window shows), the old one closed only when this
 * opens; the node count is the game's TRI's, so `cd` must be known; 1, or 0 with `why` */
static int open_session(LightUi *ui, const char *dir, const char *cd, int track, int time, char *why, size_t cap)
{
    LightTrackSession next;
    unsigned nodes = 0;
    if (light_ui_track(track) == NULL) return snprintf(why, cap, "no such track"), 0;
    if (!light_track_session_check_folder(dir, why, cap)) return 0;   /* the folder's own reason first */
    if (cd == NULL || cd[0] == '\0') return snprintf(why, cap, "the game's files (the CD data) are not known: the track's node count is read from them (start the editor with --cd DIR)"), 0;
    if (!original_track_nodes(cd, light_ui_track(track), &nodes, why, cap)) return 0;
    if (!light_track_session_open(&next, dir, light_ui_track(track), nodes, why, cap)) return 0;
    if (ui->open) light_track_session_close(&ui->session);
    ui->session = next;
    ui->open = 1;
    ui->track = track;
    ui->time = time;
    ui->view = light_photo_view_pref();
    ui->weather = light_drive_weather_pref();
    ui->hud = light_photo_hud_pref();
    return 1;
}

int app_light_in_bundle(const char *path)
{
    if (path == NULL) return 0;
    for (const char *p = strstr(path, ".app"); p != NULL; p = strstr(p + 1, ".app"))
        if (p[4] == '/' || p[4] == '\0') return 1;   /* a folder named *.app, anywhere on the way */
    return 0;
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
        if (stat(probe, &st) == 0 && S_ISDIR(st.st_mode) && !app_light_in_bundle(dir)) {   /* never a default inside an app bundle: Save would break its signature */
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
    if (!open_session(ui, dir, a->export_src.cd, track, time, why, sizeof why)) {
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

static const char *const NO_FOLDER = "Choose a light folder: none is set (pick a folder that holds track_light/ and is not inside an app)";

int app_open_light_on(App *a, int track)
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
    } else if (!open_session(&next->light, a->light_dir, a->export_src.cd, track, 1, why, sizeof why)) {
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
    light_compare_init(&a->light.compare);   /* after the move: its jobs start only now */
    snprintf(a->status, sizeof a->status, "Track light: %s", a->light_dir[0] != '\0' ? a->light_dir : "no folder set");
    changed(a);
    return 1;
}

int app_open_light(App *a)
{
    return app_open_light_on(a, 0);
}

int app_light_select(App *a, int track, int time)
{
    LightUi *ui = &a->light;
    ui->error[0] = '\0';
    if (!ui->open || light_ui_track(track) == NULL || light_ui_time_letter(time) == '\0') {
        set_error(a, "no such track or time of day");
        return 0;
    }
    if (track == ui->track) {   /* the same track: the file holds every time, nothing is read again */
        ui->time = time;
        changed(a);
        return 1;
    }
    if (app_modified(a)) {
        set_error(a, "the file has unsaved changes: Save or Revert before another track");
        return 0;
    }
    char why[APP_MSG];
    why[0] = '\0';
    if (!open_session(ui, a->light_dir, a->export_src.cd, track, time, why, sizeof why)) {
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
    const int now = (int)light_track_session_clamp_node(&ui->session, node);
    if (now != ui->node) {
        ui->node = now;
        changed(a);
    }
}

void app_light_set_view(App *a, int view)
{
    LightUi *ui = &a->light;
    const int now = light_photo_view_clamp(view);
    light_photo_view_pref_set(now);
    if (now != ui->view) {
        ui->view = now;
        changed(a);
    }
}

void app_light_set_hud(App *a, int hud)
{
    LightUi *ui = &a->light;
    light_photo_hud_pref_set(hud);
    if ((hud != 0) != ui->hud) {
        ui->hud = hud != 0;
        changed(a);
    }
}

void app_light_touch(App *a)
{
    a->light.file_edits++;
    light_track_session_changed(&a->light.session);
    a->light.error[0] = '\0';
    changed(a);
}

void app_light_revert(App *a)
{
    light_track_session_revert(&a->light.session);
    a->light.error[0] = '\0';
    changed(a);
}

int app_light_save(App *a)
{
    char why[APP_MSG];
    why[0] = '\0';
    char where[LIGHT_TRACK_SESSION_PATH];
    light_track_session_path(&a->light.session, where, sizeof where);
    if (app_light_in_bundle(where) || app_light_in_bundle(a->light_dir)) {
        set_error(a, "not saved: this light file is inside an app bundle (*.app), and writing there breaks the app's signature: choose another light folder");
        return 0;
    }
    if (!light_track_session_save(&a->light.session, why, sizeof why)) {
        set_error(a, why);
        return 0;
    }
    a->light.error[0] = '\0';
    char path[LIGHT_TRACK_SESSION_PATH];
    light_track_session_path(&a->light.session, path, sizeof path);
    snprintf(a->status, sizeof a->status, "saved %s", path);
    return 1;
}

void app_light_set_compare(App *a, int mode)
{
    LightCompare *c = &a->light.compare;
    if (mode < 0 || mode >= LIGHT_COMPARE_MODES || mode == c->mode) return;
    c->mode = mode;
    c->why[0] = '\0';
}

int app_light_busy(const App *a)
{
    return a->light.photo.state == LIGHT_PHOTO_RUNNING;
}

int app_light_drive_running(const App *a)
{
    return a->light.drive.state == LIGHT_DRIVE_RUNNING;
}

int app_light_drive(App *a)
{
    LightUi *ui = &a->light;
    if (!ui->open || !ui->session.has_file) return set_error(a, "there is no light file to drive with: this track's light is made in code"), 0;
    if (a->game[0] == '\0') return set_error(a, "the game's program is not known: the test drive is the game in a window"), 0;
    if (a->export_src.cd[0] == '\0') return set_error(a, "the game's files (the CD data) are not known: the test drive needs them"), 0;
    if (ui->photo.state == LIGHT_PHOTO_RUNNING) light_photo_stop(&ui->photo);   /* one game at a time: the photo is made again after the drive */
    size_t size = 0;
    char *text = light_track_session_text(&ui->session, &size);
    if (text == NULL) return set_error(a, "out of memory"), 0;
    char why[LIGHT_DRIVE_WHY];
    set_masks(a);
    snprintf(ui->drive.masks, sizeof ui->drive.masks, "%s", ui->photo.masks);
    snprintf(ui->drive.window, sizeof ui->drive.window, "%s", a->drive_window);
    const int ok = light_drive_start(&ui->drive, a->game, a->export_src.cd, ui->session.track, light_ui_time_letter(ui->time), (unsigned)ui->node, ui->weather, text, size, why, sizeof why);
    free(text);
    if (!ok) return set_error(a, why), 0;
    ui->drive_serial = ui->edit_serial;
    ui->error[0] = '\0';
    snprintf(a->status, sizeof a->status, "test drive: %s, %s, node %u: the game's window follows every edit (F10 holds the car, F11 copies a brightness line)", ui->session.track,
             light_ui_time_name(ui->time), (unsigned)ui->node);
    return 1;
}

void app_light_drive_stop(App *a)
{
    light_drive_stop(&a->light.drive);
    a->light.drive.why[0] = '\0';   /* a stop the person asked for is not a failure to report */
    a->light.photo_serial = a->light.edit_serial - 1;   /* the photo is made again now that the game is free */
}

void app_light_set_weather(App *a, int weather)
{
    a->light.weather = light_drive_weather_clamp(weather);
    light_drive_weather_pref_set(a->light.weather);
    changed(a);   /* a running drive restarts with it, once it settles */
}

/* the running drive follows the panel: another track, time, node or weather restarts it once the change has settled; an edit of the file is written into its copy at once */
static void drive_follow(App *a, double now)
{
    LightUi *ui = &a->light;
    const LightDriveJob *d = &ui->drive;
    if (ui->edit_serial == ui->drive_serial) return;
    const int moved = strcmp(d->track, ui->session.track) != 0 || d->letter != light_ui_time_letter(ui->time) || d->node != (unsigned)ui->node || d->weather != ui->weather;
    if (moved && now - ui->edit_at < LIGHT_UI_SETTLE) return;
    if (moved) {
        app_light_drive(a);
        return;
    }
    if (now - ui->drive_wrote < LIGHT_UI_DRIVE_WRITE) return;   // the edit stays due: the next frames after the pause write it
    ui->drive_wrote = now;
    size_t size = 0;
    char *text = light_track_session_text(&ui->session, &size);
    char why[LIGHT_DRIVE_WHY];
    if (text == NULL || !light_drive_update(&ui->drive, text, size, why, sizeof why)) set_error(a, text == NULL ? "out of memory" : why);
    free(text);
    ui->drive_serial = ui->edit_serial;
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
    char *text = light_track_session_text(&ui->session, &size);
    if (text == NULL) {
        set_error(a, "out of memory");
        return 0;
    }
    char why[LIGHT_PHOTO_WHY];
    set_masks(a);   /* an original track: the masks of its folder go in front of the game's */
    const int ok = light_photo_start(&ui->photo, a->game, a->export_src.cd, ui->session.track, light_ui_time_letter(ui->time), (unsigned)ui->node, ui->view, ui->hud, light_photo_view_ticks(ui->view), text, size,
                                     why, sizeof why);
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
    if (ui->drive.state == LIGHT_DRIVE_RUNNING) {
        if (light_drive_poll(&ui->drive)) {
            drive_follow(a, now);
            return 0;   /* a drive running is no picture in the making: a headless run does not wait for it, and no photo starts beside it */
        }
        if (ui->drive.state == LIGHT_DRIVE_FAILED && ui->drive.why[0] != '\0') {
            set_error(a, ui->drive.why);
            ui->drive.why[0] = '\0';   /* said once */
        } else if (ui->drive.state == LIGHT_DRIVE_ENDED) {
            snprintf(a->status, sizeof a->status, "test drive: the game's window was closed");
        }
        ui->photo_serial = ui->edit_serial - 1;   /* the photo is made again now that the game is free */
    }
    if (ui->photo.state == LIGHT_PHOTO_RUNNING && light_photo_poll(&ui->photo)) return 1;
    if (ui->photo.state == LIGHT_PHOTO_FAILED && ui->photo.why[0] != '\0' && strcmp(ui->photo.why, "stopped") != 0) {
        set_error(a, ui->photo.why);
        ui->photo.why[0] = '\0';   /* said once */
    }
    if (ui->session.has_file && ui->edit_serial != ui->photo_serial && now - ui->edit_at >= LIGHT_UI_SETTLE) return app_light_photo(a);
    /* the comparison pictures wait for the window's own photo (one game at a time) and for a change to settle */
    const int waiting = ui->photo.state == LIGHT_PHOTO_RUNNING || ui->edit_serial != ui->photo_serial || now - ui->edit_at < LIGHT_UI_SETTLE;
    set_masks(a);
    return light_compare_poll(&ui->compare, &ui->session, ui->edit_serial, light_ui_time_letter(ui->time), ui->node, ui->view, ui->hud, a->game, a->export_src.cd, waiting);
}

void app_light_free(App *a)
{
    light_drive_free(&a->light.drive);
    light_photo_free(&a->light.photo);
    light_compare_free(&a->light.compare);
    if (a->light.open) light_track_session_close(&a->light.session);
    memset(&a->light, 0, sizeof a->light);
}

// ---- Copy to... ---------------------------------------------------------------------------------------------------------------------------------------------------------------

// the words for what the plan did at each time (the (?) beside the status) and the short note
static void copy_words(App *a, const LightTrackCopyPlan *plan, const LightTrackCopyResult *r, int replace)
{
    LightUi *ui = &a->light;
    char *d = ui->copy_detail;
    size_t used = 0;
    char done[40] = "", same[48] = "", kept[48] = "", replaced[48] = "";
    d[0] = '\0';
    for (int t = 0; t < LT_TIMES; t++) {
        const LightTrackCopyTime *to = &plan->to[t];
        const char *name = light_ui_time_name(t);
        char one[88] = "";
        if (to->action == LT_COPY_ADD) snprintf(one, sizeof one, "%s: added%s", name, to->overlaps > 0 ? " (overlaps a line: the file's order matters)" : "");
        else if (to->action == LT_COPY_SAME) snprintf(one, sizeof one, "%s: already has it", name);
        else if (to->action == LT_COPY_CLASH) snprintf(one, sizeof one, "%s: %s", name, replace ? "replaced" : "kept its own value");
        if (one[0] != '\0') used += (size_t)snprintf(d + used, sizeof ui->copy_detail - used, "%s%s", used > 0 ? "\n" : "", one);
        if (used >= sizeof ui->copy_detail) used = sizeof ui->copy_detail - 1;
        if (to->action == LT_COPY_ADD) snprintf(done + strlen(done), sizeof done - strlen(done), "%s%s", done[0] ? ", " : "", name);
        else if (to->action == LT_COPY_SAME) snprintf(same + strlen(same), sizeof same - strlen(same), "%s%s", same[0] ? ", " : "", name);
        else if (to->action == LT_COPY_CLASH && replace) snprintf(replaced + strlen(replaced), sizeof replaced - strlen(replaced), "%s%s", replaced[0] ? ", " : "", name);
        else if (to->action == LT_COPY_CLASH) snprintf(kept + strlen(kept), sizeof kept - strlen(kept), "%s%s", kept[0] ? ", " : "", name);
    }
    char *n = ui->copy_note;
    n[0] = '\0';
    if (done[0]) snprintf(n + strlen(n), sizeof ui->copy_note - strlen(n), "Copied to %s", done);
    if (replaced[0]) snprintf(n + strlen(n), sizeof ui->copy_note - strlen(n), "%sReplaced at %s", n[0] ? " \xc2\xb7 " : "", replaced);
    if (same[0]) snprintf(n + strlen(n), sizeof ui->copy_note - strlen(n), "%s%s already has it", n[0] ? " \xc2\xb7 " : "", same);
    if (kept[0]) snprintf(n + strlen(n), sizeof ui->copy_note - strlen(n), "%s%s kept its own", n[0] ? " \xc2\xb7 " : "", kept);
    if (n[0] == '\0') snprintf(n, sizeof ui->copy_note, "Nothing to copy");
    (void)r;
}

static int copy_do(App *a, const LightTrackCopyPlan *plan, int replace)
{
    LightUi *ui = &a->light;
    LightTrack *f = &ui->session.file;
    char why[LT_WHY];
    LightTrackCopyResult r;
    unsigned key[3] = { 0, 0, 0 };
    unsigned times = 0;
    for (int t = 0; t < LT_TIMES; t++)
        if (plan->to[t].action != LT_COPY_NONE) times |= 1u << t;
    light_track_copy_key(f, plan->kind, plan->from_time, plan->index, key);
    if (!light_track_copy_apply(f, plan, replace, &r, why, sizeof why)) return set_error(a, why), 0;
    ui->error[0] = '\0';
    copy_words(a, plan, &r, replace);
    if (r.added + r.replaced > 0) app_light_touch(a);
    ui->copy_mark_kind = plan->kind, ui->copy_mark_times = times, memcpy(ui->copy_mark_key, key, sizeof key), ui->copy_mark_edits = ui->file_edits;
    return 1;
}

int app_light_copy(App *a, int kind, int index, unsigned times, int mode)
{
    LightUi *ui = &a->light;
    LightTrackCopyPlan plan;
    char why[LT_WHY];
    if (a->kind != APP_KIND_LIGHT || !ui->session.has_file) return set_error(a, "there is no light file open"), 0;
    if (!light_track_copy_plan(&ui->session.file, kind, ui->time, index, times, &plan, why, sizeof why)) return set_error(a, why), 0;
    int clash = 0;
    for (int t = 0; t < LT_TIMES; t++) clash += plan.to[t].action == LT_COPY_CLASH;
    if (clash > 0 && mode == 0) return ui->copy_plan = plan, ui->copy_pending = 1, 1;
    return copy_do(a, &plan, mode == 1);
}

int app_light_copy_answer(App *a, int answer)
{
    LightUi *ui = &a->light;
    if (!ui->copy_pending) return 0;
    ui->copy_pending = 0;
    if (answer == 3) return 1;
    return copy_do(a, &ui->copy_plan, answer == 1);
}

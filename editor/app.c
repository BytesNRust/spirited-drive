#include "app.h"

#include "build.h"
#include "game_find.h"
#include "car_export_kit.h"
#include "formats/car_piece.h"

#include <sys/stat.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_status(App *a, const char *text)
{
    snprintf(a->status, sizeof a->status, "%s", text);
}

/* the last component of `dir` without trailing slashes: the id a new project takes */
static void folder_name(const char *dir, char *out, size_t cap)
{
    size_t n = strlen(dir);
    while (n > 1 && dir[n - 1] == '/') --n;
    size_t start = n;
    while (start > 0 && dir[start - 1] != '/') --start;
    const size_t len = n - start < cap - 1 ? n - start : cap - 1;
    memcpy(out, dir + start, len);
    out[len] = '\0';
}

static void drop_road(App *a)
{
    free(a->tmask_rgba);
    free(a->nodes);
    free(a->flags);
    free(a->node_class);
    a->node_class = NULL;
    a->nodes = NULL;
    a->flags = NULL;
    a->node_count = 0;
    a->road_made = 0;
    free(a->node_dist);
    free(a->point_dist);
    free(a->point_turn);
    free(a->point_width);
    free(a->point_bank);
    a->node_dist = a->point_dist = a->point_turn = a->point_width = a->point_bank = NULL;
    free(a->point_sides);
    a->point_sides = NULL;
    a->point_count = 0;
    a->node_for = -2;   // what the Node window shows is taken again from the selected point once the road is made again
    free(a->problems);
    a->problems = NULL;
    a->problem_count = 0;
    a->summary_count = 0;
    a->stage_short = 0;
    a->stage_text[0] = '\0';
}

void app_begin(App *a, const char *game, const char *user, const char *generic_art)
{
    memset(a, 0, sizeof *a);
    snprintf(a->game, sizeof a->game, "%s", game);
    snprintf(a->user, sizeof a->user, "%s", user);
    snprintf(a->generic, sizeof a->generic, "%s", generic_art != NULL ? generic_art : "");
    a->selected = -1;
    sel_init(&a->sel);
    a->node = a->node_for = -1;
    a->race_look = car_race_look_default();
    install_job_init(&a->install);
    psd_job_init(&a->psd);
    py_probe_init(&a->py, getenv("TNFS_PYTHON"));
    a->export_pick = -1;
    a->preview_paint = -1;
    a->photo_view_open = 0;   /* the large photo belongs to the car that took it */
    a->night = night_settings_default();
    a->race.run.fd = -1;   /* no output pipe: a zero is a descriptor (stdin) */
    install_job_init(&a->race.install);
    a->light.photo.run.fd = -1;
}

int app_open(App *a, const char *dir, const char *game, const char *user, const char *generic_art)
{
    app_begin(a, game, user, generic_art);
    snprintf(a->dir, sizeof a->dir, "%s", dir);
    char id[MOD_INI_ID_MAX + 1], why[APP_MSG];
    folder_name(dir, id, sizeof id);
    if (!project_create(dir, id, generic_art, why, sizeof why) || !project_open(dir, &a->project, &a->trace, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    if (project_art_refresh(dir, generic_art, PROJECT_OLD_ARTS, a->art_note, sizeof a->art_note)) fprintf(stderr, "%s (%s)\n", a->art_note, dir);
    a->art_generic = project_art_is_generic(dir, generic_art);
    a->kind = APP_KIND_TRACK;
    snprintf(a->saved_edge, sizeof a->saved_edge, "%s", a->project.edge);
    if (!track_undo_open(&a->track_undo, &a->project, &a->trace)) {
        set_status(a, "out of memory for undo");
        return 0;
    }
    a->dirty = 1;
    app_recompute(a);
    view_fit(&a->view, a->trace.points, a->trace.count, 960, 600);
    set_status(a, "opened: arrows and keys are listed at the bottom");
    return 1;
}

/* the car at `dir` as the project of `a`, which has nothing open (app_begin) */
static void build_unbuilt_kit(App *a);
static int car_zip_path(const App *a, char *out, size_t cap);

static int car_into(App *a, const char *dir)
{
    char why[APP_MSG];
    if (!car_project_detect(dir, &a->car, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    char card[CAR_PROJECT_PATH + 16];
    snprintf(card, sizeof card, "%s/mod.ini", a->car.root);
    if (!a->car.masks_only && !car_edit_open(&a->edit, card, why, sizeof why)) {   /* a masks project has no card */
        set_status(a, why);
        return 0;
    }
    snprintf(a->dir, sizeof a->dir, "%s", a->car.root);
    a->kind = APP_KIND_CAR;
    a->race_track = car_race_default_track(car_race_original_of(&a->car));
    a->mask.opacity = 0;
    a->preview_paint = -1;
    a->photo_view_open = 0;   /* the large photo belongs to the car that took it */
    a->night = night_settings_default();
    app_mask_scan(a);
    app_card_arm(a);
    set_status(a, a->car.masks_only ? "opened: a masks project (made from the game's files: for your own use, do not share)" : "opened: edit the card and the colors; Save writes mod.ini only");
    return 1;
}

int app_open_any(App *a, const char *dir, const char *game, const char *user, const char *generic_art)
{
    if (!recent_is_project(dir) && car_project_present(dir)) {
        app_begin(a, game, user, generic_art);
        const int opened = car_into(a, dir);
        if (opened) build_unbuilt_kit(a);
        return opened;
    }
    return app_open(a, dir, game, user, generic_art);
}

int app_race_busy(const App *a)
{
    return car_race_busy(&a->race);
}

int app_race(App *a, int photo)
{
    char why[APP_MSG];
    if (a->kind != APP_KIND_CAR) return set_status(a, "open a car first"), 0;
    const RaceTrack *track = car_race_track(a->race_track);
    const ExportCar *car = car_race_original_of(&a->car);
    if (track == NULL || car == NULL) return set_status(a, "Test drive is for the game's own cars: export one with Export original car"), 0;
    RaceLook look = a->race_look;   /* Preview's colour, when the catalogue has it: the race shows the car painted as the Preview does */
    const int p = a->preview_paint;
    look.paint_model = car->model, look.paint = p >= 0 && p < a->edit.now.paint_count ? car_race_paint_digit(car, a->edit.now.paints[p].rgb) : 0;
    const char *masks_of = app_car_is_kit(a) ? a->car.root : a->car.piece;   /* a kit: its art-generated/ masks, as they are now (car_race stages them); else the piece's */
    const int ok = photo ? car_race_start_photo(&a->race, a->game, a->export_src.cd, masks_of, car, track->stem, a->race_night, &look, why, sizeof why)
                         : car_race_start_race(&a->race, a->game, a->export_src.cd, masks_of, car, track->stem, a->race_night, &look, why, sizeof why);
    if (!ok) return set_status(a, why), 0;
    if (photo) sheet_free(&a->sheet), a->sheet_serial++;   // a single photo replaces the sheet in the Race window
    set_status(a, photo ? "taking the in-game photo..." : "the game is running: close its window to end the test");
    return 1;
}

int app_race_poll(App *a)
{
    const int was = car_race_busy(&a->race);
    const int running = car_race_poll(&a->race);
    if (was && !running && a->race.summary[0] != '\0') set_status(a, a->race.summary);
    return running;
}

void app_race_stop(App *a)
{
    car_race_stop(&a->race);
}

void app_set_export_source(App *a, const ExportSource *src)
{
    a->export_src = *src;
}

int app_export_car(App *a, const char *parent)
{
    char name[96], dir[APP_PATH], why[APP_MSG];
    const ExportCar *car = car_export_car(a->export_pick);
    car_export_kit_folder(a->export_pick, name, sizeof name);
    const size_t n = strlen(parent);
    if (car == NULL || snprintf(dir, sizeof dir, "%s%s%s", parent, n > 0 && parent[n - 1] == '/' ? "" : "/", name) >= (int)sizeof dir) {
        set_status(a, "no car was chosen to export");
        snprintf(a->open_error, sizeof a->open_error, "%s", a->status);
        return 0;
    }
    if (!car_export_kit(&a->export_src, a->export_pick, dir, why, sizeof why)) {   /* the kit in the current layout: the migration of the originals makes the same */
        snprintf(a->open_error, sizeof a->open_error, "Could not export %s: %s", car->label, why);
        set_status(a, a->open_error);
        return 0;
    }
    a->export_dialog = 0;
    if (!app_open_car(a, dir)) return 0;   /* it says why in open_error, and the files stay where they were made */
    snprintf(a->status, sizeof a->status, "exported %s into %s: made from the game's files, for your own use, do not share", car->label, dir);
    return 1;
}

void app_free(App *a)
{
    app_light_free(a);
    car_race_free(&a->race);
    sheet_free(&a->sheet);
    install_job_free(&a->install);
    psd_job_free(&a->psd);
    py_probe_free(&a->py);
    car_build_job_free(&a->car_build);
    car_edit_free(&a->edit);
    mask_edit_free(&a->mask.me);
    track_undo_free(&a->track_undo);
    sel_free(&a->sel);
    free(a->rotate_from);
    a->rotate_from = NULL, a->rotate_from_count = 0;
    free(a->clip_points), free(a->clip_keys);
    a->clip_points = NULL, a->clip_keys = NULL, a->clip_count = a->clip_key_count = 0;
    free(a->trace.points);
    free(a->trace.keys);
    drop_road(a);
    a->trace.points = NULL;
    a->trace.keys = NULL;
    a->trace.count = a->trace.key_count = a->trace.key_cap = 0;
}

/* the node nearest control point `i`: by its distance along the curve over the road's (equal, to a few centimetres) node spacing */
int app_nearest_node(const App *a, int i)
{
    if (a->node_count < 2 || a->point_dist == NULL || i < 0 || i >= a->point_count) return 0;
    const double step = a->node_dist != NULL ? a->node_dist[a->node_count - 1] / (a->node_count - 1) : 0;
    const int node = step > 0 ? (int)lround(a->point_dist[i] / step) : 0;
    return node < 0 ? 0 : node >= a->node_count ? a->node_count - 1 : node;
}

void app_recompute(App *a)
{
    if (!a->dirty) return;
    a->dirty = 0;
    drop_road(a);
    a->length = track_trace_length(a->trace.points, a->trace.count);
    ExchangeError err;
    memset(&err, 0, sizeof err);
    a->road_text[0] = a->build_text[0] = '\0';
    if (!track_trace_nodes_keyed(a->trace.points, a->trace.count, a->trace.width, a->trace.keys, a->trace.key_count, &a->nodes, &a->node_count, &err)) {
        snprintf(a->road_text, sizeof a->road_text, "%s", err.message);
        return;
    }
    a->flags = calloc((size_t)a->node_count, sizeof *a->flags);
    if (a->flags == NULL) {
        drop_road(a);
        snprintf(a->road_text, sizeof a->road_text, "out of memory");
        return;
    }
    a->road_made = 1;
    if (build_road_check(a->nodes, a->node_count, a->build_text, sizeof a->build_text)) a->build_text[0] = '\0';
    a->stage_short = track_trace_stage_short(a->node_count, a->stage_text, sizeof a->stage_text);
    track_trace_flags(a->nodes, a->node_count, a->flags);
    a->node_dist = calloc((size_t)a->node_count, sizeof *a->node_dist);
    a->point_dist = calloc((size_t)a->trace.count, sizeof *a->point_dist);
    a->point_turn = calloc((size_t)a->trace.count, sizeof *a->point_turn);
    a->point_width = calloc((size_t)a->trace.count, sizeof *a->point_width);
    a->point_sides = calloc((size_t)a->trace.count, sizeof *a->point_sides);
    a->point_bank = calloc((size_t)a->trace.count, sizeof *a->point_bank);
    if (a->node_dist != NULL && a->point_dist != NULL && a->point_turn != NULL && a->point_width != NULL && a->point_sides != NULL && a->point_bank != NULL) {
        a->point_count = a->trace.count;
        track_trace_distances(a->nodes, a->node_count, a->node_dist);
        track_trace_point_distances(a->trace.points, a->trace.count, a->point_dist);
        track_trace_point_widths(a->trace.points, a->trace.count, a->trace.width, a->trace.keys, a->trace.key_count, a->point_width);
        track_trace_point_sides(a->trace.points, a->trace.count, a->trace.keys, a->trace.key_count, a->point_sides);
        track_trace_point_banks(a->trace.points, a->trace.count, a->trace.keys, a->trace.key_count, a->point_bank);
        for (int i = 0; i < a->trace.count; ++i) a->point_turn[i] = track_trace_signed_turn(a->nodes, app_nearest_node(a, i));
    } else {   // no memory for the figures: the road is as made, without them
        free(a->node_dist);
        free(a->point_dist);
        free(a->point_turn);
        free(a->point_width);
        free(a->point_bank);
        a->node_dist = a->point_dist = a->point_turn = a->point_width = a->point_bank = NULL;
        free(a->point_sides);
        a->point_sides = NULL;
    }
    a->summary_count = track_trace_summary(a->flags, a->node_count, a->summary);
    a->problem_count = track_trace_problems(a->nodes, a->node_count, a->flags, NULL, 0);
    if (a->problem_count > 0) {
        a->problems = calloc((size_t)a->problem_count, sizeof *a->problems);
        if (a->problems != NULL) track_trace_problems(a->nodes, a->node_count, a->flags, a->problems, a->problem_count);
        else a->problem_count = 0;
    }
}

const char *app_road_refusal(const App *a)
{
    if (a->road_text[0] != '\0') return a->road_text;
    for (int i = 0; i < a->problem_count; ++i)
        if (a->problems[i].red) return NULL;   /* Build stops at a red node first: Problems' row names it */
    return a->build_text[0] != '\0' ? a->build_text : NULL;
}

void app_sync_selection(App *a)
{
    if (a->selected != a->node_for) {
        a->node_for = a->selected;
        a->node = a->selected >= 0 && a->road_made && a->point_dist != NULL && a->selected < a->point_count ? app_nearest_node(a, a->selected) : -1;
    }
    if (a->node >= a->node_count || !a->road_made) a->node = a->road_made && a->node_count > 0 ? a->node_count - 1 : -1;
}

void app_select_node(App *a, int node)
{
    if (!a->road_made || node < 0 || node >= a->node_count) return;
    const double nx = a->nodes[node].position[0] / 65536.0, nz = a->nodes[node].position[2] / 65536.0;
    int best = 0;
    double best_d = HUGE_VAL;
    for (int i = 0; i < a->trace.count; ++i) {
        const double d = hypot(a->trace.points[i].x - nx, a->trace.points[i].z - nz);
        if (d < best_d) best_d = d, best = i;
    }
    a->selected = a->node_for = best;
    a->node = node;
}

void app_title(const App *a, char *out, size_t cap)
{
    const char *name = a->kind == APP_KIND_TRACK ? a->project.id : a->kind == APP_KIND_CAR ? (a->car.info.id[0] != '\0' ? a->car.info.id : a->car.folder) : a->kind == APP_KIND_LIGHT ? "Track light" : "";
    if (name[0] == '\0') snprintf(out, cap, "Spirited Drive Studio");
    else snprintf(out, cap, "Spirited Drive Studio \xe2\x80\x94 %s%s", name, app_modified(a) ? " *" : "");
}

void app_track_commit(App *a)
{
    if (a->kind == APP_KIND_TRACK) track_undo_commit(&a->track_undo, &a->project, &a->trace);
}

int app_track_undo(App *a, int redo)
{
    if (a->kind != APP_KIND_TRACK) return 0;
    if (!(redo ? track_undo_redo : track_undo_undo)(&a->track_undo, &a->project, &a->trace)) return 0;
    if (a->selected >= a->trace.count) a->selected = a->trace.count - 1;
    a->dirty = 1;
    app_recompute(a);
    set_status(a, redo ? "redone" : "undone");
    return 1;
}

int app_modified(const App *a)
{
    if (a->kind == APP_KIND_CAR) return car_edit_modified(&a->edit);
    if (a->kind == APP_KIND_LIGHT) return light_session_dirty(&a->light.session);
    return a->kind == APP_KIND_TRACK && track_undo_modified(&a->track_undo, &a->project, &a->trace);
}

void app_ask(App *a, AppAction act, const char *path)
{
    if (act == APP_ACT_NONE || a->confirm != APP_ACT_NONE || a->go != APP_ACT_NONE) return;
    if (install_job_busy(&a->install)) {   /* the game's installer is writing the player folder: nothing else until it ends */
        set_status(a, "An install is running: wait for it to end");
        return;
    }
    if (car_race_busy(&a->race)) {   /* the game is running on this project's masks: nothing else until it ends */
        set_status(a, "A race is running: wait for it to end, or press Stop");
        return;
    }
    const int gives = act == APP_ACT_NEW_AT || act == APP_ACT_OPEN_AT || act == APP_ACT_OPEN_CAR_AT || act == APP_ACT_EXPORT_AT || act == APP_ACT_LIGHT_AT;
    const int answered = gives && a->discard_accepted;   /* the question was answered before the picker: Save or Discard stands for the open it gave */
    if (gives) a->discard_accepted = 0;
    if ((app_modified(a) || act == APP_ACT_QUIT) && !answered) {   /* leaving Studio is asked every time, the rest only with something unsaved */
        a->confirm = act;
        snprintf(a->confirm_path, sizeof a->confirm_path, "%s", path != NULL ? path : "");
    } else {
        a->go = act;
        snprintf(a->go_path, sizeof a->go_path, "%s", path != NULL ? path : "");
    }
}

void app_answer(App *a, int answer)
{
    if (a->confirm == APP_ACT_NONE) return;
    if (answer == APP_ANSWER_SAVE && !app_save(a)) return;   // it stays waiting: the status says why
    if (answer == APP_ANSWER_SAVE || answer == APP_ANSWER_DISCARD) {
        a->go = a->confirm;
        memcpy(a->go_path, a->confirm_path, sizeof a->go_path);
        a->discard_accepted = a->confirm == APP_ACT_NEW || a->confirm == APP_ACT_OPEN || a->confirm == APP_ACT_OPEN_CAR || a->confirm == APP_ACT_EXPORT_PICK
                              || a->confirm == APP_ACT_LIGHT_PICK;   /* a folder picker follows: the open it gives is not asked about again */
    }
    a->confirm = APP_ACT_NONE;
}

void app_pick_cancelled(App *a)
{
    a->discard_accepted = 0;
}

/* puts the open project first in the recent list, by its real path (a relative one or one with a slash at its end would be another entry, and
 * greyed from another folder), and keeps the list */
static void remember(App *a)
{
    if (a->kind == APP_KIND_NONE) return;
    const int kind = a->kind == APP_KIND_CAR ? RECENT_CAR : RECENT_TRACK;
    char real[PATH_MAX];
    if (realpath(a->dir, real) != NULL) recent_push(&a->recent, real, kind);
    else recent_push(&a->recent, a->dir, kind);
    if (a->recent_file[0] != '\0') recent_save(&a->recent, a->recent_file);
}

void app_carry_window(App *to, const App *from)
{
    to->recent = from->recent;
    memcpy(to->recent_file, from->recent_file, sizeof to->recent_file);
    memcpy(to->workspace, from->workspace, sizeof to->workspace);
    to->workspace_put_aside = from->workspace_put_aside;
    memcpy(to->workspace_file, from->workspace_file, sizeof to->workspace_file);
    to->workspace_list = from->workspace_list;
    memcpy(to->game_tried, from->game_tried, sizeof to->game_tried);   /* what the game lookup found stays with the window, not with a project */
    memcpy(to->game_memo, from->game_memo, sizeof to->game_memo);
    to->export_src = from->export_src;   /* so does where the game's files are */
    to->export_pick = from->export_pick;
    to->drive_pick = from->drive_pick;   /* the editor's setting, not the project's */
    memcpy(to->light_dir, from->original_dir[0] != '\0' ? from->data_dir : from->light_dir, sizeof to->light_dir);   /* an original track's own light folder stays with it */
    memcpy(to->data_dir, from->data_dir, sizeof to->data_dir);
    memcpy(to->repo_arg, from->repo_arg, sizeof to->repo_arg);
    memcpy(to->repo_cfg, from->repo_cfg, sizeof to->repo_cfg);
    memcpy(to->art_dir, from->art_dir, sizeof to->art_dir);
    memcpy(to->tools, from->tools, sizeof to->tools);   /* Convert PSD's folders are the window's too */
    memcpy(to->psd_root, from->psd_root, sizeof to->psd_root);
}

/* a kit with no car file yet is built, in a thread, as it opens: the car file is the build's (a masks project has its own) */
static void build_unbuilt_kit(App *a)
{
    char why[APP_MSG];
    if (a->kind != APP_KIND_CAR || a->car.masks_only || a->car.built) return;
    if (!car_build_job_start(&a->car_build, a->car.root, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "the car file was not built: %s", why);
        return;
    }
    snprintf(a->status, sizeof a->status, "building the car from the kit...");
}

int app_car_building(const App *a)
{
    return car_build_job_busy(&a->car_build);
}

int app_car_build_poll(App *a)
{
    if (!car_build_job_busy(&a->car_build)) return 0;
    if (car_build_job_poll(&a->car_build)) return 1;
    if (a->car_build.check) {   // the kit's check ended: the findings are the panel's, the status is the totals
        const size_t errors = car_check_count(&a->car_build.list, CAR_CHECK_ERROR), warnings = car_check_count(&a->car_build.list, CAR_CHECK_WARNING);
        if (!a->car_build.ok) snprintf(a->status, sizeof a->status, "the check could not run: %s", a->car_build.why);
        else snprintf(a->status, sizeof a->status, "checked: %zu error%s, %zu warning%s", errors, errors == 1 ? "" : "s", warnings, warnings == 1 ? "" : "s");
        return 0;
    }
    if (a->car_build.ok) {
        snprintf(a->status, sizeof a->status, "built the car: build/piece and the zip are made again");
        if (!car_zip_path(a, a->zip, sizeof a->zip)) a->zip[0] = '\0';
    }
    else snprintf(a->status, sizeof a->status, "the car could not be built: %s", a->car_build.why);
    app_car_reread(a);
    return 0;
}

static int open_in(App *a, const char *dir, AppKind kind)
{
    App *next = calloc(1, sizeof *next);
    if (next == NULL) {
        set_status(a, "out of memory");
        return 0;
    }
    int opened;
    if (kind == APP_KIND_CAR) {
        app_begin(next, a->game, a->user, a->generic[0] != '\0' ? a->generic : NULL);
        opened = car_into(next, dir);
    } else {
        opened = app_open(next, dir, a->game, a->user, a->generic[0] != '\0' ? a->generic : NULL);
    }
    if (!opened) {
        set_status(a, next->status);
        snprintf(a->open_error, sizeof a->open_error, "%s", next->status);
        app_free(next);
        free(next);
        return 0;
    }
    app_carry_window(next, a);
    if (a->py.state == 2) {   /* the python found is the session's (the log below now belongs to the old window's app_free); a probe not done is made again by the new project, with the same TNFS_PYTHON */
        py_probe_free(&next->py);
        next->py = a->py;
        memset(&next->py.run, 0, sizeof next->py.run);
        next->py.run.fd = -1;
    }
    app_free(a);
    *a = *next;   // the new project's arrays belong to `a` now
    free(next);
    a->fit_pending = 1;
    remember(a);
    build_unbuilt_kit(a);
    return 1;
}

AppRoom app_room(const App *a)
{
    return a->kind == APP_KIND_CAR ? APP_ROOM_CARS : a->kind == APP_KIND_NONE ? APP_ROOM_HUB : APP_ROOM_TRACKS;
}

int app_close_project(App *a)
{
    App *next = calloc(1, sizeof *next);
    if (next == NULL) {
        set_status(a, "out of memory");
        return 0;
    }
    app_begin(next, a->game, a->user, a->generic[0] != '\0' ? a->generic : NULL);
    app_carry_window(next, a);
    if (a->py.state == 2) {   // the python found is the session's, as open_in keeps it
        py_probe_free(&next->py);
        next->py = a->py;
        memset(&next->py.run, 0, sizeof next->py.run);
        next->py.run.fd = -1;
    }
    app_free(a);
    *a = *next;
    free(next);
    return 1;
}

int app_open_project(App *a, const char *dir)
{
    if (original_track_present(dir)) return app_open_original(a, dir);
    if (!recent_is_project(dir)) {
        set_status(a, "that folder holds no project (no project.ini): use New to start one there");
        snprintf(a->open_error, sizeof a->open_error, "%s", a->status);
        return 0;
    }
    return open_in(a, dir, APP_KIND_TRACK);
}

int app_open_car(App *a, const char *dir)
{
    return open_in(a, dir, APP_KIND_CAR);
}

int app_open_recent(App *a, const char *dir)
{
    for (int i = 0; i < a->recent.count; ++i)
        if (strcmp(a->recent.dir[i], dir) == 0 && a->recent.kind[i] == RECENT_CAR) return app_open_car(a, dir);
    return app_open_project(a, dir);
}

int app_new_project(App *a, const char *dir)
{
    if (recent_is_project(dir)) {
        set_status(a, "that folder already holds a project: use Open");
        snprintf(a->open_error, sizeof a->open_error, "%s", a->status);
        return 0;
    }
    return open_in(a, dir, APP_KIND_TRACK);
}

void app_keep_recent(App *a, const char *file)
{
    snprintf(a->recent_file, sizeof a->recent_file, "%s", file != NULL ? file : "");
    recent_load(&a->recent, a->recent_file);
    remember(a);
}

int app_node_count(const App *a)
{
    return a->road_made ? a->node_count : 4 * track_trace_segments(a->length) + 1;
}

/* 1 when a track project is open: the track's actions say so when it is not */
static int track_open(App *a)
{
    if (a->kind == APP_KIND_TRACK) return 1;
    set_status(a, a->kind == APP_KIND_CAR ? "Build and Install are for a car kit: this project is a masks project or a car as exported, which has no build" : "open a track first");
    return 0;
}

int app_save(App *a)
{
    char why[APP_MSG];
    if (a->kind == APP_KIND_LIGHT) return app_light_save(a);
    if (a->kind == APP_KIND_CAR && a->car.masks_only) return set_status(a, "a masks project has no card to save: paint the masks in your picture editor, on the PNG files in car_masks/"), 0;
    if (a->kind == APP_KIND_CAR) {   /* the card: only mod.ini, only the lines that changed */
        const int changed = car_edit_modified(&a->edit);
        if (changed && !app_card_save_ok(a)) return 0;
        if (changed && app_photos_missing(a, why, sizeof why)) return set_status(a, why), 0;
        if (!car_edit_save(&a->edit, why, sizeof why)) {
            set_status(a, why);
            return 0;
        }
        if (changed) app_card_keep_mine(a);   /* our own write is not a change on disk */
        app_photos_swept(a);
        set_status(a, changed ? "saved mod.ini" : "nothing to save: mod.ini is as it was");
        return 1;
    }
    if (!track_open(a)) return 0;
    if (!project_save_trace(a->dir, &a->trace, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    if (strcmp(a->project.edge, a->saved_edge) != 0) {
        if (!project_set_edge(a->dir, a->project.edge, why, sizeof why)) {
            set_status(a, why);
            return 0;
        }
        snprintf(a->saved_edge, sizeof a->saved_edge, "%s", a->project.edge);
    }
    track_undo_saved(&a->track_undo, &a->project, &a->trace);
    set_status(a, "saved trace.json and project.ini");
    return 1;
}

/* a car kit in the current layout is open (not a masks project, not an exported original's piece folder): the one shape Build and Install are for */
static int car_kit_open(const App *a)
{
    return a->kind == APP_KIND_CAR && !a->car.masks_only && !a->car.legacy;
}

int app_car_is_kit(const App *a)
{
    return car_kit_open(a);
}

/* the package car build writes: build/<id>.zip (the stem when the card has no id: as studio's write_build names it) */
static int car_zip_path(const App *a, char *out, size_t cap)
{
    const char *name = a->car.info.id[0] != '\0' ? a->car.info.id : a->car.info.stem;
    return snprintf(out, cap, "%s/build/%s.zip", a->car.root, name) < (int)cap;
}

/* Build on a kit: car build's work, in a thread; app_car_build_poll tells its end. The sources are read from disk, so what is not saved is saved first */
static int build_car(App *a)
{
    char why[APP_MSG];
    if (car_piece_stem_is_original(a->car.info.stem)) return set_status(a, "build: an original car is the game's own: it has no package to build (its car file is made for the Preview when it opens); Install does not take it either"), 0;
    if (app_install_busy(a) || car_race_installing(&a->race)) return set_status(a, "build: the game is installing the package: wait for it to end"), 0;   /* it reads the zip */
    if (car_edit_modified(&a->edit)) return set_status(a, "build: mod.ini has changes that are not saved: save them first"), 0;
    if (!car_build_job_start(&a->car_build, a->car.root, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "build: %s", why);
        return 0;
    }
    set_status(a, "building the car from the kit...");
    return 1;
}

/* Check on a kit: the report of studio/car_kit_check.h, in the build's thread (one at a time with a build); app_car_build_poll tells its end */
int app_check(App *a)
{
    char why[APP_MSG];
    if (!car_kit_open(a)) return set_status(a, "check: this is not a car kit"), 0;
    if (!car_build_job_start_check(&a->car_build, a->car.root, why, sizeof why)) return snprintf(a->status, sizeof a->status, "check: %s", why), 0;
    set_status(a, "checking the kit...");
    return 1;
}

int app_build(App *a)
{
    char why[APP_MSG], zip[APP_PATH];
    if (car_kit_open(a)) return build_car(a);
    if (!track_open(a)) return 0;
    if (app_install_busy(a) || car_race_installing(&a->race)) return set_status(a, "build: the game is installing the package: wait for it to end"), 0;   /* it reads the zip */
    if (!build_project(a->dir, &a->project, &a->trace, zip, sizeof zip, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "build refused: %s", why);
        return 0;
    }
    snprintf(a->zip, sizeof a->zip, "%s", zip);
    snprintf(a->status, sizeof a->status, "built %s", strrchr(zip, '/') != NULL ? strrchr(zip, '/') + 1 : zip);
    return 1;
}

static int game_missing_status(App *a, const char *what)
{
    snprintf(a->status, sizeof a->status, "%sthe game was not found: copy Spirited Drive and Spirited Drive Studio to Applications, pick it with Locate game, or start the editor with --game PATH (looked at %s)",
             what, a->game_tried[0] != '\0' ? a->game_tried : a->game);
    return 0;
}

/* the game's program can be opened (a file the player's system lets this one read; whether it runs is the exec's to say) */
static int game_there(const App *a)
{
#if defined(_WIN32)
    (void)a;   /* the editor does not run the game there: install_zip prints the command to run by hand, whatever --game says */
    return 1;
#else
    FILE *f = fopen(a->game, "rb");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
#endif
}

int app_game_found(const App *a)
{
    return game_there(a);
}

void app_set_game_lookup(App *a, const char *tried, const char *memo)
{
    snprintf(a->game_tried, sizeof a->game_tried, "%s", tried != NULL ? tried : "");
    snprintf(a->game_memo, sizeof a->game_memo, "%s", memo != NULL ? memo : "");
}

int app_set_game(App *a, const char *picked)
{
    char program[APP_PATH];
    if (!game_find_program_of(picked, program, sizeof program)) {
        snprintf(a->status, sizeof a->status, "locate game: %s is not Spirited Drive (pick its .app)", picked != NULL ? picked : "nothing");
        return 0;
    }
    FILE *f = fopen(program, "rb");
    if (f == NULL) {
        snprintf(a->status, sizeof a->status, "locate game: no game program at %s", program);
        return 0;
    }
    fclose(f);
    snprintf(a->game, sizeof a->game, "%s", program);
    if (a->game_memo[0] != '\0') {
        FILE *m = fopen(a->game_memo, "wb");
        if (m != NULL) fprintf(m, "%s\n", program), fclose(m);
    }
    snprintf(a->status, sizeof a->status, "the game is %s", program);
    return 1;
}

int app_check_game(App *a)
{
    return game_there(a) || game_missing_status(a, "");
}

int app_install(App *a)
{
    struct stat st;
    if (!car_kit_open(a) && !track_open(a)) return 0;
    if (!game_there(a)) return game_missing_status(a, "install: ");
    if (car_kit_open(a) && car_piece_stem_is_original(a->car.info.stem)) return set_status(a, "install: an original car is the game's own: it is not installed (look at it, retouch its masks, take a photo)"), 0;
    if (car_kit_open(a)) {   /* the kit's package is the one its last build made; it is there from an earlier session too */
        if (car_build_job_busy(&a->car_build)) return set_status(a, "install: the car is being built: wait for it to end"), 0;
        if (!car_zip_path(a, a->zip, sizeof a->zip) || stat(a->zip, &st) != 0) {
            a->zip[0] = '\0';
            return set_status(a, "build first: install takes the package the build makes"), 0;
        }
    }
    if (a->zip[0] == '\0') {
        set_status(a, "build first (B): install takes the built package");
        return 0;
    }
    if (install_job_busy(&a->install)) return set_status(a, "install: one is running already"), 0;
    if (car_race_busy(&a->race)) return set_status(a, "install: a test drive is running: wait for it to end, or press Stop"), 0;
    char why[APP_MSG];
    if (!install_job_start(&a->install, a->game, a->zip, a->user, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "install failed: %s", why);
        return 0;
    }
    set_status(a, "installing...");
    return 1;
}

int app_install_poll(App *a)
{
    if (!install_job_busy(&a->install)) return 0;
    if (install_job_poll(&a->install)) return 1;
    if (a->install.ok) {
        snprintf(a->status, sizeof a->status, "installed into %s", a->user[0] != '\0' ? a->user : "the game's player folder");
        char next[MOD_INI_VERSION_MAX + 1];
        if (a->kind == APP_KIND_CAR && install_version_unraised(a->install.run.log)) {   /* the game kept the old version: the person raises it (was Rehacer's offer) */
            const size_t n = strlen(a->status);
            if (car_edit_version_next(a->edit.now.value[CE_VERSION], next, sizeof next))
                snprintf(a->status + n, sizeof a->status - n, "; the game says the files changed but the version did not: raise Version in the Car tab to %s", next);
            else snprintf(a->status + n, sizeof a->status - n, "; the game says the files changed but the version did not: raise Version in the Car tab");
        }
    } else {
        snprintf(a->status, sizeof a->status, "install failed: %s", a->install.why);
    }
    return 0;
}

void app_install_stop(App *a)
{
    install_job_stop(&a->install);
}

int app_install_busy(const App *a)
{
    return install_job_busy(&a->install);
}

/* a copy of the trace's points with `point` put at `index` (0..count: index == count appends) */
static TracePoint *points_with(const App *a, int index, TracePoint point)
{
    TracePoint *more = malloc(sizeof *more * (size_t)(a->trace.count + 1));
    if (more == NULL) return NULL;
    memcpy(more, a->trace.points, sizeof *more * (size_t)index);
    more[index] = point;
    memcpy(more + index + 1, a->trace.points + index, sizeof *more * (size_t)(a->trace.count - index));
    return more;
}

static void put_points(App *a, TracePoint *points, int count, int selected)
{
    free(a->trace.points);
    a->trace.points = points;
    a->trace.count = count;
    a->selected = selected;
    a->dirty = 1;
}

/* the point after the last one, along the last segment */
static TracePoint extension(const App *a)
{
    const TracePoint *last = &a->trace.points[a->trace.count - 1], *prev = &a->trace.points[a->trace.count - 2];
    const double dx = last->x - prev->x, dz = last->z - prev->z, len = hypot(dx, dz);
    TracePoint p = *last;
    if (len > 0) {
        p.x += dx / len * APP_NEW_STEP;
        p.z += dz / len * APP_NEW_STEP;
    } else {
        p.z += APP_NEW_STEP;
    }
    return p;
}

static int can_add(App *a)
{
    if (a->trace.count < PROJECT_POINTS_MAX) return 1;
    set_status(a, "the trace has the most points it can hold");
    return 0;
}

void app_add_point(App *a)
{
    if (a->kind != APP_KIND_TRACK) return;
    if (!can_add(a)) return;
    TracePoint *more = points_with(a, a->trace.count, extension(a));
    if (more == NULL) {
        set_status(a, "out of memory");
        return;
    }
    put_points(a, more, a->trace.count + 1, a->trace.count);
    set_status(a, "point added at the end");
}

void app_insert_point(App *a)
{
    if (a->kind != APP_KIND_TRACK) return;
    if (a->selected < 0) {
        set_status(a, "select a point first (click it)");
        return;
    }
    if (a->selected == a->trace.count - 1) {
        app_add_point(a);
        return;
    }
    if (!can_add(a)) return;
    const TracePoint *p = &a->trace.points[a->selected], *q = &a->trace.points[a->selected + 1];
    const TracePoint mid = { .x = (p->x + q->x) / 2, .z = (p->z + q->z) / 2, .h = track_trace_height_at(a->trace.points, a->trace.count, a->selected, 0.5) };   /* on the curve: no height moves */
    TracePoint *more = points_with(a, a->selected + 1, mid);
    if (more == NULL) {
        set_status(a, "out of memory");
        return;
    }
    track_trace_keys_insert_point(a->trace.keys, a->trace.key_count, a->selected + 1);   // the keys stay on their points
    put_points(a, more, a->trace.count + 1, a->selected + 1);
    set_status(a, "point inserted after the selected one");
}

int app_insert_point_at(App *a, int seg, double frac)
{
    if (a->kind != APP_KIND_TRACK) return 0;
    if (seg < 0 || seg + 1 >= a->trace.count || !(frac >= APP_INSERT_MARGIN && frac <= 1 - APP_INSERT_MARGIN)) {
        set_status(a, "click a place on the profile between two points first");
        return 0;
    }
    if (!can_add(a)) return 0;
    const TracePoint *p = &a->trace.points[seg], *q = &a->trace.points[seg + 1];
    const TracePoint at = { .x = p->x + frac * (q->x - p->x), .z = p->z + frac * (q->z - p->z), .h = track_trace_height_at(a->trace.points, a->trace.count, seg, frac) };   /* on the curve: no height moves */
    TracePoint *more = points_with(a, seg + 1, at);
    if (more == NULL) {
        set_status(a, "out of memory");
        return 0;
    }
    track_trace_keys_insert_point(a->trace.keys, a->trace.key_count, seg + 1);   // the keys stay on their points
    put_points(a, more, a->trace.count + 1, seg + 1);
    set_status(a, "point inserted at the clicked place");
    return 1;
}

void app_profile_place(App *a, int seg, double frac)
{
    a->profile_place.set = 1;
    a->profile_place.seg = seg;
    a->profile_place.frac = frac;
}

int app_insert_here(App *a)
{
    if (!a->profile_place.set) {
        set_status(a, "click a place on the profile between two points first");
        return 0;
    }
    const int done = app_insert_point_at(a, a->profile_place.seg, a->profile_place.frac);
    if (done) a->profile_place.set = 0;
    return done;
}

void app_set_point_height(App *a, int point, double metres)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || !(fabs(metres) <= 1e9)) return;   /* not a number: nothing */
    double want = round(metres / TRACE_HEIGHT_STEP_M) * TRACE_HEIGHT_STEP_M;
    want = want > TRACE_HEIGHT_MAX_M ? TRACE_HEIGHT_MAX_M : want < -TRACE_HEIGHT_MAX_M ? -TRACE_HEIGHT_MAX_M : want;
    if (a->trace.points[point].h == want) return;   // the same: nothing changed
    a->trace.points[point].h = want;
    a->dirty = 1;
}

/* a width for a key: inside the road's range and a whole byte, a quarter of a metre */
static double key_metres(double metres)
{
    const double m = metres < TRACE_ROAD_MIN_M ? TRACE_ROAD_MIN_M : metres > TRACE_ROAD_MAX_M ? TRACE_ROAD_MAX_M : metres;
    return round(m * TRACE_WIDTH_PER_METRE) / TRACE_WIDTH_PER_METRE;
}

void app_set_point_width(App *a, int point, double metres)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return;
    TraceKey v;
    memset(&v, 0, sizeof v);
    v.has = TRACE_KEY_WIDTH, v.width_m = key_metres(metres);
    app_set_scoped(a, point, &v);
}

void app_clear_point_width(App *a, int point)
{
    if (a->kind != APP_KIND_TRACK) return;
    const int before = a->trace.key_count;
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    const int had = at >= 0 && (a->trace.keys[at].has & TRACE_KEY_WIDTH);
    track_trace_key_clear_width(a->trace.keys, &a->trace.key_count, point);
    if (had || a->trace.key_count != before) a->dirty = 1;
}

void app_set_point_surface(App *a, int point, int side, TraceSurface surface)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return;
    if (surface == TRACE_SURFACE_INHERIT) {   // inherit again: that side's key goes
        const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
        if (at < 0 || !(a->trace.keys[at].has & TRACE_KEY_SHOULDER_SURFACE)) return;   // nothing to inherit again
        const TraceSurface now = side == TRACK_GEN_LEFT ? a->trace.keys[at].shoulder_l_surface : a->trace.keys[at].shoulder_r_surface;
        if (now == TRACE_SURFACE_INHERIT) return;
        track_trace_key_set_surface(&a->trace.keys, &a->trace.key_count, &a->trace.key_cap, point, side, surface);
        a->dirty = 1;
        return;
    }
    if ((side != TRACK_GEN_LEFT && side != TRACK_GEN_RIGHT) || (surface != TRACE_SURFACE_VERGE && surface != TRACE_SURFACE_TARMAC)) return;
    TraceKey v;
    memset(&v, 0, sizeof v);
    v.has = TRACE_KEY_SHOULDER_SURFACE;
    *(side == TRACK_GEN_LEFT ? &v.shoulder_l_surface : &v.shoulder_r_surface) = surface;
    app_set_scoped(a, point, &v);
}

int app_tarmac_art_note(const App *a, char *text, size_t cap)
{
    if (a->kind != APP_KIND_TRACK || a->art_generic) return 0;
    for (int i = 0; i < a->trace.key_count; ++i) {
        const TraceKey *k = &a->trace.keys[i];
        if ((k->has & TRACE_KEY_SHOULDER_SURFACE) && (k->shoulder_l_surface == TRACE_SURFACE_TARMAC || k->shoulder_r_surface == TRACE_SURFACE_TARMAC)) {
            snprintf(text, cap, "an asphalt shoulder (Road profile) draws with bank 4 of the art (%s), which is asphalt only in Studio's own GENERIC.FAM: check it in the game",
                     a->project.art);
            return 1;
        }
    }
    return 0;
}

void app_set_point_bank(App *a, int point, double degrees)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || !isfinite(degrees)) return;
    const double most = TRACE_BANK_INPUT_MAX_DEG;
    TraceKey v;
    memset(&v, 0, sizeof v);
    v.has = TRACE_KEY_BANK, v.bank_deg = round((degrees < -most ? -most : degrees > most ? most : degrees) * 10) / 10;
    app_set_scoped(a, point, &v);
}

void app_clear_point_bank(App *a, int point)
{
    if (a->kind != APP_KIND_TRACK) return;
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    if (at < 0 || !(a->trace.keys[at].has & TRACE_KEY_BANK)) return;
    track_trace_key_clear_bank(a->trace.keys, &a->trace.key_count, point);
    a->dirty = 1;
}

void app_remove_point(App *a)
{
    if (a->kind != APP_KIND_TRACK) return;
    if (a->selected < 0) {
        set_status(a, "select a point first (click it)");
        return;
    }
    if (a->trace.count <= TRACE_POINTS_MIN) {
        char text[64];
        snprintf(text, sizeof text, "a trace keeps %d points", TRACE_POINTS_MIN);
        set_status(a, text);
        return;
    }
    TracePoint *fewer = malloc(sizeof *fewer * (size_t)(a->trace.count - 1));
    if (fewer == NULL) {
        set_status(a, "out of memory");
        return;
    }
    memcpy(fewer, a->trace.points, sizeof *fewer * (size_t)a->selected);
    memcpy(fewer + a->selected, a->trace.points + a->selected + 1, sizeof *fewer * (size_t)(a->trace.count - a->selected - 1));
    const int next = a->selected < a->trace.count - 1 ? a->selected : a->trace.count - 2;
    track_trace_keys_remove_point(a->trace.keys, &a->trace.key_count, a->selected);   // its key goes with it
    put_points(a, fewer, a->trace.count - 1, next);
    set_status(a, "point removed");
}

/* the car as a job left it (Convert PSD): the lists of textures and masks, and the card (nothing on screen was edited while it ran) */
void app_car_reread(App *a)
{
    CarProject *fresh = malloc(sizeof *fresh);
    char why[APP_MSG], card[CAR_PROJECT_PATH + 16];
    if (fresh == NULL) return;
    if (!car_project_detect(a->dir, fresh, why, sizeof why)) {
        snprintf(a->status + strlen(a->status), sizeof a->status - strlen(a->status), "; the car no longer reads: %s", why);
    } else {
        a->car = *fresh;
    }
    free(fresh);
    snprintf(card, sizeof card, "%s/mod.ini", a->car.root);
    CarEdit next;
    if (!car_edit_modified(&a->edit) && car_edit_open(&next, card, why, sizeof why)) {
        car_edit_free(&a->edit);
        a->edit = next;
    }
    app_mask_scan(a);   /* the job may have changed the textures and the masks */
    const MaskStamp kept = a->cdisk.card;
    const int edits = car_edit_modified(&a->edit);
    app_card_arm(a);
    if (edits) a->cdisk.card = kept;   /* edits kept over a card the job wrote: the next look says «Changed on disk» */
    a->cdisk.serial++;   /* the Model reads the car file again */
}

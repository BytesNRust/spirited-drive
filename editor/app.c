#include "app.h"

#include "build.h"
#include "game_find.h"

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
    free(a->nodes);
    free(a->flags);
    a->nodes = NULL;
    a->flags = NULL;
    a->node_count = 0;
    a->road_made = 0;
    free(a->node_dist);
    free(a->point_dist);
    free(a->point_turn);
    a->node_dist = a->point_dist = a->point_turn = NULL;
    a->point_count = 0;
    a->node_for = -2;   // what the Node window shows is taken again from the selected point once the road is made again
    free(a->problems);
    a->problems = NULL;
    a->problem_count = 0;
    a->summary_count = 0;
}

void app_begin(App *a, const char *game, const char *user, const char *generic_art)
{
    memset(a, 0, sizeof *a);
    snprintf(a->game, sizeof a->game, "%s", game);
    snprintf(a->user, sizeof a->user, "%s", user);
    snprintf(a->generic, sizeof a->generic, "%s", generic_art != NULL ? generic_art : "");
    a->selected = -1;
    a->node = a->node_for = -1;
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
    a->kind = APP_KIND_TRACK;
    a->saved_print = session_fingerprint(&a->trace);
    a->dirty = 1;
    app_recompute(a);
    view_fit(&a->view, a->trace.points, a->trace.count, 960, 600);
    set_status(a, "opened: arrows and keys are listed at the bottom");
    return 1;
}

/* the car at `dir` as the project of `a`, which has nothing open (app_begin) */
static int car_into(App *a, const char *dir)
{
    char why[APP_MSG];
    if (!car_project_detect(dir, &a->car, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    char card[CAR_PROJECT_PATH + 16];
    snprintf(card, sizeof card, "%s/mod.ini", a->car.piece);
    if (!car_edit_open(&a->edit, card, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    snprintf(a->dir, sizeof a->dir, "%s", a->car.root);
    a->kind = APP_KIND_CAR;
    set_status(a, "opened: edit the card and the colors; Save writes mod.ini only");
    return 1;
}

int app_open_any(App *a, const char *dir, const char *game, const char *user, const char *generic_art)
{
    if (!recent_is_project(dir) && car_project_present(dir)) {
        app_begin(a, game, user, generic_art);
        return car_into(a, dir);
    }
    return app_open(a, dir, game, user, generic_art);
}

void app_free(App *a)
{
    rehacer_free(&a->rehacer);
    car_edit_free(&a->edit);
    free(a->trace.points);
    drop_road(a);
    a->trace.points = NULL;
    a->trace.count = 0;
}

/* the node nearest control point `i`: by its distance along the curve over the road's (equal, to a few centimetres) node spacing */
static int nearest_node(const App *a, int i)
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
    a->road_text[0] = '\0';
    if (!track_trace_nodes(a->trace.points, a->trace.count, a->trace.width, &a->nodes, &a->node_count, &err)) {
        snprintf(a->road_text, sizeof a->road_text, "%s", err.message);
        return;
    }
    a->flags = calloc((size_t)a->node_count, 1);
    if (a->flags == NULL) {
        drop_road(a);
        snprintf(a->road_text, sizeof a->road_text, "out of memory");
        return;
    }
    a->road_made = 1;
    track_trace_flags(a->nodes, a->node_count, a->flags);
    a->node_dist = calloc((size_t)a->node_count, sizeof *a->node_dist);
    a->point_dist = calloc((size_t)a->trace.count, sizeof *a->point_dist);
    a->point_turn = calloc((size_t)a->trace.count, sizeof *a->point_turn);
    if (a->node_dist != NULL && a->point_dist != NULL && a->point_turn != NULL) {
        a->point_count = a->trace.count;
        track_trace_distances(a->nodes, a->node_count, a->node_dist);
        track_trace_point_distances(a->trace.points, a->trace.count, a->point_dist);
        for (int i = 0; i < a->trace.count; ++i) a->point_turn[i] = track_trace_signed_turn(a->nodes, nearest_node(a, i));
    } else {   // no memory for the figures: the road is as made, without them
        free(a->node_dist);
        free(a->point_dist);
        free(a->point_turn);
        a->node_dist = a->point_dist = a->point_turn = NULL;
    }
    a->summary_count = track_trace_summary(a->flags, a->node_count, a->summary);
    a->problem_count = track_trace_problems(a->nodes, a->node_count, a->flags, NULL, 0);
    if (a->problem_count > 0) {
        a->problems = calloc((size_t)a->problem_count, sizeof *a->problems);
        if (a->problems != NULL) track_trace_problems(a->nodes, a->node_count, a->flags, a->problems, a->problem_count);
        else a->problem_count = 0;
    }
}

void app_sync_selection(App *a)
{
    if (a->selected != a->node_for) {
        a->node_for = a->selected;
        a->node = a->selected >= 0 && a->road_made && a->point_dist != NULL && a->selected < a->point_count ? nearest_node(a, a->selected) : -1;
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

int app_modified(const App *a)
{
    if (a->kind == APP_KIND_CAR) return car_edit_modified(&a->edit);
    return a->kind == APP_KIND_TRACK && session_fingerprint(&a->trace) != a->saved_print;
}

void app_ask(App *a, AppAction act, const char *path)
{
    if (act == APP_ACT_NONE || a->confirm != APP_ACT_NONE || a->go != APP_ACT_NONE) return;
    if (a->rehacer.running) {   /* the script is working on the kit: nothing else until it ends */
        set_status(a, "Rehacer is running: wait for it to end, or press Stop");
        return;
    }
    if (app_modified(a)) {
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
    }
    a->confirm = APP_ACT_NONE;
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
    next->recent = a->recent;
    memcpy(next->recent_file, a->recent_file, sizeof next->recent_file);
    memcpy(next->game_tried, a->game_tried, sizeof next->game_tried);   /* what the game lookup found stays with the window, not with a project */
    memcpy(next->game_memo, a->game_memo, sizeof next->game_memo);
    app_free(a);
    *a = *next;   // the new project's arrays belong to `a` now
    free(next);
    a->fit_pending = 1;
    remember(a);
    return 1;
}

int app_open_project(App *a, const char *dir)
{
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
    set_status(a, a->kind == APP_KIND_CAR ? "a car has no Build or Install here yet" : "open a track first");
    return 0;
}

int app_save(App *a)
{
    char why[APP_MSG];
    if (a->kind == APP_KIND_CAR) {   /* the card: only mod.ini, only the lines that changed */
        const int changed = car_edit_modified(&a->edit);
        if (!car_edit_save(&a->edit, why, sizeof why)) {
            set_status(a, why);
            return 0;
        }
        set_status(a, changed ? "saved mod.ini" : "nothing to save: mod.ini is as it was");
        return 1;
    }
    if (!track_open(a)) return 0;
    if (!project_save_trace(a->dir, &a->trace, why, sizeof why)) {
        set_status(a, why);
        return 0;
    }
    a->saved_print = session_fingerprint(&a->trace);
    set_status(a, "saved trace.json");
    return 1;
}

int app_build(App *a)
{
    char why[APP_MSG], zip[APP_PATH];
    if (!track_open(a)) return 0;
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
    if (!track_open(a)) return 0;
    if (!game_there(a)) return game_missing_status(a, "install: ");
    if (a->zip[0] == '\0') {
        set_status(a, "build first (B): install takes the built package");
        return 0;
    }
    char why[APP_MSG];
    if (!install_zip(a->game, a->zip, a->user, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "install failed: %s", why);
        return 0;
    }
    snprintf(a->status, sizeof a->status, "installed into %s", a->user[0] != '\0' ? a->user : "the game's player folder");
    return 1;
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
    const TracePoint mid = { (p->x + q->x) / 2, (p->z + q->z) / 2 };
    TracePoint *more = points_with(a, a->selected + 1, mid);
    if (more == NULL) {
        set_status(a, "out of memory");
        return;
    }
    put_points(a, more, a->trace.count + 1, a->selected + 1);
    set_status(a, "point inserted after the selected one");
}

void app_remove_point(App *a)
{
    if (a->kind != APP_KIND_TRACK) return;
    if (a->selected < 0) {
        set_status(a, "select a point first (click it)");
        return;
    }
    if (a->trace.count <= 2) {
        set_status(a, "a trace keeps 2 points");
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
    put_points(a, fewer, a->trace.count - 1, next);
    set_status(a, "point removed");
}

/* ---- Rehacer ---- */

int app_rehacer_busy(const App *a)
{
    return a->rehacer.running;
}

int app_rehacer_available(const App *a)
{
    return a->kind == APP_KIND_CAR && rehacer_script_present(a->dir);
}

/* the car as the script left it: the lists of textures and masks, and the card (nothing on screen was edited while it ran) */
static void reload_car(App *a)
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
    snprintf(card, sizeof card, "%s/mod.ini", a->car.piece);
    CarEdit next;
    if (!car_edit_modified(&a->edit) && car_edit_open(&next, card, why, sizeof why)) {
        car_edit_free(&a->edit);
        a->edit = next;
    }
}

int app_rehacer(App *a)
{
    char why[APP_MSG], stamp[32];
    if (a->kind != APP_KIND_CAR) return set_status(a, "open a car first"), 0;
    if (a->rehacer.running) return set_status(a, "Rehacer is already running"), 0;
    if (!rehacer_script_present(a->dir)) return set_status(a, "this kit has no rehacer.command, so there is nothing to run"), 0;
    if (car_edit_modified(&a->edit)) return set_status(a, "mod.ini has changes that are not saved: save them first, then run Rehacer"), 0;
    rehacer_free(&a->rehacer);
    rehacer_stamp(stamp, sizeof stamp);
    if (!rehacer_backup(a->dir, stamp, REHACER_KEEP, REHACER_BACKUP_MAX_MB, a->rehacer.backup, sizeof a->rehacer.backup, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "Rehacer did not run, the backup failed: %s", why);
        return 0;
    }
    if (!rehacer_start(&a->rehacer, a->dir, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "Rehacer did not start: %s", why);
        return 0;
    }
    const char *slash = strrchr(a->rehacer.backup, '/');
    snprintf(a->status, sizeof a->status, "Rehacer running; the kit was backed up in %s/%s", REHACER_BACKUPS, slash != NULL ? slash + 1 : a->rehacer.backup);
    return 1;
}

int app_rehacer_poll(App *a)
{
    if (!a->rehacer.running) return 0;
    if (rehacer_poll(&a->rehacer)) return 1;
    if (a->rehacer.stopped) snprintf(a->status, sizeof a->status, "Rehacer was stopped (exit code %d)", a->rehacer.exit_code);
    else if (a->rehacer.exit_code == 0) snprintf(a->status, sizeof a->status, "Rehacer finished: exit code 0");
    else snprintf(a->status, sizeof a->status, "Rehacer failed: exit code %d (see the log)", a->rehacer.exit_code);
    reload_car(a);
    return 0;
}

void app_rehacer_stop(App *a)
{
    rehacer_stop(&a->rehacer);
}

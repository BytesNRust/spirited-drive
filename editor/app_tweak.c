/* Edit tweak, step 1, at the App level (app.h's app_tweak_*): the tweak folder of the open original car or track in <workspace>/tweaks/<id>/, and Apply to the game repo. No window yet: the steps
 * tweak-make and tweak-apply drive it. */
#include "app.h"
#include "car_export_kit.h"
#include "car_race.h"
#include "tweak.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int car_target(const App *a, char *dir, size_t cap, char *why, size_t why_cap)
{
    const ExportCar *car = a->kind == APP_KIND_CAR ? car_race_original_of(&a->car) : NULL;
    char id[64];
    if (car == NULL) return snprintf(why, why_cap, "only one of the game's own cars has a tweak"), 0;
    if (a->workspace[0] == '\0') return snprintf(why, why_cap, "choose a workspace first"), 0;
    tweak_id(car->folder, id, sizeof id);
    snprintf(dir, cap, "%s/tweaks/%s", a->workspace, id);
    return 1;
}

int app_tweak_make(App *a)
{
    char dir[APP_PATH], why[APP_MSG] = "", masks[APP_PATH] = "", dash[APP_PATH] = "", psd[APP_PATH] = "";
    if (a->kind == APP_KIND_CAR) {
        const ExportCar *car = car_race_original_of(&a->car);
        if (!car_target(a, dir, sizeof dir, why, sizeof why)) return snprintf(a->status, sizeof a->status, "tweak: %s", why), 0;
        if (a->car.has_masks_dir) car_project_masks_dir(&a->car, masks, sizeof masks);
        car_project_dash_dir(&a->car, dash, sizeof dash);
        car_project_edit_dir(&a->car, psd, sizeof psd);
        const int colours = car->kind == EXPORT_DRIVABLE && !a->car.masks_only;
        if (!tweak_make_car(dir, car, masks, dash, psd, a->edit.now.paints, colours ? a->edit.now.paint_count : 0, why, sizeof why))
            return snprintf(a->status, sizeof a->status, "tweak: %s", why), 0;
        return snprintf(a->status, sizeof a->status, "tweak: %s", dir), 1;
    }
    char stem[16], light[APP_PATH + 64], gen[APP_PATH + 64];
    if (a->original_dir[0] == '\0' || !original_track_stem_of(a->original_dir, stem, sizeof stem)) return snprintf(a->status, sizeof a->status, "tweak: open one of the game's own cars or tracks first"), 0;
    if (a->workspace[0] == '\0') return snprintf(a->status, sizeof a->status, "tweak: choose a workspace first"), 0;
    char id[64];
    tweak_id(stem, id, sizeof id);
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, id);
    snprintf(light, sizeof light, "%s/light/track_light/%s.txt", a->original_dir, stem);
    snprintf(gen, sizeof gen, "%s/art-generated/track_masks", a->original_dir);
    snprintf(psd, sizeof psd, "%s/art-source/masks", a->original_dir);
    if (!tweak_make_track(dir, stem, light, gen, psd, why, sizeof why)) return snprintf(a->status, sizeof a->status, "tweak: %s", why), 0;
    return snprintf(a->status, sizeof a->status, "tweak: %s", dir), 1;
}

int app_tweak_apply(App *a, const char *id)
{
    TweakApplied done;
    char why[APP_MSG] = "", dir[APP_PATH];
    if (a->workspace[0] == '\0') return snprintf(a->status, sizeof a->status, "apply to game repo: choose a workspace first"), 0;
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, id);
    if (app_repo_data(a)[0] == '\0') return snprintf(a->status, sizeof a->status, "apply to game repo: the game repo's data/ is not set (File > Settings...)"), 0;
    if (!tweak_apply(dir, app_repo_data(a), &done, why, sizeof why)) return snprintf(a->status, sizeof a->status, "apply to game repo: %s", why), 0;
    return snprintf(a->status, sizeof a->status, "applied to the game repo: %d car mask%s, %d dash file%s, %d light file%s, %d track mask%s, colors %s",
        done.masks, done.masks == 1 ? "" : "s", done.dash, done.dash == 1 ? "" : "s", done.light, done.light == 1 ? "" : "s", done.track_masks, done.track_masks == 1 ? "" : "s", done.colors ? "written" : "unchanged"), 1;
}

/* ---- step 2: the list, Open, and the Tweak panel's calls ---- */

static int exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

const char *app_tweak_base(const App *a)
{
    if (app_repo_data(a)[0] != '\0') return app_repo_data(a);
    return a->data_dir[0] != '\0' ? a->data_dir : a->export_src.data;
}

static void row_fill(App *a, TweakRow *r, const char *id, const char *project)
{
    char dir[APP_PATH], mod[APP_PATH + 16];
    snprintf(r->id, sizeof r->id, "%s", id);
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, id);
    snprintf(mod, sizeof mod, "%s/mod.ini", dir);
    r->has = a->workspace[0] != '\0' && exists(mod);
    r->old = !r->has && a->workspace[0] != '\0' && project != NULL && exists(project);
    if (r->has) {
        char why[APP_MSG] = "";
        static TweakStatus scratch;   /* the panel's own status (a->tweak_st) is made when a tweak opens */
        if (tweak_status(dir, app_tweak_base(a), &scratch, why, sizeof why)) snprintf(r->summary, sizeof r->summary, "%s", scratch.summary);
        else snprintf(r->summary, sizeof r->summary, "%s", why);
    } else if (r->old) {
        snprintf(r->summary, sizeof r->summary, "from the old workspace");
    }
}

void app_tweak_list_refresh(App *a)
{
    a->tweak_rows = 0;
    for (int i = 0; i < EXPORT_CARS; ++i) {
        const ExportCar *car = car_export_car(i);
        TweakRow *r = &a->tweak_row[a->tweak_rows++];
        char id[64], kit[96], project[APP_PATH];
        memset(r, 0, sizeof *r);
        snprintf(r->name, sizeof r->name, "%s", car->label);
        snprintf(r->kind, sizeof r->kind, "%s", car->kind == EXPORT_DRIVABLE ? "drivable" : car->kind == EXPORT_TRAFFIC ? "traffic" : "police");
        r->usable = car_export_available(&a->export_src, i, r->why, sizeof r->why);
        tweak_id(car->folder, id, sizeof id);
        car_export_kit_folder(i, kit, sizeof kit);
        if (!workspace_path(a->workspace, WORKSPACE_KIND_CAR, kit, project, sizeof project)) project[0] = '\0';   /* the old workspace's working copy: what "from the old workspace" is */
        row_fill(a, r, id, project);
    }
    for (int i = 0; i < OT_STEMS; ++i) {
        const char *stem = original_track_stem(i);
        TweakRow *r = &a->tweak_row[a->tweak_rows++];
        char id[64], project[APP_PATH];
        memset(r, 0, sizeof *r);
        snprintf(r->name, sizeof r->name, "%s", stem);
        snprintf(r->kind, sizeof r->kind, "track");
        r->usable = a->data_dir_ok;
        if (!r->usable) snprintf(r->why, sizeof r->why, "the game's data/ is not found: %s", a->data_dir[0] != '\0' ? a->data_dir : "(none)");
        tweak_id(stem, id, sizeof id);
        if (!original_track_old_dir(a->workspace, stem, project, sizeof project) || !original_track_present(project)) project[0] = '\0';   /* the old workspace's working copy */
        row_fill(a, r, id, project[0] != '\0' ? project : NULL);
    }
    if (a->tweak_pick >= a->tweak_rows) a->tweak_pick = -1;
}

int app_tweak_active(const App *a)
{
    if (a->kind == APP_KIND_CAR) return car_race_original_of(&a->car) != NULL;
    return a->kind == APP_KIND_LIGHT && a->original_dir[0] != '\0';
}

/* the open tweak's folder name; 0 when what is open has none */
static int open_id(const App *a, char *id, size_t cap)
{
    if (a->kind == APP_KIND_CAR && car_race_original_of(&a->car) != NULL) return tweak_id(car_race_original_of(&a->car)->folder, id, cap), 1;
    char stem[16];
    if (a->kind == APP_KIND_LIGHT && a->original_dir[0] != '\0' && original_track_stem_of(a->original_dir, stem, sizeof stem)) return tweak_id(stem, id, cap), 1;
    return 0;
}

int app_tweak_refresh(App *a)
{
    char id[64], dir[APP_PATH];
    a->tweak_st_ok = 0;
    a->tweak_why[0] = '\0';
    a->tweak_seen = a->tmask_serial;   /* also when it fails: the panel does not try again every frame */
    if (!open_id(a, id, sizeof id)) return snprintf(a->tweak_why, sizeof a->tweak_why, "only one of the game's own cars or tracks has a tweak"), 0;
    char said[APP_MSG];
    snprintf(said, sizeof said, "%s", a->status);   /* making the tweak again is the panel's own work: what the status said stays */
    const int made = app_tweak_make(a);
    if (!made) snprintf(a->tweak_why, sizeof a->tweak_why, "%s", a->status);
    snprintf(a->status, sizeof a->status, "%s", said);
    if (!made) return 0;
    snprintf(a->tweak_id, sizeof a->tweak_id, "%s", id);
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, id);
    const char *base = app_tweak_base(a);
    if (base[0] != '\0' && !tweak_base_write(dir, base, a->tweak_why, sizeof a->tweak_why)) return 0;
    if (!tweak_status(dir, base, &a->tweak_st, a->tweak_why, sizeof a->tweak_why)) return 0;
    a->tweak_st_ok = 1;
    a->tweak_seen = a->tmask_serial;
    return 1;
}

int app_tweak_check(App *a)
{
    char dir[APP_PATH];
    if (!app_tweak_refresh(a)) return snprintf(a->tweak_report, sizeof a->tweak_report, "%s", a->tweak_why), 0;
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, a->tweak_id);
    return tweak_check(dir, a->tweak_report, sizeof a->tweak_report) && tweak_check_masks(dir, a->tweak_report, sizeof a->tweak_report) && snprintf(a->tweak_report, sizeof a->tweak_report, "ok");
}

int app_tweak_pack(App *a)
{
    char dir[APP_PATH], build[APP_PATH + 8], zip[APP_PATH + 64], report[APP_MSG];
    if (!app_tweak_refresh(a)) return snprintf(a->status, sizeof a->status, "pack zip: %s", a->tweak_why), 0;
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, a->tweak_id);
    snprintf(build, sizeof build, "%s/build", dir);
    snprintf(zip, sizeof zip, "%s/%s.zip", build, a->tweak_id);
    mkdir(build, 0777);
    const int ok = tweak_pack(dir, app_tweak_base(a), zip, 1, report, sizeof report);
    snprintf(a->status, sizeof a->status, "%s%s", ok ? "pack zip: " : "pack zip: not made: ", report);
    return ok;
}

/* the folders above `path` (the tweak's studio/), so a kit can be made in it */
static int make_parents(const char *path)
{
    char tmp[APP_PATH];
    snprintf(tmp, sizeof tmp, "%s", path);
    char *last = strrchr(tmp, '/');
    if (last == NULL) return 0;
    *last = '\0';
    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0777) != 0 && errno != EEXIST) return 0;
        *p = '/';
    }
    return mkdir(tmp, 0777) == 0 || errno == EEXIST;
}

/* what bringing an older workspace's working copy in said, for the status line: the old folder is left, the owner deletes it by hand */
static void converted_note(char *note, size_t cap, const char *old, int files, const char *skipped)
{
    snprintf(note, cap, "; copied %d files from the old %s into the tweak (the old folder is left: delete it by hand when you want; later edits there are not read)%s%s", files, old, skipped[0] != '\0' ? "; " : "", skipped);
}

/* an own tweak with the id of an original is not the original's working copy: it is left alone */
static int refuse_foreign(App *a, const char *label, const char *home)
{
    if (original_home_state(home) != OH_FOREIGN) return 0;
    snprintf(a->open_error, sizeof a->open_error, "Could not open %s: %s exists and is not a game original's working copy (an own tweak with that id): rename or move it", label, home);
    snprintf(a->status, sizeof a->status, "%s", a->open_error);
    return 1;
}

int app_tweak_open_car(App *a, int index)
{
    char kit[96], id[64], old[APP_PATH], home[APP_PATH], why[APP_MSG] = "", note[APP_PATH + 160] = "";
    const ExportCar *car = car_export_car(index);
    if (car == NULL) return 0;
    car_export_kit_folder(index, kit, sizeof kit);
    tweak_id(car->folder, id, sizeof id);
    if (a->workspace[0] == '\0' || !original_home(a->workspace, id, home, sizeof home) || !workspace_path(a->workspace, WORKSPACE_KIND_CAR, kit, old, sizeof old)) return snprintf(a->status, sizeof a->status, "choose a workspace first (File > Workspace)"), 0;
    int files = 0;
    if (refuse_foreign(a, car->label, home)) return 0;
    const int converted = original_convert(old, home, &files, why, sizeof why);   /* one at a time, when its tweak is opened; the old folder stays */
    if (converted < 0) {
        snprintf(a->open_error, sizeof a->open_error, "Could not open %s: %s", car->label, why);
        return snprintf(a->status, sizeof a->status, "%s", a->open_error), 0;
    }
    if (converted == 1) converted_note(note, sizeof note, old, files, why);
    const int fresh = !exists(home);   /* no working copy yet (and none came from the old folder): made from the game */
    if (fresh && !make_parents(home)) {
        snprintf(a->open_error, sizeof a->open_error, "Could not open %s: cannot make %s", car->label, home);
        return snprintf(a->status, sizeof a->status, "%s", a->open_error), 0;
    }
    if (fresh && (!original_mark_home(home) || !car_export_kit(&a->export_src, index, home, why, sizeof why))) {   /* the project the tweak is made from, as the old Export original car made it; a failed one leaves no half project */
        original_drop_home(home);
        snprintf(a->open_error, sizeof a->open_error, "Could not open %s: %s", car->label, why[0] != '\0' ? why : "cannot write the mark of the working copy");
        return snprintf(a->status, sizeof a->status, "%s", a->open_error), 0;
    }
    if (!app_open_tweak_car(a, home)) return 0;
    app_workspace_refresh(a);
    if (!app_tweak_refresh(a)) return snprintf(a->status, sizeof a->status, "opened %s, but its tweak was not made: %s", car->label, a->tweak_why), 0;
    return snprintf(a->status, sizeof a->status, "tweak %s: %s/tweaks/%s%s", car->label, a->workspace, a->tweak_id, note), 1;
}

int app_tweak_open_track(App *a, int index)
{
    char dir[APP_PATH], old[APP_PATH], why[OT_WHY] = "", note[APP_PATH + 160] = "";
    const char *stem = original_track_stem(index);
    if (stem == NULL) return 0;
    if (a->workspace[0] == '\0' || !original_track_dir(a->workspace, stem, dir, sizeof dir) || !original_track_old_dir(a->workspace, stem, old, sizeof old)) return snprintf(a->status, sizeof a->status, "choose a workspace first (File > Workspace)"), 0;
    int files = 0;
    if (refuse_foreign(a, stem, dir)) return 0;
    if (original_track_present(old)) {   /* an older workspace's folder is copied in once, with what was edited in it; the old one stays */
        const int converted = original_convert(old, dir, &files, why, sizeof why);
        if (converted < 0) {
            snprintf(a->open_error, sizeof a->open_error, "Could not open %s: %s", stem, why);
            return snprintf(a->status, sizeof a->status, "%s", a->open_error), 0;
        }
        if (converted == 1) converted_note(note, sizeof note, old, files, why);
    }
    if (original_home_state(dir) == OH_ABSENT && !original_mark_home(dir)) {
        snprintf(a->open_error, sizeof a->open_error, "Could not open %s: cannot write the mark of %s", stem, dir);
        return snprintf(a->status, sizeof a->status, "%s", a->open_error), 0;
    }
    if (a->data_dir_ok) {   /* the track is made from the game's files when the tweak has none, and a folder from an older version is brought up to date (what was edited is never touched) */
        OriginalAdd add = { 0 };
        if (!original_track_add(a->workspace, stem, a->data_dir, a->art_dir[0] != '\0' ? a->art_dir : NULL, &add, why, sizeof why) && !original_track_present(dir)) {
            snprintf(a->open_error, sizeof a->open_error, "Could not open %s: %s", stem, why);
            return snprintf(a->status, sizeof a->status, "%s", a->open_error), 0;
        }
    } else if (!original_track_present(dir)) {
        snprintf(a->open_error, sizeof a->open_error, "Could not open %s: the game's data/ is not found (File > Settings...)", stem);
        return snprintf(a->status, sizeof a->status, "%s", a->open_error), 0;
    }
    if (!app_open_original(a, dir)) return 0;
    app_workspace_refresh(a);
    if (!app_tweak_refresh(a)) return snprintf(a->status, sizeof a->status, "opened %s, but its tweak was not made: %s", stem, a->tweak_why), 0;
    return snprintf(a->status, sizeof a->status, "tweak %s: %s/tweaks/%s%s", stem, a->workspace, a->tweak_id, note), 1;
}

int app_tweak_open_row(App *a, int row)
{
    if (row < 0 || row >= a->tweak_rows || !a->tweak_row[row].usable) return 0;
    return row < EXPORT_CARS ? app_tweak_open_car(a, row) : app_tweak_open_track(a, row - EXPORT_CARS);
}

static int apply_now(App *a)
{
    char dir[APP_PATH], why[APP_MSG] = "";
    TweakApplied done;
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, a->tweak_id);
    if (!tweak_apply_skipping(dir, app_repo_data(a), &done, why, sizeof why)) return snprintf(a->status, sizeof a->status, "apply to game repo: %s", why), 0;
    char said[APP_MSG];
    snprintf(said, sizeof said, "applied to the game repo: %d file%s written, %d left as the repo has them, colors %s",
        done.masks + done.dash + done.light + done.track_masks, done.masks + done.dash + done.light + done.track_masks == 1 ? "" : "s", done.skipped, done.colors ? "written" : "unchanged");
    app_tweak_refresh(a);   /* the list shows what is left */
    snprintf(a->status, sizeof a->status, "%s", said);
    return 1;
}

int app_tweak_apply_open(App *a)
{
    if (app_repo_data(a)[0] == '\0') {   /* the first time: the repo's folder is asked for, here and only here, and kept */
        app_repo_data_pick(a);
        return snprintf(a->status, sizeof a->status, "choose the game repo's folder, then Apply again"), 0;
    }
    if (!app_tweak_refresh(a)) return snprintf(a->status, sizeof a->status, "apply to game repo: %s", a->tweak_why), 0;
    a->tweak_confirm_dialog = 1;   /* what it writes is listed first, as Copy to game repo did: nothing is written before the answer */
    return 0;
}

int app_tweak_apply_confirmed(App *a)
{
    a->tweak_confirm_dialog = 0;
    if (!app_tweak_refresh(a)) return snprintf(a->status, sizeof a->status, "apply to game repo: %s", a->tweak_why), 0;
    if (a->tweak_st.own > 0) return a->tweak_own_dialog = 1, 0;
    return apply_now(a);
}

int app_tweak_apply_answer(App *a, int replace)
{
    a->tweak_own_dialog = 0;
    if (!replace) return apply_now(a);
    /* Replace: the repo's own changes are overwritten: the base is forgotten, so nothing is "its own" */
    char base[APP_PATH];
    snprintf(base, sizeof base, "%s/tweaks/%s/studio/base.txt", a->workspace, a->tweak_id);
    remove(base);
    return apply_now(a);
}

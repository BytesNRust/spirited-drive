/* The workspace of the App (app.h's app_workspace_*): the folder with cars/ and tracks/, what it holds, and where the folder dialogs start. */
#include "app.h"

#include "formats/mod_ini.h"
#include "track_undo.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int present(const char *dir, int kind)
{
    if (kind == WORKSPACE_KIND_TRACK ? original_track_present(dir) : car_race_original_kit(dir)) return 0;   // a game original's working copy from an older workspace: it opens through Edit tweak, not here
    return recent_present(dir, kind == WORKSPACE_KIND_CAR ? RECENT_CAR : RECENT_TRACK);
}

void app_workspace_refresh(App *a)
{
    workspace_scan(a->workspace, present, &a->workspace_list);
}

static void keep(const App *a)
{
    if (a->workspace_file[0] != '\0') (void)workspace_save(a->workspace_file, a->workspace, a->workspace_put_aside);
}

void app_workspace_keep(App *a, const char *file)
{
    snprintf(a->workspace_file, sizeof a->workspace_file, "%s", file != NULL ? file : "");
    workspace_load(a->workspace_file, a->workspace, sizeof a->workspace, &a->workspace_put_aside);
    char why[APP_MSG];
    if (a->workspace[0] != '\0' && !workspace_prepare(a->workspace, why, sizeof why)) a->workspace[0] = '\0';   /* the folder is gone: no workspace (the file keeps what it had) */
    workspace_scan(a->workspace, present, &a->workspace_list);
    workspace_load_repo(a->workspace_file, a->repo_cfg, sizeof a->repo_cfg);
}

const char *app_repo_data(const App *a)
{
    const char *dir = a->repo_arg[0] != '\0' ? a->repo_arg : a->repo_cfg;
    return app_light_in_bundle(dir) ? "" : dir;
}

void app_set_repo_arg(App *a, const char *dir)
{
    snprintf(a->repo_arg, sizeof a->repo_arg, "%s", dir != NULL ? dir : "");
}

int app_repo_data_set(App *a, const char *dir_or_empty)
{
    snprintf(a->repo_cfg, sizeof a->repo_cfg, "%s", dir_or_empty != NULL ? dir_or_empty : "");
    return workspace_save_repo(a->workspace_file, a->repo_cfg);
}

void app_repo_data_pick(App *a)
{
    if (a->go == APP_ACT_NONE && a->confirm == APP_ACT_NONE) a->go = APP_ACT_REPO_PICK;
}

static int is_dir(const char *dir, const char *sub)
{
    char path[APP_PATH];
    struct stat st;
    return snprintf(path, sizeof path, "%s/%s", dir, sub) < (int)sizeof path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void app_repo_data_apply(App *a)
{
    const char *const repo = app_repo_data(a);
    if (repo[0] == '\0') return;
    char art[APP_PATH];
    snprintf(art, sizeof art, "%s", a->art_dir);   // (app_set_game_data writes a->art_dir: not from itself)
    app_set_game_data(a, repo, art);   /* the game's data/ the original tracks are added from: the repo chosen is the one (an installed app has no source tree to fall back on) */
    if (a->light_dir[0] == '\0' || app_light_in_bundle(a->light_dir)) snprintf(a->light_dir, sizeof a->light_dir, "%s", repo);   /* and the light Studio tunes */
}

int app_repo_data_choose(App *a, const char *dir)
{
    char data[APP_PATH];
    snprintf(data, sizeof data, "%s", dir != NULL ? dir : "");
    if (!(is_dir(data, "car_masks") && is_dir(data, "track_light")) && is_dir(data, "data")) strncat(data, "/data", sizeof data - strlen(data) - 1);   /* the repo itself was chosen */
    if (data[0] == '\0' || !is_dir(data, "car_masks") || !is_dir(data, "track_light"))
        return snprintf(a->status, sizeof a->status, "game repo folder: %s is not the repo's data/ (it needs car_masks/ and track_light/ in it): nothing changed", data), 0;
    if (app_light_in_bundle(data))
        return snprintf(a->status, sizeof a->status, "game repo folder: %s is inside an app bundle: choose the repo's own data/", data), 0;
    if (!app_repo_data_set(a, data) && a->workspace_file[0] != '\0')   /* a headless run keeps no file: the choice holds for the run */
        return snprintf(a->status, sizeof a->status, "game repo folder: could not keep %s in the settings file", data), 0;
    a->repo_arg[0] = '\0';   /* what is chosen here is the one repo from now on: --port-data no longer shadows it (Copy writes where Add originals reads) */
    app_repo_data_apply(a);
    snprintf(a->status, sizeof a->status, "game repo folder: %s (Copy to game repo writes there; Add originals copies the light files from there)", data);
    return 1;
}

int app_workspace_set(App *a, const char *dir)
{
    char why[APP_MSG];
    if (!workspace_prepare(dir, why, sizeof why)) {
        snprintf(a->status, sizeof a->status, "workspace: %s", why);
        return 0;
    }
    snprintf(a->workspace, sizeof a->workspace, "%s", dir);
    a->workspace_put_aside = 0;
    workspace_scan(a->workspace, present, &a->workspace_list);
    keep(a);
    snprintf(a->status, sizeof a->status, "%d car%s, %d track%s in the workspace", a->workspace_list.cars, a->workspace_list.cars == 1 ? "" : "s", a->workspace_list.tracks, a->workspace_list.tracks == 1 ? "" : "s");
    return 1;
}

void app_workspace_clear(App *a)
{
    a->workspace[0] = '\0';
    a->workspace_put_aside = 1;
    memset(&a->workspace_list, 0, sizeof a->workspace_list);
    keep(a);
    snprintf(a->status, sizeof a->status, "no workspace: Open and New start where the system puts them");
}

void app_workspace_put_aside(App *a)
{
    a->workspace_put_aside = 1;
    keep(a);
}

int app_workspace_offer(const App *a)
{
    return a->workspace[0] == '\0' && !a->workspace_put_aside;
}

int app_workspace_item(const App *a, int kind, int i, char *out, size_t cap)
{
    if (kind != 0 && kind != 1) return 0;
    const int count = kind == 0 ? a->workspace_list.cars : a->workspace_list.tracks;
    if (i < 0 || i >= count) return 0;
    return workspace_path(a->workspace, kind == 0 ? WORKSPACE_KIND_CAR : WORKSPACE_KIND_TRACK, kind == 0 ? a->workspace_list.car[i] : a->workspace_list.track[i], out, cap);
}

void app_workspace_pick(App *a)
{
    if (a->go == APP_ACT_NONE && a->confirm == APP_ACT_NONE) a->go = APP_ACT_WORKSPACE_PICK;
}

void app_dialog_dir(const App *a, AppAction kind, char *out, size_t cap)
{
    if (kind == APP_ACT_WORKSPACE_PICK && a->workspace[0] != '\0') {   /* the workspace itself, to change it */
        snprintf(out, cap, "%s", a->workspace);
        return;
    }
    const int car = kind == APP_ACT_OPEN_CAR || kind == APP_ACT_EXPORT_PICK;
    workspace_start_dir(kind == APP_ACT_WORKSPACE_PICK ? "" : a->workspace, car ? WORKSPACE_KIND_CAR : WORKSPACE_KIND_TRACK, a->dir, out, cap);
}

int app_new_track_named(App *a, const char *name, const char *id)
{
    char made[MOD_INI_ID_MAX + 1], path[APP_PATH], text[APP_MSG];
    text[0] = '\0';
    if (a->workspace[0] == '\0') snprintf(text, sizeof text, "no workspace: New track asks for a folder");
    else if (name == NULL || name[strspn(name, " ")] == '\0') snprintf(text, sizeof text, "give the track a name");
    else {
        if (id == NULL || id[0] == '\0') id = workspace_id_from_name(name, made, sizeof made, MOD_INI_ID_MAX) ? made : "";
        if (!mod_ini_id_ok(id)) snprintf(text, sizeof text, "the id may hold only a-z, 0-9 and '-' (not first), up to %d", MOD_INI_ID_MAX);
        else if (!workspace_path(a->workspace, WORKSPACE_KIND_TRACK, id, path, sizeof path)) snprintf(text, sizeof text, "the workspace's path is too long");
        else {
            struct stat st;
            if (stat(path, &st) == 0) snprintf(text, sizeof text, "\"%s\" already exists in the workspace's tracks: choose another name or id", id);
        }
    }
    if (text[0] != '\0') {
        snprintf(a->new_track_error, sizeof a->new_track_error, "%s", text);
        snprintf(a->status, sizeof a->status, "%s", text);
        return 0;
    }
    if (!app_new_project(a, path)) return 0;
    char why[APP_MSG];   /* the project's title is what was typed; the folder and id are the id's */
    if (project_set_name(a->dir, name, why, sizeof why)) {
        snprintf(a->project.name, sizeof a->project.name, "%s", name);
        track_undo_free(&a->track_undo);   /* the name is in the file now: the saved state and the first step are the project as it is, so the new track is not "modified" */
        if (!track_undo_open(&a->track_undo, &a->project, &a->trace)) snprintf(a->status, sizeof a->status, "out of memory: the track's undo steps are off");
    } else {
        snprintf(a->status, sizeof a->status, "the track was made, its name was not kept: %s", why);
    }
    a->new_track_error[0] = '\0';
    app_workspace_refresh(a);
    return 1;
}

/* The workspace of the App (app.h's app_workspace_*): the folder with cars/ and tracks/, what it holds, and where the folder dialogs start. */
#include "app.h"

#include <stdio.h>
#include <string.h>

static int present(const char *dir, int kind)
{
    return recent_present(dir, kind == WORKSPACE_KIND_CAR ? RECENT_CAR : RECENT_TRACK);
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
    snprintf(a->status, sizeof a->status, "workspace: %s (%d cars, %d tracks)", dir, a->workspace_list.cars, a->workspace_list.tracks);
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

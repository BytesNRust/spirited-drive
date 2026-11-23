/* Original tracks of the App (app.h's app_original_*, dep103): added to the workspace from the game's data/, opened on the Track light window, published back. */
#include "app.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void app_set_game_data(App *a, const char *data_dir, const char *art_dir)
{
    snprintf(a->data_dir, sizeof a->data_dir, "%s", data_dir != NULL ? data_dir : "");
    a->data_dir_ok = a->data_dir[0] != '\0' && is_dir(a->data_dir);
    snprintf(a->art_dir, sizeof a->art_dir, "%s", art_dir != NULL ? art_dir : "");
    if (a->art_dir[0] == '\0' && a->data_dir[0] != '\0') {   // <repo>/data/../../../art/tnfs-masks/tracks: the worktree is dev/<name>, the art folder is beside dev/
        char probe[APP_PATH + 40];
        snprintf(probe, sizeof probe, "%s/../../../art/tnfs-masks/tracks", a->data_dir);
        if (is_dir(probe)) snprintf(a->art_dir, sizeof a->art_dir, "%s", probe);
    }
}

int app_original_count(const App *a)
{
    int n = 0;
    char dir[APP_PATH];
    for (int i = 0; a->workspace[0] != '\0' && i < OT_STEMS; ++i)
        n += original_track_dir(a->workspace, original_track_stem(i), dir, sizeof dir) && original_track_present(dir);
    return n;
}

/* how many of the workspace's original tracks hold at least one PSD in art-source/masks */
static int tracks_with_psds(const App *a)
{
    int n = 0;
    for (int i = 0; a->workspace[0] != '\0' && i < OT_STEMS; ++i) {
        char dir[APP_PATH], masks[APP_PATH + 24];
        if (!original_track_dir(a->workspace, original_track_stem(i), dir, sizeof dir) || snprintf(masks, sizeof masks, "%s/art-source/masks", dir) >= (int)sizeof masks) continue;
        DIR *d = opendir(masks);
        for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) {
            const size_t len = strlen(e->d_name);
            if (len > 4 && strcmp(e->d_name + len - 4, ".psd") == 0) { ++n; break; }
        }
        if (d != NULL) closedir(d);
    }
    return n;
}

int app_original_add_all(App *a)
{
    char why[OT_WHY] = "";
    if (a->workspace[0] == '\0') return snprintf(a->status, sizeof a->status, "original tracks: choose a workspace first"), 0;
    if (a->data_dir[0] == '\0') return snprintf(a->status, sizeof a->status, "original tracks: Studio has no game data folder to copy the light files from (choose File > Game repo folder...)"), 0;
    if (!is_dir(a->data_dir)) return snprintf(a->status, sizeof a->status, "original tracks: game data folder not found: %s (choose File > Game repo folder...)", a->data_dir), 0;
    OriginalAdd add = { 0 };
    const int ok = original_track_add_all(a->workspace, a->data_dir, a->art_dir[0] != '\0' ? a->art_dir : NULL, &add, why, sizeof why);
    app_workspace_refresh(a);
    if (!ok) return snprintf(a->status, sizeof a->status, "original tracks: %s", why), 0;
    char psd_words[240];   // what the mask PSDs did: copied, or why none were (no folder to copy from, or nothing new in it), and whether the workspace has its own
    const int own = tracks_with_psds(a);
    if (add.psds > 0) snprintf(psd_words, sizeof psd_words, "%d mask PSDs copied", add.psds);
    else if (a->art_dir[0] == '\0') snprintf(psd_words, sizeof psd_words, "0 mask PSDs copied: no PSD folder (--art-dir); %s", own > 0 ? "the workspace's own PSDs are kept" : "the workspace has none yet");
    else snprintf(psd_words, sizeof psd_words, "0 mask PSDs copied: none new in %s; %s", a->art_dir, own > 0 ? "the workspace's own PSDs are kept" : "the workspace has none yet");
    snprintf(a->status, sizeof a->status, "original tracks: %d in the workspace; %d light files copied, %s, %d kept as they were, %d other files skipped", add.tracks, add.light_files, psd_words, add.kept, add.skipped);
    return 1;
}

int app_open_original(App *a, const char *dir)
{
    char stem[16], light[APP_PATH], keep[APP_PATH];
    if (!original_track_stem_of(dir, stem, sizeof stem) || !original_track_light_dir(dir, light, sizeof light)) {
        snprintf(a->open_error, sizeof a->open_error, "%s is not an original track's folder", dir);
        snprintf(a->status, sizeof a->status, "%s", a->open_error);
        return 0;
    }
    int track = 0;
    while (light_ui_track(track) != NULL && strcmp(light_ui_track(track), stem) != 0) ++track;
    snprintf(keep, sizeof keep, "%s", a->light_dir);
    snprintf(a->light_dir, sizeof a->light_dir, "%s", light);
    if (!app_open_light_on(a, track)) {   // nothing changed: the old folder comes back
        snprintf(a->light_dir, sizeof a->light_dir, "%s", keep);
        return 0;
    }
    snprintf(a->original_dir, sizeof a->original_dir, "%s", dir);
    snprintf(a->status, sizeof a->status, "Original track %s: %s", stem, dir);
    return 1;
}

int app_original_can_copy(const App *a)
{
    return a->kind == APP_KIND_LIGHT && a->original_dir[0] != '\0' && app_repo_data(a)[0] != '\0';
}

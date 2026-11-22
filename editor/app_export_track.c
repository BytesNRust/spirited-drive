/* See app.h: Export original track. */
#include "app.h"

#include "formats/mod_ini.h"
#include "original_track_trace.h"

#include <stdio.h>
#include <string.h>

int app_export_original_track(App *a, const char *stem, const char *name, const char *id)
{
    char made[MOD_INI_ID_MAX + 1], tracks[APP_PATH], dir[APP_PATH], text[APP_MSG];
    text[0] = '\0';
    if (a->workspace[0] == '\0') snprintf(text, sizeof text, "no workspace: Export original track makes the track in the workspace's tracks/ folder");
    else if (name == NULL || name[strspn(name, " ")] == '\0') snprintf(text, sizeof text, "give the track a name");
    else if (stem == NULL || stem[0] == '\0') snprintf(text, sizeof text, "choose the original track");
    else {
        if (id == NULL || id[0] == '\0') id = workspace_id_from_name(name, made, sizeof made, MOD_INI_ID_MAX) ? made : "";
        if (snprintf(tracks, sizeof tracks, "%s/tracks", a->workspace) >= (int)sizeof tracks) snprintf(text, sizeof text, "the workspace's path is too long");
        else if (!original_track_export(a->export_src.cd, stem, tracks, id, name, a->generic[0] != '\0' ? a->generic : NULL, dir, sizeof dir, text, sizeof text)) {
            if (text[0] == '\0') snprintf(text, sizeof text, "the track was not made");
        }
    }
    if (text[0] != '\0') {
        snprintf(a->export_track_error, sizeof a->export_track_error, "%s", text);
        snprintf(a->status, sizeof a->status, "%s", text);
        return 0;
    }
    a->export_track_error[0] = '\0';
    app_workspace_refresh(a);
    return app_open_project(a, dir);
}

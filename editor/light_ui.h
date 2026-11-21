/* The Track light mode's state inside the App (dep98 step 2, part C): the open light file (light_session.h), the track, time of day and node of the photo, and the
 * photo job (light_photo.h). The functions that work on it are app_light_* (app.h, app_light.c). */
#ifndef TNFS_EDITOR_LIGHT_UI_H
#define TNFS_EDITOR_LIGHT_UI_H

#include "light_compare.h"
#include "light_photo.h"
#include "light_session.h"

enum {
    LIGHT_UI_TRACKS = 16,        /* the combo's entries: the 15 CD tracks that have light files and TR5, whose light is made in code */
    LIGHT_UI_TIMES = 4,          /* dawn, day, dusk, night */
    LIGHT_UI_MSG = 320
};
#define LIGHT_UI_SETTLE 0.5   /* seconds from the last change to the photo that follows it */

/* the track combo's entries, in the game's order: "AL1".."TR7"; NULL past LIGHT_UI_TRACKS. TR5 opens with nothing to edit. */
const char *light_ui_track(int index);
/* a time's letter 'A'..'N' and its name "Dawn".."Night" for index 0..3 */
char light_ui_time_letter(int time);
const char *light_ui_time_name(int time);

typedef struct {
    int open;                    /* the mode's session is on (kind APP_KIND_LIGHT) */
    LightSession session;        /* the working copy of the file of (track, time); session.has_file 0 for a track with none */
    int track;                   /* light_ui_track's index */
    int time;                    /* 0..3 */
    int node;                    /* the photo's node, within the road of the file */
    int view;                    /* the photo's camera, 0..LIGHT_VIEWS-1 (light_photo.h): kept as light_photo_view_pref */
    unsigned edit_serial;        /* counts every change of what the photo shows: an edit, a Revert, a track or time or node */
    unsigned photo_serial;       /* the edit_serial the last photo was started for */
    double now;                  /* the clock of the last app_light_poll (seconds): a change is stamped with it */
    double edit_at;              /* the clock of the last change: a photo follows when the clock is LIGHT_UI_SETTLE past it */
    LightPhotoJob photo;         /* the photo's job, running or last run */
    char error[LIGHT_UI_MSG];    /* why the last Save, select or photo was refused, until the next try; "" for none */
    LightCompare compare;        /* part D: the Before picture or the four times of day beside the photo (light_compare.h) */
} LightUi;

#endif

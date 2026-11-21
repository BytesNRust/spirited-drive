/* Check dash (dep98): the owner's loop for a dashboard (and the body masks) made in PSDs: the Masks window warns when a PSD is newer than the PNG made from it
 * (psd_stale.h), and «Check dash» does Convert PSD (app_psd.c) and then, when it ended well, takes the cockpit photo at night with the car's own gauge colours
 * (gauge light 5) with this project's dash masks, so the result is in the Drive window's picture after one click. */
#ifndef TNFS_TRACK_EDITOR_APP_DASH_H
#define TNFS_TRACK_EDITOR_APP_DASH_H

#include "app.h"

/* 1 when Check dash can start now; else 0 with the reason in `why` (not a car you drive, no PSD folder for it, no converter or python, a run in progress) */
int app_dash_possible(const App *a, char *why, size_t why_cap);
/* starts it: Convert PSD, then the cockpit photo when the conversion ended well; 1 when the conversion started, 0 with the status saying why */
int app_dash_check_start(App *a);
/* every frame, after app_psd_poll: takes the next step, keeps App.dash_stale up to date; 1 while the check waits for the conversion */
int app_dash_poll(App *a);
/* the warning again now (after a conversion, an open) */
void app_dash_stale_refresh(App *a);

#endif

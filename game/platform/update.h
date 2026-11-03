/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The update check: one background question to GitHub at start ("which is the latest release?"), never blocking a
 * frame. Pure parsing and version order are in core/update/update_check.h.
 *
 * Use from the game:
 *   platform_update_check_start(TNFS_VERSION);       once, after the user's "Check for updates" setting says On
 *   ...each frame or menu draw:
 *   char latest[32];
 *   if (platform_update_check_poll(latest, sizeof latest) == UPDATE_NEWER) show "Update available: v<latest>"
 *   ...when the player picks it:
 *   platform_open_releases_page();
 */
#ifndef TNFS_PLATFORM_UPDATE_H
#define TNFS_PLATFORM_UPDATE_H

#include <stddef.h>

typedef enum {
    UPDATE_PENDING = 0,   /* not started, or the answer has not come yet */
    UPDATE_NEWER = 1,     /* a newer release exists: `latest` holds its version */
    UPDATE_NONE = -1      /* up to date, a dev build, offline, or the answer could not be read */
} UpdateState;

/* Starts the check in the background (once; later calls do nothing): one GET to our API, which also counts the start. Dev builds (x.y.z-dev...) never check. */
void platform_update_check_start(const char *current_version);

/* The check's state; with UPDATE_NEWER, `latest` (cap >= 32) receives the newer version, e.g. "0.9.1". */
UpdateState platform_update_check_poll(char *latest, size_t cap);

/* Sends one anonymous event ("first_start", "data_ready" or "first_race") to our API on its own thread, never
 * blocking. port_stats.h decides whether and when; this only sends. */
void platform_stats_post(const char *event, const char *version);

/* Opens the latest release's download page in the player's browser; 0 when it could not. */
int platform_open_releases_page(void);

#endif

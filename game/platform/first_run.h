/* port (the first-run wizard): the player's CD folder chosen, checked and copied to the user's own folder, so --data is not needed afterwards. A window of the
 * platform's own dialogs (no game art is needed: there is none yet). */
#ifndef TNFS_FIRST_RUN_H
#define TNFS_FIRST_RUN_H

#include <stdbool.h>
#include <stddef.h>

/* The folder the wizard copies to (<user dir>/gamedata); false when the platform has no user folder. */
bool first_run_user_copy(char *out, size_t out_size);

/* Runs the wizard: explains, asks for the CD's folder, checks it (a folder that is not the PC SE's: another try, or use it anyway), copies it to `dest` with a progress
 * bar. Returns true with `dest` made, false when the player quit, the copy failed or there is no display (a headless run: the caller says to use --data). */
bool first_run_setup(const char *dest);

#endif

/* The editor's two outputs: the package (the zip the game installs) built from a project's source, and the install of that zip. The source is
 * never edited by a build, and the installed content is never touched in place: the game's own installer does the install. */
#ifndef TNFS_TRACK_EDITOR_BUILD_H
#define TNFS_TRACK_EDITOR_BUILD_H

#include "project.h"

#include <stddef.h>

/* Builds <dir>/build/<id>.zip from the project and its trace: the road's nodes (refused while a node breaks a limit), the piece's TRI, INFO
 * and day art taken from the template track of the data folder `data_dir` and the card, checked the way the game checks a track piece.
 * 1 with `zip` the path written; 0 with `why` naming the fault otherwise. */
int build_project(const char *dir, const Project *p, const Trace *t, const char *data_dir, char *zip, size_t zip_cap, char *why,
                  size_t why_cap);

/* Installs `zip` into the player folder `user_dir` the way the game's own installer does (`tnfs --user DIR --install ZIP`), by running the
 * game. 1 when it exits 0; 0 with `why` otherwise. */
int install_zip(const char *game, const char *zip, const char *user_dir, char *why, size_t why_cap);

#endif

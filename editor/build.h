/* The editor's two outputs: the package (the zip the game installs) built from a project's source, and the install of that zip. The source is
 * never edited by a build, and the installed content is never touched in place: the game's own installer does the install. */
#ifndef TNFS_TRACK_EDITOR_BUILD_H
#define TNFS_TRACK_EDITOR_BUILD_H

#include "project.h"

#include <stddef.h>

/* Builds <dir>/build/<id>.zip from the project and its trace: the road's nodes (refused while a node breaks a limit), turned into the TRI of
 * track_template_generic (no CD byte), the INFO of a generic piece and the stage's art (project.ini's art=, a FAM), and the card, checked the
 * way the game checks a track piece. 1 with `zip` the path written; 0 with `why` naming the fault otherwise. */
int build_project(const char *dir, const Project *p, const Trace *t, char *zip, size_t zip_cap, char *why, size_t why_cap);

/* Installs `zip` into the player folder `user_dir` the way the game's own installer does (`tnfs --user DIR --install ZIP`), by running the
 * game. 1 when it exits 0; 0 with `why` otherwise. */
int install_zip(const char *game, const char *zip, const char *user_dir, char *why, size_t why_cap);

/* The shell command that does the same install by hand (what Windows prints, where the editor does not run the game itself): `GAME --user DIR
 * --install ZIP`, or `GAME --install ZIP` when `user_dir` is empty (the game then installs into its own player folder). */
void install_command_text(const char *game, const char *zip, const char *user_dir, char *out, size_t cap);

#endif

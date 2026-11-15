/* port (dep59 n4): the mods in effect, one place for every reader. Only Restomod loads mods: in Stock and Restored there are none, and
 * `--mods DIR` is ignored there with a line in the log. In Restomod the folders the game reads mod files from, highest first, are
 * `--mods DIR`, then Restomod's list (`mods=` in its file, mod_list.h; provisional until the content model's profile replaces it,
 * notes/dep59-content-model-design.md) as <player folder>/mods/<id>/, a listed mod with no folder left out. They follow the settings: every write of a settings file (port_settings_forget, on the thread that writes)
 * works them out again, and a change of list or of mode is in effect for the next file read. The overlay (platform_set_mods) and the
 * port's data (data_path_set_mods) are given the same folders. mod_set_generation moves on only when the folders change: what is kept
 * from a mod file keeps the generation it was made under and is made again when it moved (as PortFollow does for a setting). */
#ifndef TNFS_MOD_SET_H
#define TNFS_MOD_SET_H

#include "formats/mod_list.h"

#include <stdbool.h>
#include <stdint.h>

/* the folders in effect from now on, and after every settings write; `cli_dir` (NULL or "": none) is --mods. False when cli_dir is
 * given, the mode is Restomod and it is not a folder (nothing changes then); in Stock and Restored it is not looked at. Called again, it starts over with the new folder. */
bool mod_set_start(const char *data_root, const char *cli_dir);
/* no mods in effect and no following (tests) */
void mod_set_stop(void);

/* never 0; moves on whenever the folders in effect change */
uint32_t mod_set_generation(void);
/* moves the generation on: mod_set.c's when the folders change; a cache's test calls it to stand for a change (mod_generation.c) */
void mod_set_generation_move(void);

/* The folders in effect, highest first, into `dirs` (up to `cap`; the pointers stay good until the folders change, mod_set_generation);
 * how many were put there. 0 in Stock and Restored. */
int mod_set_dirs(const char *dirs[], int cap);

/* port (dep59, temporary until step 5): 1 once --mods has served a file that changes the race (mod_file_rule) in this run: the race keeps
 * no record and no replay. An installed folder never serves one: it serves only what its kind of piece may carry, and says once in each
 * generation what it leaves out. mod_set_start and mod_set_stop start a new run. */
bool mod_set_session_modified(void);
/* what serving such a file does, and a new run (mod_set_session.c: no more than the flag, so what reads it needs nothing else) */
void mod_set_session_mark_modified(void);
void mod_set_session_clear(void);

/* Restomod's list written as `l` (whichever mode is in use); true when its file was written. The folders follow at once. */
bool mod_set_save_list(const ModList *l);

#endif

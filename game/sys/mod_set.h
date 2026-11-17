/* port (dep59 n4): the mods in effect, one place for every reader. Only Restomod loads mods: in Stock and Restored there are none, and
 * `--mods DIR` is ignored there with a line in the log. In Restomod the folders the game reads mod files from, highest first, are
 * `--mods DIR`, then the edition in use's (port (dep59 step 4): edition_file.h; the standard edition has none, whatever is installed):
 * its tweaks in their order, then its added cars, each as <player folder>/mods/<id>/, a piece with no folder left out. They follow the
 * settings: every write of a settings file (port_settings_forget, on the thread that writes) works them out again, and a change of
 * edition or of mode is in effect for the next file read. The overlay (platform_set_mods) and the
 * port's data (data_path_set_mods) are given the same folders. mod_set_generation moves on only when the folders change: what is kept
 * from a mod file keeps the generation it was made under and is made again when it moved (as PortFollow does for a setting). */
#ifndef TNFS_MOD_SET_H
#define TNFS_MOD_SET_H


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

/* 1 once --mods has served a track's places file (track/<TRACK>.txt, which can change the grip) to the live light editor in this run (light_edit=1):
 * the race keeps no record and no replay. mod_set_start and mod_set_stop start a new run. Nothing else in --mods changes a race (it is not served). */
bool mod_set_places_served(void);
/* what serving it does, and a new run (mod_set_places.c: no more than the flag, so what reads it needs nothing else) */
void mod_set_places_mark(void);
void mod_set_places_clear(void);


#endif

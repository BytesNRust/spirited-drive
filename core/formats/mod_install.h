/* dep59 step 2: what installing a mod does, worked out before anything is written (the installer, game/sys/mod_store.c, then does
 * exactly this). Every path is relative to the player's folder:
 *  - a new mod is unpacked into mods/.staging/<id>/ and renamed to mods/<id>/;
 *  - an update (the same id, another or the same version) first moves mods/<id>/ to mods_backup/<id>-<old version>-<date>/;
 *  - a folder mods/<id>/ with no mod.ini was put there by hand, not by the game: it is never replaced;
 *  - the backups of an id are kept to the last MOD_BACKUPS_KEPT.
 * port (dep59 step 4): an install only makes the mod available; the editions say which are in use (edition_ini.h). */
#ifndef TNFS_MOD_INSTALL_H
#define TNFS_MOD_INSTALL_H

#include "formats/mod_check.h"
#include "formats/mod_ini.h"

enum { MOD_BACKUPS_KEPT = 3, MOD_INSTALL_DATE = 15 /* "YYYYMMDD-HHMMSS" */ };

typedef enum { MOD_INSTALL_NEW, MOD_INSTALL_UPDATE, MOD_INSTALL_SAME, MOD_INSTALL_REFUSED } ModInstallKind;

typedef struct {
    ModInstallKind kind;
    char staging[160];   /* "mods/.staging/<id>" */
    char target[160];    /* "mods/<id>" */
    char backup[200];    /* "mods_backup/<id>-<old version>-<date>", "" unless an update (or the same version again) */
    char old_version[MOD_INI_VERSION_MAX + 1];
    char why[160];       /* when refused */
} ModInstallPlan;

/* What installing `check` does, given what is at mods/<id> today: `installed` is its card, or NULL when there is no such folder;
 * `folder_without_card` 1 when the folder exists and has no mod.ini (never replaced). `date` is "YYYYMMDD-HHMMSS". MOD_INSTALL_SAME (the
 * same id and version) is installed again as an update, with its backup. */
void mod_install_plan(const ModCheck *check, const ModInfo *installed, int folder_without_card, const char *date, ModInstallPlan *out);

/* The backups of `id` to remove so that `keep` stay, the oldest first: their indexes in `names` (the folders of mods_backup/), at most
 * `cap`; how many. A folder is a backup of `id` when it is "<id>-<version>-<YYYYMMDD-HHMMSS>"; its date orders it. */
int mod_backups_to_drop(const char *id, const char *const names[], int count, int keep, int drop[], int cap);


#endif

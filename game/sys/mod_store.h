/* port (dep59 step 2): the mods installed in the player's folder. A mod's zip is checked whole (mod_check_zip) before anything is
 * written, then unpacked into mods/.staging/<id>/ and renamed to mods/<id>/; an update first moves the installed folder to
 * mods_backup/<id>-<version>-<date>/, and the last MOD_BACKUPS_KEPT backups of each id are kept (formats/mod_install.h). Uninstalling moves
 * the folder to a backup too: nothing is deleted but backups past the last three, or when asked for. A failure half way leaves nothing in
 * mods/ but what was there before. Nothing is written outside mods/ and mods_backup/, and no symbolic link is followed (platform.h).
 * port (dep59 step 4): installing or restoring a mod only makes it available, and uninstalling changes no edition (one that names it
 * skips it): the editions say which are in use (edition_file.h). */
#ifndef TNFS_MOD_STORE_H
#define TNFS_MOD_STORE_H

#include "formats/mod_ini.h"

#include <stddef.h>

/* Each returns 1, or 0 with the reason for the player in `why`. */
/* installs or updates the mod in the zip at `zip_path`: its id in `id_out`, and the version it replaced in `old_out` ("" when new) */
int mod_store_install(const char *zip_path, char *id_out, size_t id_cap, char *old_out, size_t old_cap, char *why, size_t why_cap);
int mod_store_uninstall(const char *id, char *why, size_t why_cap);
/* the newest backup of `id` back to mods/<id>; an installed one there goes to a backup first */
int mod_store_restore(const char *id, char *why, size_t why_cap);
/* every backup of `id` removed */
int mod_store_drop_backups(const char *id, char *why, size_t why_cap);

typedef struct {
    ModInfo info;
    int active;    /* named by the edition in use (edition_file.h) */
    int backups;   /* how many backups it has */
} ModStoreEntry;
/* the installed mods (folders of mods/ with a card), by id; how many (at most `cap`) */
int mod_store_list(ModStoreEntry *out, int cap);

#endif

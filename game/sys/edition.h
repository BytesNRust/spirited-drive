/* port (dep59 step 4, T10): the edition changed with no restart (notes/dep59-step4-editions-plan.md, section 5). Restomod's edition= is
 * written (port_mode_save), which tells every follower of the settings (port_settings_on_forget): the mod folders are put in effect again
 * and their generation moves on (mod_set.h), and what the menus build from the edition in use (edition_file.h's edition_active: the car
 * list, the track list, the unlocks) is read at its next use, nothing kept from the edition before. */
#ifndef TNFS_EDITION_H
#define TNFS_EDITION_H

/* Restomod's edition set to `id` (NULL or "" the standard edition), said in the log as mark:edition=<id> (- for the standard one): 1 when
 * it changed, 0 when it was that already (nothing written), -1 when `id` can name no edition (edition_id_ok) or the file could not be
 * saved (logged) */
int edition_set(const char *id);

#endif

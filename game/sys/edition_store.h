/* port (dep59 step 4b, T2): the editions the CONTENT screen keeps, <player folder>/editions/<id>.ini (formats/edition_ini.h). Each change is
 * saved at once, whole: written beside the file as <id>.ini.new and put in its place, so a stop half way leaves the old file or the new one,
 * never part of one. After a save the settings are forgotten (port_settings_forget), so the edition in use is read again: nothing is kept
 * from before. An edition's id comes from its name when it is made and never changes after (Rename changes name= alone). Every function
 * returns 1, or 0 with the reason in `why` (English, for the player) and nothing changed. */
#ifndef TNFS_EDITION_STORE_H
#define TNFS_EDITION_STORE_H

#include "formats/edition_ini.h"

#include <stddef.h>

/* the id a new edition named `name` gets: lower case a-z 0-9, any other run of characters one '-', none at either end ("edition" when
 * nothing is left or it is not an edition's id, as "original" is not); -2, -3 ... on it while editions/<id>.ini is there */
void edition_store_new_id(const char *name, char out[EDITION_ID_MAX + 1]);

/* `e` as editions/<id>.ini (an edition's id, edition_id_ok; a good name, edition_name_ok) */
int edition_store_save(const char *id, const Edition *e, char *why, size_t why_cap);

/* a new edition named `name` with the standard set (the eight cars, the eight tracks: no rule broken), its id into `id_out` */
int edition_store_create(const char *name, char id_out[EDITION_ID_MAX + 1], char *why, size_t why_cap);

/* a copy of the edition `from` named `name`, under a new id into `id_out` */
int edition_store_duplicate(const char *from, const char *name, char id_out[EDITION_ID_MAX + 1], char *why, size_t why_cap);

/* the edition `id`'s name= changed to `name`; its id, its file and its lists stay */
int edition_store_rename(const char *id, const char *name, char *why, size_t why_cap);

/* editions/<id>.ini removed; when it was the edition in use, the standard one is (edition_set) */
int edition_store_delete(const char *id, char *why, size_t why_cap);

#endif

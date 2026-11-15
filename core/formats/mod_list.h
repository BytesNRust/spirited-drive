/* The active mods (dep59 n4): `mods=` in a mode's file, the ids of the mods in <player folder>/mods/ the game reads, in their order: the
 * first one listed wins over the ones after it. Ids are separated by commas, with spaces round them dropped. An id is a mod's (mod_ini.h's
 * rule) or "legacy", the folder the loose files of before were moved to. */
#ifndef TNFS_MOD_LIST_H
#define TNFS_MOD_LIST_H

#include "formats/mod_ini.h"

#include <stddef.h>

enum { MOD_LIST_MAX = 32, MOD_LIST_TEXT_MAX = MOD_LIST_MAX * (MOD_INI_ID_MAX + 1) + 1 };

typedef struct {
    int count;
    char ids[MOD_LIST_MAX][MOD_INI_ID_MAX + 1];
} ModList;

/* The list in `text` (NULL or "": none) into `out`. An id that is not usable, one already in the list, or one past MOD_LIST_MAX is left
 * out; returns how many were. */
int mod_list_parse(const char *text, ModList *out);

/* `l` as `mods=` writes it ("a,b", "" for none); 0, with "" in `out`, when it does not fit in `cap`. */
int mod_list_format(const ModList *l, char *out, size_t cap);

/* The place of `id` in `l`, or -1 */
int mod_list_index(const ModList *l, const char *id);

/* 1 when `id` may be in the list: a mod's id, or "legacy" */
int mod_list_id_ok(const char *id);

#endif

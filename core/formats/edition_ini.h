/* port (dep59 step 4): a Restomod edition, <player folder>/editions/<id>.ini (notes/dep59-step4-editions-plan.md, section 1). Lines
 * `key=value` as mod.ini's (spaces round both trimmed, CR LF or LF, blank lines and lines starting with '#'):
 *   name    what Game Mode shows: 1..EDITION_NAME_MAX of printable ASCII (0x20..0x7e), required
 *   cars    the cars in Control Central's order, ids between commas (a stock car's, formats/stock_content.h, or an added car's mod id)
 *   tracks  the tracks in their order, the same way
 *   tweaks  the tweaks' mod ids, the first one listed winning over the ones after it
 * An id is a mod's (mod_ini_id_ok) and not "original", once per list; empty items are nothing. A piece that is not installed is an id
 * like any other: the file keeps it, and whoever loads the edition skips it. A key the game does not know is left for a later version;
 * a key twice is refused. The standard edition has no file. */
#ifndef TNFS_EDITION_INI_H
#define TNFS_EDITION_INI_H

#include "formats/mod_ini.h"

#include <stddef.h>
#include <stdint.h>

enum {
    EDITION_MAX_SIZE = 8192,
    EDITION_ID_MAX = MOD_INI_ID_MAX,
    EDITION_NAME_MAX = 12,
    EDITION_LIST_MAX = 64,     /* cars: as many as can be added (CAR_ADDED_MAX) */
    EDITION_TRACKS_MAX = 32,
    EDITION_TWEAKS_MAX = 32
};

typedef struct {
    char name[EDITION_NAME_MAX + 1];
    int cars, tracks, tweaks;
    char car[EDITION_LIST_MAX][MOD_INI_ID_MAX + 1];
    char track[EDITION_TRACKS_MAX][MOD_INI_ID_MAX + 1];
    char tweak[EDITION_TWEAKS_MAX][MOD_INI_ID_MAX + 1];
} Edition;

/* 1 and `out` filled when the file is good; 0, `out` cleared, with the reason in `why` (English, for the player) otherwise */
int edition_parse(const uint8_t *text, size_t size, Edition *out, char *why, size_t why_cap);

/* `e` as its file: name, then cars, tracks and tweaks (a list with nothing in it has no line). The length written; 0, with "" in `out`,
 * when it does not fit in `cap` */
int edition_format(const Edition *e, char *out, size_t cap);

/* 1 when `id` may name an edition or one of its pieces: a mod's id (mod_ini_id_ok), and not "original", which is kept for the game's own */
int edition_id_ok(const char *id);

/* 1 when `name` may be an edition's: 1..EDITION_NAME_MAX of printable ASCII, and not "Customize game" (Game Mode draws that label from its
 * own art), in any case; 0 with the reason in `why` otherwise */
int edition_name_ok(const char *name, char *why, size_t why_cap);

#endif

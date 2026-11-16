/* port (dep59 step 4b, T4): the CONTENT screen's lists (notes/dep59-step4b-content-screen-plan.md; the design's "Pantalla CONTENT" and
 * "Reglas"), with no drawing and no files: the rows of a tab (Cars | Tracks | Tweaks) for the edition being edited, the All row's three
 * states, and the edits a row's box, the All row and the up / down arrows make to the edition.
 *   rows   an own edition's: first the pieces it names, in its order and switched on (a stock one "(game)", an installed mod with its
 *          version, an id that is neither "missing"); then the pieces it does not name, switched off: the stock ones in their order, the
 *          installed mods of the tab's type, and the mods uninstalled that still have a backup ("(removed)": no box to switch, only their
 *          Previous version). The standard edition's (NULL): the stock pieces switched on, the mods off, every row locked and no arrows.
 *          Tracks are the nine stock ones until step 5 (the hidden track is never listed); the Tweaks tab has no stock row.
 *   edits  each makes a new edition from the one given (the one given is left as it was): a row switched on goes to the end of its list,
 *          one switched off leaves it; the arrows move a row that is on one place within its list. None is made in the standard edition
 *          (`in` NULL: 0, `out` not written).
 *   rule   an edition needs at least one car and one track it can race (stock or installed: a missing one does not count); the screen does
 *          not let the player leave without them. */
#ifndef TNFS_CONTENT_ROWS_H
#define TNFS_CONTENT_ROWS_H

#include "formats/edition_ini.h"
#include "formats/mod_ini.h"

enum { CONTENT_CARS, CONTENT_TRACKS, CONTENT_TWEAKS, CONTENT_TABS };
/* what a row is */
enum { CONTENT_ROW_GAME, CONTENT_ROW_MOD, CONTENT_ROW_MISSING, CONTENT_ROW_REMOVED };
/* the All row */
enum { CONTENT_ALL_OFF, CONTENT_ALL_ON, CONTENT_ALL_MIXED };
/* what content_rules_ok finds missing */
enum { CONTENT_RULES_OK = 0, CONTENT_RULES_NO_CAR = 1, CONTENT_RULES_NO_TRACK = 2 };
enum { CONTENT_ROWS_MAX = 160 };

typedef struct {
    char id[MOD_INI_ID_MAX + 1];
    char name[MOD_INI_NAME_MAX + 1];         /* the stock piece's English name, the mod's name; a missing one's id */
    char version[MOD_INI_VERSION_MAX + 1];   /* an installed or removed mod's; "" for a stock piece ("(game)") and a missing one */
    int kind;                                /* CONTENT_ROW_* */
    int on;                                  /* named by the edition (the box ticked) */
    int switchable;                          /* the box can change: not locked, not removed */
    int movable;                             /* the arrows can move it: on, and not locked */
    int backup;                              /* the mod has a backup: its Previous version is offered */
} ContentRow;

typedef struct {
    ContentRow row[CONTENT_ROWS_MAX];
    int count;
    int locked;   /* the standard edition's: nothing changes */
} ContentRows;

/* the mods the rows are built from: the installed ones (mod_store_list's), and the uninstalled ones that still have a backup (their last
 * backup's card); `backups` gives, for each installed mod, whether it has a backup (NULL: none has) */
typedef struct {
    const ModInfo *installed;
    const int *backups;
    int installed_count;
    const ModInfo *removed;
    int removed_count;
} ContentMods;

/* the rows of `tab` for the edition `e` (NULL: the standard edition) */
void content_rows_build(ContentRows *out, const Edition *e, int tab, const ContentMods *mods);

/* the All row's state: CONTENT_ALL_ON when every switchable row is on, CONTENT_ALL_OFF when none is (or there is none), else MIXED */
int content_rows_all(const ContentRows *rows);

/* the edition with row `index` of `rows` (built from `in` for `tab`) switched; 1 and `out` made, 0 (`out` as `in`) when the row cannot be */
int content_rows_switch(const Edition *in, Edition *out, int tab, const ContentRows *rows, int index);
/* the All row taken: every switchable row on, or off when all were on; 1 and `out` made, 0 (`out` as `in`) when nothing changes */
int content_rows_switch_all(const Edition *in, Edition *out, int tab, const ContentRows *rows);
/* row `index` moved one place up (`step` -1) or down (1) among the rows that are on; 1 and `out` made, 0 (`out` as `in`) when it cannot */
int content_rows_move(const Edition *in, Edition *out, int tab, const ContentRows *rows, int index, int step);

/* CONTENT_RULES_OK, or the CONTENT_RULES_* bits of what `e` lacks (NULL, the standard edition, lacks nothing) */
int content_rules_ok(const Edition *e, const ContentMods *mods);

#endif

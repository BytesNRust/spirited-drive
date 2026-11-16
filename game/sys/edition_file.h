/* port (dep59 step 4): the edition in use (notes/dep59-step4-editions-plan.md, section 1). Restomod's `edition=<id>` (port_settings.h)
 * names <player folder>/editions/<id>.ini (formats/edition_ini.h); no key is the standard edition, today's game. An edition whose file is
 * missing or damaged, or that keeps no car or no track once the pieces that are not there are left out, is not used: the standard one
 * is, with a line in the log, and neither the key nor the file is changed. Stock and Restored have no edition. */
#ifndef TNFS_EDITION_FILE_H
#define TNFS_EDITION_FILE_H

#include "formats/edition_ini.h"

#include <stddef.h>

/* 0 the standard edition; 1 the edition in use, into `out`; -1 an edition= that cannot be used (logged once per id and reason): the
 * standard edition is used */
int edition_active(Edition *out);

/* editions/<id>.ini into `out`: 1; 0 with the reason in `why` (its file missing, or edition_parse's) */
int edition_load(const char *id, Edition *out, char *why, size_t why_cap);

/* 1 when `e` keeps a car and a track that are there (edition_car_present, edition_track_present); 0 with the reason in `why` */
int edition_usable(const Edition *e, char *why, size_t why_cap);

/* 1 when the car `id` is there: a stock car's id (formats/stock_content.h), or an installed mod whose card says it is a car */
int edition_car_present(const char *id);

/* 1 when the track `id` is there: a stock track's id that can be listed (not hidden, which has no geometry on the CD) */
int edition_track_present(const char *id);

/* the ids of the editions in editions/ (the files <id>.ini whose name is an id), in order, at most `cap`; how many */
int edition_list(char ids[][EDITION_ID_MAX + 1], int cap);

#endif

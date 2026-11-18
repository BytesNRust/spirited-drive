/* port (dep59 showcase, owner 2026-11-18): what the two hosts of the RECORDS screen (an added car's page, the CD pages) share: the ground the table
 * is drawn over and the table's own loop (pages by the arrows or the pager's halves, closed by DONE, Enter, Escape or the back button). */
#ifndef TNFS_CAR_RECORDS_RUN_H
#define TNFS_CAR_RECORDS_RUN_H

#include "game/frontend/car_records_view.h"

#include <stdint.h>

/* every track the player has (central_glue.c): the edition's, else the standard list */
void cg_records_tracks(TrackList *l);

/* `out` (the page's picture, 640 x 480 indices on `palette`) with its rows from CAR_RECORDS_TOP down made dark: the clean configuration
 * background (`tint` of the mod's first paint when not NULL), each pixel the nearest dark colour among the `usable` ones of `palette` (8 bits a
 * channel). The rows above (the title band) are left as they are */
void car_records_ground(uint8_t *out, const uint8_t palette[768], const uint8_t usable[256], const uint8_t tint[3]);

/* what a host draws and shows for one frame of the table: the ground, the table over it, its palette, the flip */
typedef void (*CarRecordsShow)(void *ctx, const CarRecordsView *v);

/* the screen's loop until it closes; `show` is called when the page changes (and once at the start). The press that closed it is let go before
 * this returns */
void car_records_run(CarRecordsView *v, CarRecordsShow show, void *ctx);

#endif

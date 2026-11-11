/* the baked light as the cars take it (view_light.c); ported_view.c calls these */
#ifndef TNFS_VIEW_LIGHT_H
#define TNFS_VIEW_LIGHT_H

#include "game/race/headlight_pool.h"

#include <stdint.h>

extern int light_changed;   /* the light editor's reload put new rows in the same light: the built track is built again */
extern HeadlightPool *pool_fed;   /* the pool headlights_update last fed */
void road_tunnels_forget(void);
void headlights_update(void);   /* the cars with their lights on, each frame */

#endif

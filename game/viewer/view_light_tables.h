/* the light's tables made again when the palette or tint moved (view_light_tables.c); ported_view.c calls these */
#ifndef TNFS_VIEW_LIGHT_TABLES_H
#define TNFS_VIEW_LIGHT_TABLES_H

#include "viewer/view_shared.h"

#include <stdint.h>

void light_tables_prepare(const TrackArt *art);
#endif

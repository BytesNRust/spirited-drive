/* Export original car as a kit (dep100 item 6): a car of the game made into a kit in the current layout (formats/car_kit_paths.h) instead of a project: the car's
 * CFM (and PDN and PBS, for the eight cars you drive) laid out by `car kit`, the game's masks flat in art-generated/. A traffic car or the police car, which have no PBS
 * and no card, get a kit with a minimal card (type=car, the stem, redistributable=0). The masks are the game's bytes; the PSDs are not here (tools/migrate_originals.py
 * copies them into art-source/masks). Needs tnfs_tools. */
#ifndef TNFS_TRACK_EDITOR_CAR_EXPORT_KIT_H
#define TNFS_TRACK_EDITOR_CAR_EXPORT_KIT_H

#include "car_export.h"

#include <stddef.h>

/* Makes the kit of car `index` in `kit`, a folder that is not there yet: made beside as "<kit>.exporting" and renamed when whole. 1 when made; 0 with `why`. */
int car_export_kit(const ExportSource *src, int index, const char *kit, char *why, size_t why_cap);

#endif

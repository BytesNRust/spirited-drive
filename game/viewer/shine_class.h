/* port (body shine): which texels of a car's body textures are paint (shine_draw.h's class maps), found by the address of the texture's
 * texels in the car's file: the sink gets a polygon's FshEntry, whose payload is those texels. Made when the car is loaded, from the
 * draft of the repaint (drive_livery_draft) before any colour changes, and kept until the car files are freed. */
#ifndef TNFS_SHINE_CLASS_H
#define TNFS_SHINE_CLASS_H

#include "viewer/shine_draw.h"

#include <stdint.h>

void shine_class_clear(void);
void shine_class_drop_slot(int slot);
/* registers the texture whose texels are at `payload` (width x height) for the car in `slot`; `keep` (width x height of REPAINT_DRAFT_*) is copied */
void shine_class_add(int slot, const uint8_t *payload, int width, int height, const uint8_t *keep);
/* the class map of the texture at `payload`, or NULL */
const ShineClass *shine_class_find(const uint8_t *payload);

#endif

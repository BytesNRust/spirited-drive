/* port (portal_both): a tunnel's portal is a 3D model placed at the node before its first tunnel node, facing out of that mouth; the other mouth has
 * none of its own (the CD's builders put a portal where the road comes in), so a race that is driven or seen from the other way (a reversed circuit, the 360
 * camera) looks at a hole or at the portal's back. portal_faces_add puts the portal's second face on the other mouth: the same model turned half a turn about
 * its centre, so it faces out of that mouth and frames the same lane. Boards (billboards) are not copied, and a mouth that already has a record of the model
 * is left alone, so a second call adds nothing. */
#ifndef TNFS_PORTAL_FACES_H
#define TNFS_PORTAL_FACES_H

#include "formats/tri_reverse.h"

#include <stddef.h>
#include <stdint.h>

/* The TRI file in memory (`size` bytes, written in place: the records are inserted in their node order, there must be room in the 1000 slots); `pivots` as
 * tri_reverse_with has them (the centre of each model, NULL for none). Returns the records added. */
uint32_t portal_faces_add(uint8_t *file, size_t size, const TriPivot *pivots, uint32_t pivot_count);

#endif

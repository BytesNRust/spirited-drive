/* port (lamp_halos): the street lamps' halo at night. The lamp boards' own sprites are noted as they are drawn (their screen rectangles); after the 3D, the lamp entries' blobs inside one get a
 * halo, in the colour of the blob's entry. A traffic sign the reflectors light is painted in the same entries and is no lamp: it lies in no rectangle, and gets none. */
#ifndef TNFS_STREET_HALO_H
#define TNFS_STREET_HALO_H

#include "core/formats/fsh.h"
#include "game/gfx/quad.h"
#include "game/race/view.h"

#include <stdint.h>

/* The track's lamp boards (their bitmaps, `count` of at most 3), the three emissive entries of the lamp and the window entry (never a member). Called whenever any of them changes. */
void street_halo_boards(const FshEntry *const bitmaps[3], int count, const uint8_t entry[3], uint8_t window_entry);

/* A new frame: forget the rectangles. */
void street_halo_frame_begin(void);

/* A polygon drawn with `texture`: when it is a lamp board, its screen rectangle is noted. The exe's mappers give the polygon's four screen corners. */
void street_halo_note(const FshEntry *texture, const int32_t screen[8]);

/* Modern 3D: the corners in camera space (a board the near plane clips takes the whole view). */
void street_halo_note_cam(const FshEntry *texture, const View *view, const int32_t *const cam[4]);

/* After the 3D: the halos. `night` is whether the track is lit as at night. */
void street_halo_draw(const QuadTarget *t, const uint8_t palette[768], int night);

#endif

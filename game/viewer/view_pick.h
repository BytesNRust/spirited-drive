/* port (light_edit): the light editor's aim. While it is on, each polygon of the track drawn says whether it covers the aim point (the mouse pointer; the 3D view's centre until there is one); the last one that does is
 * the nearest (the painter's order), and the editor shows its texture as the mask file that names it (slot_NN, scenery_NN), so the owner can mark a lamp he sees. */
#ifndef TNFS_VIEW_PICK_H
#define TNFS_VIEW_PICK_H

#include "formats/fsh.h"
#include "game/race/track_art.h"
#include "game/race/view.h"

#include <stddef.h>
#include <stdint.h>

typedef enum { VIEW_PICK_NONE, VIEW_PICK_SLOT, VIEW_PICK_SCENERY, VIEW_PICK_OTHER } ViewPickKind;

void view_pick_set(int on);   /* the editor on or off; off notes nothing */

/* the mouse pointer at (x, y) on the shown frame of width x height (the frame the editor draws on): the next frame's aim. A size of 0: none (the view's centre) */
void view_pick_pointer(int x, int y, int width, int height);

/* a frame begins: the aim is the pointer (view_pick_pointer, mapped to this target) or else `view`'s centre, the textures are `art`'s (kept: it must last the frame), the 3D is drawn on a width x height target; nothing found yet */
void view_pick_frame_start(const View *view, const TrackArt *art, int width, int height);

void view_pick_note(const FshEntry *texture, const int32_t screen[8]);   /* a quad drawn at these screen corners */
void view_pick_note_cam(const FshEntry *texture, const View *view, const int32_t *const cam[4]);   /* one drawn from camera space (Modern 3D): none with a corner behind the near plane */

/* the aim point on the 3D target (and its size in `width`, `height` when not NULL); 0 when off */
int view_pick_aim_on(int *x, int *y, int *width, int *height);
static inline int view_pick_aim(int *x, int *y) { return view_pick_aim_on(x, y, NULL, NULL); }

/* what the aim is on: a road slot (`slot`, `variant`), a scenery board (`slot` its number), another texture, or nothing */
ViewPickKind view_pick_found(int *slot, int *variant);

#endif

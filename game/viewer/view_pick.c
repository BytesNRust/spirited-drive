/* port (light_edit): see view_pick.h. */
#include "game/viewer/view_pick.h"


static int pick_on;
static const TrackArt *pick_art;
static int aim_x, aim_y, target_w, target_h;
static const FshEntry *found;   /* the last texture drawn over the aim this frame, NULL none */

void view_pick_set(int on) { pick_on = on != 0; }

void view_pick_frame_start(const View *view, const TrackArt *art, int width, int height)
{
    found = NULL;
    pick_art = art;
    target_w = width, target_h = height;
    if (view != NULL) aim_x = view->centre_x, aim_y = view->centre_y;
}

/* (x, y) inside the quad (convex, either winding; on an edge counts) */
static int covers(const double sx[4], const double sy[4], double x, double y)
{
    int pos = 0, neg = 0;
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) & 3;
        const double cross = (sx[j] - sx[i]) * (y - sy[i]) - (sy[j] - sy[i]) * (x - sx[i]);
        pos |= cross > 0, neg |= cross < 0;
    }
    return !(pos && neg);
}

static void note_at(const FshEntry *texture, const double sx[4], const double sy[4])
{
    if (covers(sx, sy, aim_x + 0.5, aim_y + 0.5)) found = texture;
}

void view_pick_note(const FshEntry *texture, const int32_t screen[8])
{
    if (!pick_on || texture == NULL) return;
    double sx[4], sy[4];
    for (int i = 0; i < 4; i++) sx[i] = screen[2 * i], sy[i] = screen[2 * i + 1];
    note_at(texture, sx, sy);
}

void view_pick_note_cam(const FshEntry *texture, const View *view, const int32_t *const cam[4])
{
    if (!pick_on || texture == NULL || view == NULL) return;
    double sx[4], sy[4];
    for (int i = 0; i < 4; i++) {
        const double z = cam[i][2];
        if (!(z >= view->near_z) || !(z > 0)) return;
        sx[i] = view->centre_x + (double)cam[i][0] * view->focal_x / z;
        sy[i] = view->centre_y - (double)cam[i][1] * view->focal_x / z;
    }
    note_at(texture, sx, sy);
}

int view_pick_aim_on(int *x, int *y, int *width, int *height)
{
    if (!pick_on) return 0;
    if (x != NULL) *x = aim_x;
    if (y != NULL) *y = aim_y;
    if (width != NULL) *width = target_w;
    if (height != NULL) *height = target_h;
    return 1;
}

ViewPickKind view_pick_found(int *slot, int *variant)
{
    if (!pick_on || found == NULL) return VIEW_PICK_NONE;
    for (int s = 0; pick_art != NULL && s < TRACK_TEXTURE_SLOTS; s++)
        for (int v = 0; v < TRACK_TEXTURE_VARIANTS; v++)
            if (pick_art->slots[s][v].present && &pick_art->slots[s][v].bitmap == found) {
                if (slot != NULL) *slot = s;
                if (variant != NULL) *variant = v;
                return VIEW_PICK_SLOT;
            }
    for (int s = 0; pick_art != NULL && s < TRACK_SCENERY_TEXTURES; s++)
        if (pick_art->scenery[s].present && &pick_art->scenery[s].bitmap == found) {
            if (slot != NULL) *slot = s;
            if (variant != NULL) *variant = 0;
            return VIEW_PICK_SCENERY;
        }
    return VIEW_PICK_OTHER;
}

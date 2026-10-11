/* The race's views (race_view.h). */
#include "game/race/race_view.h"

void race_view_layout(RaceCamera *camera, int32_t layout, const ViewLayouts *layouts, int32_t mode, View *view,
                      HorizonTables *horizon)
{
    camera->layout = layout;
    /* port guard: the original indexes both unchecked */
    if (mode < 0 || mode >= VIEW_SCREEN_MODES || layout < 0 || layout >= VIEW_LAYOUTS) return;
    const ViewLayout *row = &(*layouts)[mode][layout];
    camera->layout_row = row;
    horizon_tables(horizon, row->width, camera->fov, row->scale);
    view_init(view, row->left + row->x, row->top + row->y, row->width, row->height, camera->fov, camera->fov);
}

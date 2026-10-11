/* The race's views (race_view.h). */
#include "game/race/race_view.h"

void race_view_layout(RaceViews *views, RaceCamera *camera, int32_t layout)
{
    camera->layout = layout;
    /* port guard: the original indexes both unchecked */
    if (views->mode < 0 || views->mode >= VIEW_SCREEN_MODES || layout < 0 || layout >= VIEW_LAYOUTS) return;
    const ViewLayout *row = &views->layouts[views->mode][layout];
    camera->layout_row = row;
    horizon_tables(&views->horizon, row->width, camera->fov, row->scale);
    view_init(&views->view, row->left + row->x, row->top + row->y, row->width, row->height, camera->fov, camera->fov);
}

void race_view_select(RaceViews *views, RaceCamera *camera, int32_t view, int32_t size)
{
    camera->view = view;
    /* port guard: the original indexes all three unchecked */
    if (views->mode >= 0 && views->mode < VIEW_SCREEN_MODES && view >= 0 && view < VIEW_ROWS && size >= 0 &&
        size < VIEW_SIZES) {
        const ViewRow *row = &views->rows[views->mode][view];
        camera->view_row = row;
        race_view_layout(views, camera, row->layouts[size]);
    }
    views->changed = 1;
}

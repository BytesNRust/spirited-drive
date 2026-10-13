/* The race's views (race_view.h). */
#include "game/race/race_view.h"

#include "game/gfx/x86_int.h"

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

void race_camera_view(View *view, const RaceCameraPair *pair, int rear, const int32_t shake[2], int32_t screen_shift,
                      const RaceCamera **camera, const int32_t **matrix)
{
    const RaceCamera *c = rear ? pair->rear : pair->front;
    *camera = c;
    *matrix = rear ? pair->rear_matrix : pair->front_matrix;
    const ViewLayout *l = c->layout_row;
    view_init(view, x_add(l->x, l->left), x_add(l->y, l->top), l->width, l->height, c->fov, c->fov);
    if (!rear) view->centre_x = x_add(view->centre_x, shake[0]), view->centre_y = x_add(view->centre_y, shake[1]);
    view->centre_x = x_shl(view->centre_x, screen_shift & 31), view->centre_y = x_shl(view->centre_y, screen_shift & 31);
}

void race_view_reselect(RaceViews *views, RaceCamera *camera, int32_t size)
{
    if (camera->view >= 0) race_view_select(views, camera, camera->view, size);
}

void race_view_size(RaceViews *views, RaceCamera *camera, int32_t *window_size, int32_t size)
{
    if (size > VIEW_SIZE_LARGEST) size = VIEW_SIZE_LARGEST;
    if (size == *window_size) return;
    *window_size = size;
    /* port guard: the callers pass a byte; the original indexes the row unchecked */
    if (camera->view_row == NULL || size < 0) return;
    race_view_layout(views, camera, camera->view_row->layouts[size]);
}

void race_view_next(RaceViews *views, RaceCamera *camera, int32_t size)
{
    /* port guard: the original indexes both unchecked */
    if (views->mode < 0 || views->mode >= VIEW_SCREEN_MODES || camera->view < 0 || camera->view >= VIEW_ROWS) return;
    const int32_t next = views->rows[views->mode][camera->view].next;
    if (next > -1) race_view_select(views, camera, next, size);
}

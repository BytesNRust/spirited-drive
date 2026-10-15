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

int32_t race_view_choose(RaceViews *views, RaceCamera *camera, int32_t view, int32_t size)
{
    if (view <= -1) return 0;
    race_view_select(views, camera, view, size);
    return 1;
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

void race_view_window(const RaceCamera *second, int32_t window_size, int32_t *span, int32_t *size)
{
    *size = window_size;
    *span = (int32_t)((uint32_t)second->view - (uint32_t)second->unknown_08);
}

void race_view_screen(const RaceViewScreenOps *ops, RaceViews *views, RaceCamera *first, RaceCamera *second,
                      int32_t step, int32_t size, int32_t *window_size)
{
    if (size > VIEW_SIZE_LARGEST) size = VIEW_SIZE_LARGEST;
    first->view = first->unknown_08;
    *window_size = size;
    first->view_row = &views->rows[views->mode][first->view];
    ops->layout(ops->ctx, first, first->view_row->layouts[size]);
    second->view = x_add(second->unknown_08, step);
    second->view_row = &views->rows[views->mode][second->view];
    views->changed = 1;
    ops->layout(ops->ctx, second, second->view_row->layouts[*window_size]);
    views->changed = 1;
}

enum { DASH_BASE = 0x6c, DASH_LOW_DIV = 200, DASH_HIGH_ROWS = 480, VIEW_SIZE_FIRST = 0 };

static void start_camera(const RaceViewStartOps *ops, RaceViews *views, RaceCamera *cam, int32_t view, int changed)
{
    cam->view = view;
    const ViewRow *row = &views->rows[views->mode][view];
    if (changed) views->changed = 1;
    cam->view_row = row;
    ops->layout(ops->ctx, cam, row->layouts[VIEW_SIZE_FIRST]);
}

static void start_viewport(const RaceViewStartOps *ops, const RaceCamera *cam)
{
    const ViewLayout *l = cam->layout_row;
    ops->viewport(ops->ctx, x_add(l->left, l->x), x_add(l->top, l->y), l->width, l->height, cam->fov);
    ops->reciprocals(ops->ctx);
}

/* 004055d0 (watcall model) */
static void model_heights(RaceViews *views, const int32_t (*model_y)[VIEW_MODELS][3], int32_t model)
{
    const int32_t mode = views->mode;
    const int32_t v = model_y[mode][model][0];
    views->layouts[mode][0].unknown_24 = v;
    views->layouts[mode][1].unknown_24 = v;
    views->layouts[mode][2].unknown_24 = v / 2;
}

static void drop_layouts(RaceViews *views, int mode, int32_t drop, int32_t divide)
{
    for (int k = 0; k < VIEW_SIZES; k++) {
        ViewLayout *l = &views->layouts[mode][views->rows[mode][0].layouts[k]];
        l->y = (int32_t)((uint32_t)l->height * (uint32_t)drop) / divide;
    }
}

void race_view_start(const RaceViewStartOps *ops, RaceViews *views, RaceCamera cams[VIEW_START_CAMERAS],
                     const RaceViewStartInputs *in, RaceViewStartOut *out)
{
    start_camera(ops, views, &cams[0], cams[0].unknown_08, 0);
    start_camera(ops, views, &cams[1], x_add(cams[1].unknown_08, in->step), 1);
    start_camera(ops, views, &cams[2], cams[2].unknown_08, 1);
    views->changed = 1;
    start_viewport(ops, &cams[0]);
    out->recip = ops->recip_table(ops->ctx);
    start_viewport(ops, &cams[2]);
    cams[0].unknown_14 = out->recip;
    cams[1].unknown_14 = out->recip;
    cams[2].unknown_14 = VIEW_RECIP_TABLE;
    out->eye[0] = in->eye_height;
    out->eye[1] = in->eye_height;
    model_heights(views, in->model_y, in->model);
    const int32_t drop = x_sub(in->dash, DASH_BASE);
    drop_layouts(views, 0, drop, DASH_LOW_DIV);
    drop_layouts(views, 1, (int32_t)((uint32_t)drop * (uint32_t)DASH_HIGH_ROWS) / DASH_LOW_DIV, DASH_HIGH_ROWS);
}

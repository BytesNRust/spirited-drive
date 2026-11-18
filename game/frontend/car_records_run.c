/* See car_records_run.h. */
#include "game/frontend/car_records_run.h"

#include "formats/showcase_tint.h"
#include "game/frontend/added_gallery.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/showcase.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/video.h"

#include <stdlib.h>
#include <string.h>

enum { DARK_PERCENT = 22, LET_GO_FRAMES = 30 };

/* the configuration screens' clean background as RGB; a flat brown when it cannot be composed */
static void background_rgb(uint8_t *rgb)
{
    static const uint8_t FLAT[3] = { 58, 44, 30 };
    uint8_t *pixels = malloc((size_t)GL_W * GL_H), palette[768];
    const int ok = pixels != NULL && config_bgnd_load(resource_load_art, CONFIG_BGND_OPTIONS, 0, pixels, palette);
    if (!ok) port_log("car records: no clean background: a flat one\n");
    for (int i = 0; i < GL_W * GL_H; i++) memcpy(rgb + 3 * i, ok ? palette + 3 * pixels[i] : FLAT, 3);
    free(pixels);
}

void car_records_ground(uint8_t *out, const uint8_t palette[768], const uint8_t usable[256], const uint8_t tint[3])
{
    uint8_t *bgnd = malloc((size_t)GL_W * GL_H * 3);
    if (bgnd == NULL) return;
    background_rgb(bgnd);
    if (tint != NULL) showcase_tint_apply(bgnd, (size_t)GL_W * GL_H, tint);
    int last = -1, last_index = 0;   /* a run of one colour is looked up once */
    for (int y = CAR_RECORDS_TOP; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            const uint8_t *c = bgnd + 3 * ((size_t)y * GL_W + x);
            const int key = c[0] << 16 | c[1] << 8 | c[2];
            if (key != last) last = key, last_index = palette_nearest(palette, usable, c[0] * DARK_PERCENT / 100, c[1] * DARK_PERCENT / 100, c[2] * DARK_PERCENT / 100);
            out[y * GL_W + x] = (uint8_t)last_index;
        }
    free(bgnd);
}

void car_records_run(CarRecordsView *v, CarRecordsShow show, void *ctx)
{
    port_log("mark:screen=car_records\n");   /* port: the headless walk waits for it */
    int last_key = input_get_key(), last_buttons = pointer_read_buttons(), done = 0, changed = 1;
    while (!done) {
        if (changed) {
            show(ctx, v);
            port_log("mark:records_page=%d\n", v->page);
            changed = 0;
        }
        const int key = input_get_key(), buttons = pointer_read_buttons();
        const int press = key != last_key && key != 0 ? key : 0;
        if (buttons != last_buttons && (buttons & POINTER_BUTTON_SELECT)) {
            int x, y;
            pointer_position(&x, &y);
            const int step = car_records_view_hit(v, x, y);
            if (step != 0) car_records_view_step(v, step), changed = 1;
            else if (x >= SHOWCASE_DONE_LEFT && x <= SHOWCASE_DONE_RIGHT && y >= SHOWCASE_BOTTOM_TOP) done = 1;
        } else if (buttons != last_buttons && (buttons & POINTER_BUTTON_BACK)) done = 1;
        last_key = key, last_buttons = buttons;
        if (press == GALLERY_KEY_ESCAPE || press == GALLERY_KEY_ENTER) done = 1;
        else if (press == GALLERY_KEY_LEFT) car_records_view_step(v, -1), changed = 1;
        else if (press == GALLERY_KEY_RIGHT) car_records_view_step(v, 1), changed = 1;
        menu_audio_music_service();
        video_wait_vblank();
    }
    for (int i = 0; i < LET_GO_FRAMES; i++) {   /* the press that closed it let go, so the page does not act on it */
        if (input_get_key() == 0 && (pointer_read_buttons() & POINTER_BUTTON_SELECT) == 0) break;
        menu_audio_music_service();
        video_wait_vblank();
    }
}

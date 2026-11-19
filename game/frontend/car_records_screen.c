/* See car_records_screen.h. */
#include "game/frontend/car_records_screen.h"

#include "formats/showcase_tint.h"
#include "game/frontend/added_gallery.h"
#include "game/frontend/car_records_view.h"
#include "game/frontend/central_glue_private.h"
#include "game/frontend/central_split.h"
#include "game/frontend/page_arrows.h"
#include "game/frontend/records_glue.h"
#include "game/frontend/config.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/menu_audio.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdlib.h>
#include <string.h>

enum { DONE_RIGHT = 0x82, SOUND_PAGE = 0x47, SOUND_CHOSEN = 0x48, LET_GO_FRAMES = 30,
       CREAM_RGB = 0xe8d3b7, YELLOW_RGB = 0xffce06, RED_RGB = 0xd01010, BLACK_RGB = 0x000000 };

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
    for (int y = CAR_RECORDS_BAND; y < GL_H; y++)
        for (int x = 0; x < GL_W; x++) {
            const uint8_t *c = bgnd + 3 * ((size_t)y * GL_W + x);
            const int key = c[0] << 16 | c[1] << 8 | c[2];
            if (key != last) last = key, last_index = palette_nearest(palette, usable, c[0], c[1], c[2]);
            out[y * GL_W + x] = (uint8_t)last_index;
        }
    free(bgnd);
}

static int nearest(const uint8_t palette[768], int rgb)
{
    uint8_t usable[256];
    memset(usable, 1, sizeof usable);
    return palette_nearest(palette, usable, rgb >> 16, rgb >> 8 & 0xff, rgb & 0xff);
}

/* the page indicator as the Graphics, Controls and EXTRAS pages draw it (page_arrows.h): the count and the original's triangles, centred */
static void nav_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    text_select(style == 0 ? TEXT_CREAM : TEXT_YELLOW);
    text_draw_smooth(text, x, y, 1, 1);
}
static int32_t nav_width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static int32_t nav_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1) { (void)ctx; return pointer_in_rect(x0, x1, y0, y1); }
static PageNavOps nav_ops(void)
{
    const PageNavOps n = { NULL, nav_text, nav_width, nav_in_rect, central_page_arrow_size, central_page_arrow };
    return n;
}

/* the page's mean colour behind the column heads (the ground's pixels, already tinted), as the rule of car_records_reddish asks */
static int heads_on_red(const CarRecordsLook *look)
{
    long sum[3] = { 0, 0, 0 };
    int n = 0;
    for (int y = CAR_RECORDS_HEAD_Y; y < CAR_RECORDS_HEAD_Y + CAR_RECORDS_HEAD_ROWS; y++)
        for (int x = 0; x < GL_W; x++, n++)
            for (int c = 0; c < 3; c++) sum[c] += look->palette[3 * look->ground[y * GL_W + x] + c];
    const int r = (int)(sum[0] / n), g = (int)(sum[1] / n), b = (int)(sum[2] / n);
    port_log("car records: the page behind the heads is %d,%d,%d (%s)\n", r, g, b, car_records_reddish(r, g, b) ? "reddish: shadowed heads" : "flat heads");
    return car_records_reddish(r, g, b);
}

/* one frame of the screen into the back buffer */
static void screen_frame(const CarRecordsLook *look, const CarRecordsView *v, const CarRecordsInk *ink)
{
    int w, h;
    ea_select_default();
    uint8_t *const to = video_target(&w, &h);
    for (int y = 0; y < GL_H && y < h; y++) memcpy(&to[y * w], &look->ground[y * GL_W], (size_t)(w < GL_W ? w : GL_W));
    if (look->title != NULL) look->title(look->ctx);
    car_records_view_draw(v, resource_kmh(), ink);
    car_records_view_footer(v, ink);
    const PageNavOps nav = nav_ops();
    page_nav_draw(&nav, v->page, car_records_view_pages(v), page_nav_lit(&nav, v->page, car_records_view_pages(v)));
}

static void screen_show(const uint8_t *palette)
{
    video_set_palette(0, 256, palette);
    video_flip();
    pointer_show();
}

void car_records_screen(const char *id, const CarRecordsLook *look)
{
    CarRecordsView *v = malloc(sizeof *v);
    TrackList l;
    if (v == NULL) return;
    records_glue_load(config_current()->image);   /* the stores load with the first race or the first Best Times: this screen can be the first */
    cg_records_tracks(&l);
    car_records_view_make(v, id, &l);
    CarRecordsInk ink = { nearest(look->palette, CREAM_RGB), nearest(look->palette, YELLOW_RGB), nearest(look->palette, RED_RGB),
                                nearest(look->palette, BLACK_RGB), 0 };
    ink.head_shadow = heads_on_red(look) ? CAR_RECORDS_HEAD_SHADOW : 0;
    uint8_t black[768];
    memset(black, 0, sizeof black);
    text_build_styles_from(look->palette);
    if (!text_small_init()) port_log("car records: the small font is not loaded\n");
    port_log("mark:screen=car_records\n");   /* port: the headless walk waits for it */
    int last_key = input_get_key(), last_buttons = pointer_read_buttons(), done = 0, changed = 1, first = 1;
    while (!done) {
        if (changed) {
            screen_frame(look, v, &ink);
            if (first) {   /* in from black */
                screen_show(black);
                video_fade_in(0, 256, look->palette);
                first = 0;
            } else screen_show(look->palette);
            port_log("mark:records_page=%d\n", v->page);
            changed = 0;
        }
        const int key = input_get_key(), buttons = pointer_read_buttons();
        const int press = key != last_key && key != 0 ? key : 0;
        if (buttons != last_buttons && (buttons & POINTER_BUTTON_SELECT)) {
            int x, y;
            pointer_position(&x, &y);
            const PageNavOps nav = nav_ops();
            const int lit = page_nav_lit(&nav, v->page, car_records_view_pages(v)), step = lit == 1 ? -1 : lit == 2 ? 1 : 0;
            if (step != 0) car_records_view_step(v, step), changed = 1, menu_audio_sound(SOUND_PAGE);
            else if (x <= DONE_RIGHT && y >= CAR_RECORDS_FOOT_TOP) done = 1;
        } else if (buttons != last_buttons && (buttons & POINTER_BUTTON_BACK)) done = 1;
        last_key = key, last_buttons = buttons;
        if (press == GALLERY_KEY_ESCAPE || press == GALLERY_KEY_ENTER) done = 1;
        else if (press == GALLERY_KEY_LEFT) car_records_view_step(v, -1), changed = 1, menu_audio_sound(SOUND_PAGE);
        else if (press == GALLERY_KEY_RIGHT) car_records_view_step(v, 1), changed = 1, menu_audio_sound(SOUND_PAGE);
        menu_audio_music_service();
        video_wait_vblank();
    }
    menu_audio_sound(SOUND_CHOSEN);
    video_fade_out(0, 256);   /* out to black: the page it came from fades in */
    for (int i = 0; i < LET_GO_FRAMES; i++) {   /* the press that closed it let go, so the page does not act on it */
        if (input_get_key() == 0 && (pointer_read_buttons() & POINTER_BUTTON_SELECT) == 0) break;
        menu_audio_music_service();
        video_wait_vblank();
    }
    free(v);
}

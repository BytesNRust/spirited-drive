/* port (dep59 step 4b, T6): the CONTENT screen's dialogs drawn (content_dialog_glue.h): the screen darkened through a shade table of the
 * palette (ea_shade_table, as the additions' panels are), the panel darker still, its frame the yellow text's colour, the words in the
 * text styles, the name's field the name box's own (glue_text_field). The log marks the dialog for the walks (mark:dialog=...). */
#include "game/frontend/content_dialog_glue.h"

#include "game/frontend/ea_lib.h"
#include "game/frontend/edition_name_box.h"
#include "game/frontend/menu_glue.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdlib.h>
#include <string.h>

enum {
    FULL = 0x10000, PANEL = 0x3000, CORNERS = 4, RELEASE_FRAMES = 30,
    YELLOW_RGB = 0xffce06, DIM_RGB = 0x8c8273   /* text.h's TEXT_YELLOW and TEXT_DIM */
};

typedef struct {
    uint8_t *kept;   /* the screen as the dialog opened, w x h */
    int w, h;
    uint8_t rgb[768];
} Dialog;

static void shade(const Dialog *d, int32_t level, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    static uint8_t table[256];   /* ea_shade_set keeps the pointer */
    ea_shade_table(level, d->rgb, table);
    ea_shade_set(table);
    const int32_t corners[2 * CORNERS] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    ea_shade_polygon(CORNERS, corners);
}

static void backdrop(void *ctx, int32_t percent)
{
    Dialog *d = ctx;
    int w, h;
    uint8_t *screen = video_target(&w, &h);
    if (d->kept && w == d->w && h == d->h) memcpy(screen, d->kept, (size_t)w * (size_t)h);
    shade(d, FULL - FULL * percent / 100, 0, 0, w - 1, h - 1);
}

static void panel(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1) { shade(ctx, PANEL, x0, y0, x1, y1); }

static void frame(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t lit)
{
    (void)ctx;
    const int32_t colour = ea_colour(lit ? YELLOW_RGB : DIM_RGB);
    ea_frame(x0, y0, x1, y1, colour);
    if (lit) ea_frame(x0 + 1, y0 + 1, x1 - 1, y1 - 1, colour);
}

static void words(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)ctx;
    text_select((TextStyle)style);
    text_draw_smooth(text, x, y, num, den);
}

static int32_t width(void *ctx, const char *text, int32_t num, int32_t den) { (void)ctx; return text_width_scaled(text, num, den); }

/* the name box's field puts back the screen's picture (ea_screen) under it as it closes and blinks: for its run the picture is the
 * dialog as drawn, then the screen's own again (the screen under the dialog redraws from it) */
static int32_t field(void *ctx, char *text, int32_t max, int32_t x, int32_t y)
{
    (void)ctx;
    int32_t pw = 0, ph = 0;
    int w, h;
    uint8_t *picture = ea_picture_pixels(ea_screen(), &pw, &ph);
    const uint8_t *screen = video_target(&w, &h);
    const size_t size = (size_t)w * (size_t)h;
    uint8_t *own = picture && pw == w && ph == h ? malloc(size) : NULL;
    if (own) memcpy(own, picture, size), memcpy(picture, screen, size);
    else port_log("content: the name's field draws over the screen's own picture\n");
    const int32_t taken = glue_text_field(text, max, x, y);
    if (own) memcpy(picture, own, size);
    free(own);
    return taken;
}

static void show(void *ctx) { (void)ctx, video_flip(), pointer_show(); }
static int32_t key_now(void *ctx) { (void)ctx; return input_get_key(); }

static int32_t pointer_at(void *ctx, int32_t *x, int32_t *y)
{
    (void)ctx;
    int px, py;
    const int32_t buttons = pointer_read_buttons();
    pointer_position(&px, &py);
    *x = px, *y = py;
    return buttons & POINTER_BUTTON_SELECT;
}

static void wait_frame(void *ctx) { (void)ctx, video_wait_vblank(); }

/* the screen put back, and the key or button that closed the dialog let go first: the screen under it does not take it too */
static void close_dialog(void *ctx)
{
    Dialog *d = ctx;
    for (int i = 0; i < RELEASE_FRAMES; i++) {
        const int k = input_get_key();   /* first: it pumps the events the buttons are read from */
        if (k == 0 && (pointer_read_buttons() & POINTER_BUTTON_SELECT) == 0) break;
        video_wait_vblank();
    }
    int w, h;
    uint8_t *screen = video_target(&w, &h);
    if (d->kept && w == d->w && h == d->h) memcpy(screen, d->kept, (size_t)w * (size_t)h);
    video_flip();
    pointer_show();
}

static ContentDialogOps open_dialog(Dialog *d, const char *mark)
{
    uint8_t *screen = video_target(&d->w, &d->h);
    d->kept = malloc((size_t)d->w * (size_t)d->h);
    if (d->kept) memcpy(d->kept, screen, (size_t)d->w * (size_t)d->h);
    else port_log("content: the screen under the dialog was not kept\n");
    video_get_palette(d->rgb);
    port_log("mark:dialog=%s\n", mark);   /* port (test harness): the walks wait for it */
    return (ContentDialogOps){ backdrop, panel, frame, words, width, field, show, key_now, pointer_at, wait_frame, close_dialog, d };
}

int32_t glue_content_install_prompt(const char *name, const char *version, const char *edition)
{
    Dialog d;
    const ContentDialogOps o = open_dialog(&d, edition ? "install" : "installed");
    const int32_t yes = content_install_prompt(&o, name, version, edition);
    free(d.kept);
    port_log("content: install of %s %s: %s\n", name ? name : "?", version ? version : "?", edition ? (yes ? "added" : "not added") : "noted");
    return yes;
}

int32_t glue_edition_name_box(const char *title, char *name)
{
    Dialog d;
    const ContentDialogOps o = open_dialog(&d, "name");
    const int32_t taken = edition_name_box(&o, title, name);
    free(d.kept);
    return taken;
}

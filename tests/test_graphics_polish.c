/* Polish: Graphics page 2's "Ultra" box and EXTRAS pages 1-3 use the binary's art in the same way as the exe's
 * Options/Controls screens, instead of a flat shaded panel. The exe's Options/Controls draws bgnd, then a lit/dark
 * shape per row (Tral/Absl/.../Dond/Cand, all from option/options.qfs). The binary has no Ultra or EXTRAS art; the
 * port's refit borrows the exe's view-row art (vanl / vand) for "Ultra" and the exe's Done/Cancel art (donl / canl /
 * dond / cand) for EXTRAS's bottom row, instead of a flat shaded panel. The refit does not change what any option
 * does. */
#include "game/frontend/graphics_menu.h"
#include "game/frontend/graphics_plus.h"
#include "game/frontend/extras_menu.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* port_settings.c / port_extras.c reach the platform's user files; the test has none */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ART_LOG = 64 };
typedef struct {
    const char *tags[ART_LOG];
    int count;
    int panel_calls;
    int last_panel_framed;
} Fake;

/* --- generic stubs (one signature per function-pointer type in the ops tables) --- */

static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static void f_void(void *ctx) { (void)ctx; }
static void f_void_v(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void f_move(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    Fake *f = ctx;
    if (f->count < ART_LOG) f->tags[f->count++] = tag;
    static uint8_t s[16];
    (void)bank;
    return s;
}
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)first, (void)count, (void)src, (void)mode;
    for (int i = 0; i < count; i++) rgb[first + i] = src ? src[i] : 0;
}
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{ (void)ctx, (void)x0, (void)x1, (void)y0, (void)y1; return 0; }
static int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{ (void)ctx, (void)style, (void)x, (void)y, (void)text; }
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx, (void)text; return 0; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)ctx, (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1;
    Fake *f = ctx;
    f->panel_calls++;
    f->last_panel_framed = framed;
}
/* fade_in: (ctx, first, count, rgb) — same arity as f_panel but different */
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{ (void)ctx, (void)first, (void)count, (void)rgb; }
static void f_fade_out(void *ctx, int32_t first, int32_t count)
{ (void)ctx, (void)first, (void)count; }
static void f_free(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void f_save_p(void *ctx, const PortSettings *s) { (void)ctx, (void)s; }
static void f_save_e(void *ctx, const PortExtras *e, int16_t unlock) { (void)ctx, (void)e, (void)unlock; }
/* extras rig keys: 0 for the first read (it arms the loop), then ESCAPE so step() sets p.result and the loop
 * returns. The loop needs rest>REST (0x19) before arming; ticks=100 arms it on the first read. */
static int ex_key_n;
static int32_t ex_key(void *ctx)
{
    (void)ctx;
    return ex_key_n++ == 0 ? 0 : 0x1b;
}
static int32_t f_ticks_100(void *ctx) { (void)ctx; return 100; }

static int any(const Fake *f, const char *tag)
{
    for (int i = 0; i < f->count; i++) if (strcmp(f->tags[i], tag) == 0) return 1;
    return 0;
}

/* Graphics polish: with view == Ultra, graphics_plus_ultra is called by put_value. The brief asks for the box behind
 * "Ultra" to be the exe's view-row art (vanl when lit) instead of a flat plus_panel(framed=0). */
static void test_ultra_box_uses_vanl_art(void)
{
    Fake f; memset(&f, 0, sizeof f);
    const GraphicsMenuOps ops = {
        .pointer_move_to = f_move, .load_art = f_art, .load_inverse = f_art, .music_feed = f_void, .shape = f_shape,
        .palette_set = f_palette, .pointer_poll = f_void, .pointer_mark = f_void, .pointer_in_rect = f_in_rect,
        .select = f_select, .blit_own = f_blit, .picture_shape = f_picture, .select_default = f_void,
        .pointer_hide = f_void, .unlock = f_void, .flip = f_void, .pointer_show = f_void, .pointer_buttons = f_zero,
        .text_styles = f_void, .fade_in = f_fade_in, .deadline_set = f_void_v, .sound = f_void_v, .lock = f_void,
        .ticks = f_zero, .pointer_activate_if_moved = f_void, .key = f_zero, .pointer_click = f_void,
        .pointer_off = f_void, .music = f_void, .net_sync = f_void, .deadline_passed = f_zero,
        .fade_out = f_fade_out, .free_inverse = f_void, .free = f_free,
        .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel, .plus_save = f_save_p,
        .ctx = &f };
    uint8_t rgb[GRAPHICS_PALETTE] = { 0 };
    graphics_plus_ultra(NULL, &ops, rgb, 1);   /* lit */
    CHECK(any(&f, "vanl"));   /* polish: red — current code does plus_panel, not blit */
}

/* EXTRAS polish: the Done/Cancel row's lit/dark art (donl/canl/dond/cand) is blitted from OPTIONS.QFS instead of a
 * flat panel + text. We pin one frame of extras_menu_run, the rig drawing once and Esc leaving. */
static void test_extras_done_row_blits_donl(void)
{
    Fake f; memset(&f, 0, sizeof f);
    const ExtrasMenuOps ops = {
        .pointer_move_to = f_move, .load_art = f_art, .shape = f_shape, .palette_set = f_palette,
        .pointer_in_rect = f_in_rect, .select = f_select, .blit_own = f_blit, .picture_shape = f_picture,
        .select_default = f_void, .pointer_hide = f_void, .flip = f_void, .pointer_show = f_void,
        .pointer_buttons = f_zero, .text_styles = f_void, .fade_in = f_fade_in, .deadline_set = f_void_v,
        .deadline_passed = f_zero, .sound = f_void_v, .ticks = f_ticks_100, .pointer_activate_if_moved = f_void,
        .key = ex_key, .pointer_click = f_void, .pointer_off = f_void, .music = f_void,
        .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel,
        .extras_save = f_save_e, .port_combine_variants = 0, .ctx = &f };
    PortExtras e = { 0 };
    int16_t unlock = 0;
    (void)extras_menu_run(&e, &unlock, &ops);
    CHECK(any(&f, "donl"));   /* polish: red — current code does plus_panel + plus_text "Done" */
}

int main(void)
{
    test_ultra_box_uses_vanl_art();
    test_extras_done_row_blits_donl();
    return TEST_REPORT();
}

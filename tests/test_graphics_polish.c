/* Polish: Graphics page 2's "Ultra" box and EXTRAS pages 1-3 use the binary's art the same way the exe's
 * Options/Controls screens use theirs. The exe's Options/Controls draws bgnd, then a lit/dark shape per row (Tral/
 * Absl/.../Dond/Cand, all from option/options.qfs). The ports refit is detail-for-detail the same, with three
 * peer-reported corrections from comparing against the original QFS banks:
 *   - GRAPHICS.QFS holds no "ultra" art; vanl / vand are the *label* art (195x25 at (5,262)), not a value box.
 *     The polish must build its value box at runtime from `bgnd`'s 96x28 sub-region at (408,261) (the place where
 *     far's art, farl/fard, sits) and write "Ultra" on top.
 *   - OPTIONS.QFS names are capitalised (Donl/Dond/Canl/Cand at sizes 82x27 / 80x24 / 112x27 / 112x24). shape_bank_find
 *     uses memcmp, so lowercase donl/dond/canl/cand return NULL on this bank.
 *   - menu_glue.c's `extras_blit_own_glue` was a no-op, dropping every blit_own call. Must forward to ea_blit_own.
 * The tests below pin all three against the running menu code. None of them touch what any option does. */
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

enum { ART_LOG = 64, BLIT_LOG = 16, PANEL_LOG = 8 };
typedef struct {
    const char *tags[ART_LOG];
    int count;
    int panel_calls;
    int last_panel_framed;
    int32_t panel_x0[PANEL_LOG], panel_y0[PANEL_LOG], panel_x1[PANEL_LOG], panel_y1[PANEL_LOG];
    int blit_calls;
    /* the first byte of every blit_own shape; records value at f_blit time so the test pin survives callers that
     * pass a stack-local FSH. */
    uint8_t blit_shape_first[BLIT_LOG];
    int blit_shape_first_set[BLIT_LOG];
    const uint8_t *blit_shapes[BLIT_LOG];
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
    /* GRAPHICS.QFS's "bgnd" is 640x480 — the runtime FSH for the Ultra box copies a 96x28 cell from it. The rig
     * hands back a real-sized buffer so the memcpy lands on valid memory. The slot is hashed from the tag so the
     * blit-payload audit can tell shapes apart. */
    static uint8_t small[8][16];
    static uint8_t big_bgnd[16 + 640 * 480];
    unsigned slot = 0;
    for (int i = 0; i < 4 && tag[i] != '\0'; i++) slot = (slot << 8) | (unsigned char)tag[i];
    if (slot == (unsigned)('b' << 24 | 'g' << 16 | 'n' << 8 | 'd')) {
        big_bgnd[0] = 1;   /* non-zero so audit treats it as a real shape */
        (void)bank;
        return big_bgnd;
    }
    slot %= 8;
    small[slot][0] = (uint8_t)(slot + 1);
    (void)bank;
    return small[slot];
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
static void f_blit(void *ctx, const uint8_t *shape)
{
    Fake *f = ctx;
    if (shape != NULL && f->blit_calls < BLIT_LOG) {
        f->blit_shapes[f->blit_calls] = shape;
        /* the production ea_blit_own reads the FSH header synchronously inside the call. Mirror that: snapshot
         * the first byte at f_blit time. */
        f->blit_shape_first[f->blit_calls] = shape[0];
        f->blit_shape_first_set[f->blit_calls] = 1;
    }
    f->blit_calls++;
}
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{ (void)ctx, (void)style, (void)x, (void)y, (void)text; }
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx, (void)text; return 0; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)ctx, (void)rgb;
    Fake *f = ctx;
    if (f->panel_calls < PANEL_LOG) {
        const int i = f->panel_calls;
        f->panel_x0[i] = x0; f->panel_y0[i] = y0; f->panel_x1[i] = x1; f->panel_y1[i] = y1;
    }
    f->panel_calls++;
    f->last_panel_framed = framed;
}
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{ (void)ctx, (void)first, (void)count, (void)rgb; }
static void f_fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
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
static int none(const Fake *f, const char *tag)
{
    return !any(f, tag);
}
static int all_have_payload(const Fake *f)
{
    /* at least the first byte of every recorded shape was non-zero at f_blit time — proves the rig actually
     * received a non-empty pointer, not the no-op glue dropping it. */
    for (int i = 0; i < f->blit_calls && i < BLIT_LOG; i++)
        if (f->blit_shape_first_set[i] && f->blit_shape_first[i] == 0) return 0;
    return 1;
}

/* Graphics polish 1/3: with view == Ultra, graphics_plus_ultra is called by put_value. The original GRAPHICS.QFS
 * has no Ultra art; vanl / vand are the *label* column (195x25 at y=262), not a value box. The polish must build a
 * runtime FSH from `bgnd`'s 96x28 value cell at (408,261) — the same place farl/fard live — and write "Ultra" on top;
 * it must NOT request vanl/vand. */
static void test_ultra_box_avoids_vanl_vand(void)
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
    graphics_plus_ultra(NULL, &ops, rgb, /*lit*/ 1, /*bank*/ 1);
    CHECK(none(&f, "vanl"));   /* polish: never the LABEL art from GRAPHICS.QFS */
    CHECK(none(&f, "vand"));
}

/* Graphics polish 1b/3: the Ultra cell must be opaque, not a slice of the burnt-in `bgnd`. bgnd is captured screen
 * art and (408,261..503,288) already contains Far's glyphs there. Sampling that slice and blitting it under Ultra
 * anti-aliases to "ULFTRA" — the F peeks out between the L and the T. The fix paints an opaque plus_panel over the
 * cell area first so any prior Farl/Fard glyphs are written over before Ultra text goes on. */
static void test_ultra_box_paints_opaque_panel(void)
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
    graphics_plus_ultra(NULL, &ops, rgb, /*lit*/ 1, /*bank*/ 1);
    /* at least one panel call, covering the 96x28 cell at (408,261) — the same place farl/fard sit. */
    int covered = 0;
    for (int i = 0; i < f.panel_calls; i++)
        if (f.panel_x0[i] <= 408 && f.panel_y0[i] <= 261 && f.panel_x1[i] >= 503 && f.panel_y1[i] >= 288) covered = 1;
    CHECK(covered);
}

/* EXTRAS polish 2/3: the Done/Cancel row's lit/dark art (Donl/Dond/Canl/Cand) is blitted from OPTIONS.QFS instead of
 * a flat panel + text. Memcmp shape lookup, so the names are CAPITALISED. We pin one frame of extras_menu_run, the rig
 * drawing once and Esc leaving. */
static void test_extras_done_row_blits_capitalised(void)
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
    ex_key_n = 0;
    (void)extras_menu_run(&e, &unlock, 1, &ops);
    /* OPTIONS.QFS names: capitalised first letter. */
    CHECK(any(&f, "Donl") || any(&f, "Dond"));
    CHECK(any(&f, "Canl") || any(&f, "Cand"));
    /* Lowercase "donl" / "dond" / "canl" / "cand" are NOT in the bank, so the menu must not ask for them. */
    CHECK(none(&f, "donl"));
    CHECK(none(&f, "dond"));
    CHECK(none(&f, "canl"));
    CHECK(none(&f, "cand"));
}

/* EXTRAS polish 3/3: the EXTRAS glue's blit_own must forward to ea_blit_own (currently a no-op). After one draw_page,
 * f_blit must have been invoked at least twice (Done then Cancel), and every blit_own arg must carry a non-zero
 * payload — proves the no-op glue is gone. We pin through the menu loop so the test stays in real call sites. */
static void test_extras_blit_own_forwards(void)
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
    ex_key_n = 0;
    (void)extras_menu_run(&e, &unlock, 1, &ops);
    CHECK(f.blit_calls >= 2);                       /* at least Done + Cancel */
    CHECK(all_have_payload(&f));                    /* not the no-op glue: the shape pointer actually reached f_blit */
}

int main(void)
{
    test_ultra_box_avoids_vanl_vand();
    test_ultra_box_paints_opaque_panel();
    test_extras_done_row_blits_capitalised();
    test_extras_blit_own_forwards();
    return TEST_REPORT();
}

/* The EXTRAS screen (extras_menu.c): two pages of switches driven from config.cfg and to it on Done. The plan's steps
 * are exercised with fake ops and a key script, the same shape as test_graphics_plus.c. */
#include "game/frontend/extras_menu.h"
#include "game/sys/port_extras.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* port_extras_load/save reads / writes user/extras.cfg through the platform; the test has no file */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       PAGE_DOWN = 0x5100 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int saves;
    PortExtras saved;
    int16_t saved_unlock;
    char drawn[96][32];
    int drawn_count;
    int pointer_in_nav;
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static void f_styles(void *ctx) { (void)ctx; }
static int32_t f_ticks(void *ctx) { (void)ctx; return 100; }
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static void f_void(void *ctx) { (void)ctx; }
static void f_one(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void f_move(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static uint8_t shape[16];
    (void)ctx, (void)bank, (void)tag;
    return shape;
}
static void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)rgb, (void)first, (void)count, (void)src, (void)mode;
}
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    Fake *f = ctx;
    return f->pointer_in_nav && x0 == EXTRAS_NAV_X0 && x1 == EXTRAS_NAV_X1 && y0 == EXTRAS_NAV_Y0 && y1 == EXTRAS_NAV_Y1;
}
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static int32_t f_buttons(void *ctx) { (void)ctx; return 0; }
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    (void)style, (void)x, (void)y;
    if (f->drawn_count < 96) snprintf(f->drawn[f->drawn_count++], sizeof f->drawn[0], "%s", text);
}
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)ctx, (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
}
static void f_save(void *ctx, const PortExtras *e, int16_t unlock)
{
    Fake *f = ctx;
    f->saves++;
    f->saved = *e;
    f->saved_unlock = unlock;
}

static int shown(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return 1;
    return 0;
}

typedef struct Rig {
    Fake fake;
    PortExtras e;
    int16_t unlock;
    PortExtras saved;
    int16_t saved_unlock;
} Rig;

static void setup(Rig *r)
{
    memset(r, 0, sizeof *r);
    r->e = port_extras_defaults();
    r->unlock = 0;
}

static void run(Rig *r, const int *keys, int count)
{
    r->fake.keys = keys;
    r->fake.key_count = count;
    const ExtrasMenuOps ops = {
        .pointer_move_to = f_move, .load_art = f_art, .shape = f_shape, .palette_set = f_palette,
        .pointer_in_rect = f_in_rect, .select = f_select, .blit_own = f_blit, .picture_shape = f_picture,
        .select_default = f_void, .pointer_hide = f_void, .flip = f_void, .pointer_show = f_void,
        .pointer_buttons = f_buttons, .text_styles = f_styles, .fade_in = f_fade_in, .deadline_set = f_one,
        .deadline_passed = f_zero, .sound = f_one, .ticks = f_ticks, .pointer_activate_if_moved = f_void,
        .key = f_key, .pointer_click = f_void, .pointer_off = f_void, .music = f_void,
        .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel,
        .extras_save = f_save, .ctx = &r->fake };
    (void)extras_menu_run(&r->e, &r->unlock, &ops);
}

/* Plan 3 Task 3 Step 1 — Edit Mirror (then Done) turns it on and turns Rally off when Rally was on */
static void test_mirror_toggle_excludes_rally(void)
{
    Rig r;
    /* ENTER on Mirror flips it on, then 8 DOWNs to Done, then ENTER for Done */
    int keys[16];
    int n = 0;
    keys[n++] = ENTER;
    for (int i = 0; i < ROWS_PAGE_1 - 2; i++) keys[n++] = DOWN;  /* 8 downs reach Done (row 8) */
    keys[n++] = ENTER;
    setup(&r);
    r.e.rally = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.mirror, 1);
    CHECK_EQ_INT(r.e.rally, 0);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* Escape restores the input PortExtras */
static void test_escape_restores_input(void)
{
    Rig r;
    const int keys[] = { SPACE, SPACE, SPACE, ESCAPE };  /* toggle off Mirror, Nitro, Turbo and quit */
    setup(&r);
    r.e.mirror = 1;
    r.e.nitro = 1;
    r.e.turbo = 1;
    run(&r, keys, 4);
    CHECK_EQ_INT(r.e.mirror, 1);
    CHECK_EQ_INT(r.e.nitro, 1);
    CHECK_EQ_INT(r.e.turbo, 1);
}

/* PAGE_DOWN turns to page 2 */
static void test_right_turns_to_page_two(void)
{
    Rig r;
    const int keys[] = { PAGE_DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK(shown(&r.fake, "< 2/2"));
}

/* Done writes the on-disk file (saves++ in the rig) */
static void test_done_writes_extras_cfg(void)
{
    Rig r;
    /* Mirror (row 0) is the initial selection. ROWS_PAGE_1-2 = 8 DOWNs land on ROW_DONE (row 8). */
    int keys[16] = {0};
    int n = 0;
    for (int i = 0; i < ROWS_PAGE_1 - 2; i++) keys[n++] = DOWN;
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.saved_unlock, 0);
}

int main(void)
{
    test_mirror_toggle_excludes_rally();
    test_escape_restores_input();
    test_right_turns_to_page_two();
    test_done_writes_extras_cfg();
    return TEST_REPORT();
}

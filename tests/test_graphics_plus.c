/* The graphics screen's port additions (graphics_plus.c, and graphics_menu.c's hooks): the fourth view distance, the
 * second page, Done saving port.cfg's settings and Cancel putting them back. Driven through fake ops with a key script. */
#include "game/frontend/graphics_menu.h"
#include "game/frontend/graphics_plus.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* port_settings.c, which the screen's settings come from, reaches the platform's files; the test has none */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, F3 = 0x3d00, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00,
       DOWN = 0x5000, PAGE_DOWN = 0x5100 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int saves, text_styles, ticks_each;   /* ticks_each: what a read of the clock adds (0: 100, enough to take a key) */
    PortSettings saved;
    char drawn[64][24];
    int drawn_count;
    int pointer_in_nav, pointer_down;
    const int *button_script;   /* when set, the buttons as read one after the other (the last one stays) */
    int button_count, button_next;
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static void f_styles(void *ctx) { ((Fake *)ctx)->text_styles++; }
static int32_t f_ticks(void *ctx)
{
    const int each = ((const Fake *)ctx)->ticks_each;
    return each != 0 ? each : 100;
}
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static void f_void(void *ctx) { (void)ctx; }
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
    const Fake *f = ctx;
    return f->pointer_in_nav && x0 == PLUS_NAV_X0 && x1 == PLUS_NAV_X1 && y0 == PLUS_NAV_Y0 && y1 == PLUS_NAV_Y1;
}
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static void f_one(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void f_fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
static int32_t f_buttons(void *ctx)
{
    Fake *f = ctx;
    if (f->button_script == NULL) return f->pointer_down;
    return f->button_script[f->button_next < f->button_count ? f->button_next++ : f->button_count - 1];
}
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    (void)style, (void)x, (void)y;
    if (f->drawn_count < 64) snprintf(f->drawn[f->drawn_count++], sizeof f->drawn[0], "%s", text);
}
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)ctx, (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
}
static void f_save(void *ctx, const PortSettings *s)
{
    Fake *f = ctx;
    f->saves++;
    f->saved = *s;
}

static int shown(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return 1;
    return 0;
}

static int shown_count(const Fake *f, const char *text)
{
    int n = 0;
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) n++;
    return n;
}

static GraphicsMenuOps make_ops(Fake *f)
{
    GraphicsMenuOps o = {
        .pointer_move_to = f_move, .load_art = f_art, .load_inverse = f_art, .music_feed = f_void, .shape = f_shape,
        .palette_set = f_palette, .pointer_poll = f_void, .pointer_mark = f_void, .pointer_in_rect = f_in_rect,
        .select = f_select, .blit_own = f_blit, .picture_shape = f_picture, .select_default = f_void,
        .pointer_hide = f_void, .unlock = f_void, .flip = f_void, .pointer_show = f_void, .pointer_buttons = f_buttons,
        .text_styles = f_styles, .fade_in = f_fade_in, .deadline_set = f_one, .sound = f_one, .lock = f_void,
        .ticks = f_ticks, .pointer_activate_if_moved = f_void, .key = f_key, .pointer_click = f_void,
        .pointer_off = f_void, .music = f_void, .net_sync = f_void, .deadline_passed = f_zero,
        .fade_out = f_fade_out, .free_inverse = f_void, .free = f_one, .ctx = f };
    o.plus_text = f_text, o.plus_text_width = f_text_width, o.plus_panel = f_panel, o.plus_save = f_save;
    return o;
}

typedef struct Rig {
    Fake fake;
    uint8_t config[GRAPHICS_KEPT];
    int16_t showing, net_off, active, buttons, quit;
    int32_t screen, inverse;
    PortSettings port;
    GraphicsPlus plus;
} Rig;

static int16_t view_of(const Rig *r)
{
    int16_t v;
    memcpy(&v, r->config + GRAPHICS_VIEW, sizeof v);
    return v;
}

/* the screen from the Options entry (mode 0) with `keys`, on a rig that has `with_plus` or not */
static void run(Rig *r, const int *keys, int count, int with_plus)
{
    memset(r, 0, sizeof *r);
    r->fake.keys = keys, r->fake.key_count = count;
    r->port = port_settings_defaults();
    r->plus.port = &r->port;
    r->plus.fullscreen_format = SCREEN_21_9;
    const GraphicsMenuOps ops = make_ops(&r->fake);
    const GraphicsMenuState st = { r->config, &r->showing, &r->net_off, &r->active, &r->buttons, &r->screen,
                                   &r->inverse, &r->quit, with_plus ? &r->plus : NULL };
    graphics_menu(&st, 0, &ops);
}

static void test_value_steps(void)
{
    PortSettings s = port_settings_defaults();
    graphics_plus_step(&s, PLUS_FORMAT, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.format, SCREEN_16_9);
    graphics_plus_step(&s, PLUS_FORMAT, +1, SCREEN_4_3);
    graphics_plus_step(&s, PLUS_FORMAT, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.format, SCREEN_4_3);                 /* wraps */
    graphics_plus_step(&s, PLUS_FORMAT, -1, SCREEN_4_3);
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    graphics_plus_step(&s, PLUS_REFRESH, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.refresh_hz, 120);
    graphics_plus_step(&s, PLUS_REFRESH, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.refresh_hz, 144);
    graphics_plus_step(&s, PLUS_REFRESH, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.refresh_hz, 60);
    graphics_plus_step(&s, PLUS_SCALE, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.scale_integer, 1);
    graphics_plus_step(&s, PLUS_SCALE, -1, SCREEN_4_3);
    CHECK_EQ_INT(s.scale_integer, 0);
    graphics_plus_step(&s, PLUS_FOV, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.fov_adjust, 5);
    graphics_plus_step(&s, PLUS_FOV, -1, SCREEN_4_3);
    graphics_plus_step(&s, PLUS_FOV, -1, SCREEN_4_3);
    graphics_plus_step(&s, PLUS_FOV, -1, SCREEN_4_3);
    CHECK_EQ_INT(s.fov_adjust, -10);
    graphics_plus_step(&s, PLUS_FOV, -1, SCREEN_4_3);
    CHECK_EQ_INT(s.fov_adjust, 20);                     /* wraps the other way */
    graphics_plus_step(&s, PLUS_FOV, +1, SCREEN_4_3);
    CHECK_EQ_INT(s.fov_adjust, -10);
}

static void test_fullscreen_takes_the_desktop_format(void)
{
    PortSettings s = port_settings_defaults();
    graphics_plus_step(&s, PLUS_FULLSCREEN, +1, SCREEN_21_9);
    CHECK_EQ_INT(s.fullscreen, 1);
    CHECK_EQ_INT(s.format, SCREEN_21_9);
    graphics_plus_step(&s, PLUS_FORMAT, -1, SCREEN_21_9);   /* the owner may change it after */
    CHECK_EQ_INT(s.format, SCREEN_16_9);
    graphics_plus_step(&s, PLUS_FULLSCREEN, +1, SCREEN_21_9);
    CHECK_EQ_INT(s.fullscreen, 0);
    CHECK_EQ_INT(s.format, SCREEN_16_9);                    /* turning it off leaves the format */
}

static void test_value_text(void)
{
    char text[16];
    PortSettings s = { SCREEN_21_9, 144, 1, 1, -5, 0 };
    graphics_plus_value(&s, PLUS_FORMAT, text); CHECK(strcmp(text, "21:9") == 0);
    graphics_plus_value(&s, PLUS_REFRESH, text); CHECK(strcmp(text, "144 Hz") == 0);
    graphics_plus_value(&s, PLUS_SCALE, text); CHECK(strcmp(text, "Whole") == 0);
    graphics_plus_value(&s, PLUS_FULLSCREEN, text); CHECK(strcmp(text, "On") == 0);
    graphics_plus_value(&s, PLUS_FOV, text); CHECK(strcmp(text, "-5") == 0);
    s.fov_adjust = 0, s.scale_integer = 0;
    graphics_plus_value(&s, PLUS_FOV, text); CHECK(strcmp(text, "0") == 0);
    graphics_plus_value(&s, PLUS_SCALE, text); CHECK(strcmp(text, "Fill") == 0);
}

static void test_view_cycle_has_four_values(void)
{
    int16_t view = 0;
    int ultra = 0;
    graphics_plus_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 1); CHECK_EQ_INT(ultra, 0);     /* far -> medium, as the original */
    graphics_plus_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 2); CHECK_EQ_INT(ultra, 0);     /* close */
    graphics_plus_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 0); CHECK_EQ_INT(ultra, 1);     /* ultra: the far art with the flag */
    graphics_plus_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 0); CHECK_EQ_INT(ultra, 0);     /* back to far */
}

static void test_page_one_cycles_four_views_and_cancel_puts_back(void)
{
    Rig r;
    const int keys[] = { DOWN, DOWN, DOWN, SPACE, SPACE, SPACE, ESCAPE };
    run(&r, keys, 7, 1);
    CHECK_EQ_INT(view_of(&r), 0);
    CHECK_EQ_INT(r.port.view_ultra, 0);                /* Escape: the entry's values */
    CHECK_EQ_INT(r.fake.saves, 0);
    CHECK(shown(&r.fake, "Ultra"));
    CHECK(shown(&r.fake, "1/2 >"));
    CHECK(r.fake.text_styles >= 2);   /* the text's colours rebuilt once the palette has faded in */
}

static void test_first_picture_is_drawn_again_after_the_fade(void)
{
    Rig r;
    const int keys[] = { 0, ESCAPE };
    run(&r, keys, 2, 1);
    CHECK_EQ_INT(shown_count(&r.fake, "1/2 >"), 2);   /* before the fade, and again with the styles built */
}

static void test_page_one_ultra_kept_by_done(void)
{
    Rig r;
    /* down to the view, three steps to ultra, down to Done, Enter */
    const int keys[] = { DOWN, DOWN, DOWN, SPACE, SPACE, SPACE, DOWN, DOWN, DOWN, ENTER };
    run(&r, keys, 10, 1);
    CHECK_EQ_INT(view_of(&r), 0);
    CHECK_EQ_INT(r.port.view_ultra, 1);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.view_ultra, 1);
}

static void test_without_plus_the_view_has_three_values(void)
{
    Rig r;
    const int keys[] = { DOWN, DOWN, DOWN, SPACE, SPACE, SPACE, DOWN, DOWN, DOWN, ENTER };
    run(&r, keys, 10, 0);
    CHECK_EQ_INT(view_of(&r), 0);                      /* 0 -> 1 -> 2 -> 0 */
    CHECK_EQ_INT(r.fake.saves, 0);
    CHECK(!shown(&r.fake, "1/2 >"));
}

static void test_second_page_edits_and_done_saves(void)
{
    Rig r;
    const int keys[] = {
        PAGE_DOWN,                 /* to page 2, the format selected */
        SPACE,                     /* 16:9 */
        DOWN, SPACE,               /* 120 Hz */
        DOWN, SPACE,               /* whole multiples */
        DOWN, SPACE,               /* fullscreen: the desktop's format, 21:9 */
        DOWN, RIGHT, RIGHT, RIGHT, LEFT,  /* field of view +10 */
        DOWN, ENTER };             /* Done */
    run(&r, keys, 15, 1);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.format, SCREEN_21_9);
    CHECK_EQ_INT(r.fake.saved.refresh_hz, 120);
    CHECK_EQ_INT(r.fake.saved.scale_integer, 1);
    CHECK_EQ_INT(r.fake.saved.fullscreen, 1);
    CHECK_EQ_INT(r.fake.saved.fov_adjust, 10);
    CHECK(shown(&r.fake, "Field of view"));
    CHECK(shown(&r.fake, "< 2/2"));
}

static void test_right_arrow_turns_to_page_two(void)
{
    Rig r;
    /* Right from the first item, the format a step, Right on Done / Cancel is the original's again: Enter on Done */
    const int keys[] = { RIGHT, SPACE, DOWN, DOWN, DOWN, DOWN, DOWN, ENTER };
    run(&r, keys, 8, 1);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.format, SCREEN_16_9);
}

static void test_the_key_that_turned_the_page_does_not_step(void)
{
    Rig r;
    /* Right turns the page and is still down (read again) before the clock has re-armed the keys: the format must not
     * move; a read of no key arms them, and Space moves it */
    const int keys[] = { RIGHT, RIGHT, 0, SPACE, 0, 0, 0, ESCAPE };
    memset(&r, 0, sizeof r);
    r.fake.ticks_each = 10;
    r.fake.keys = keys, r.fake.key_count = 8;
    r.port = port_settings_defaults();
    r.plus.port = &r.port;
    const GraphicsMenuOps ops = make_ops(&r.fake);
    const GraphicsMenuState st = { r.config, &r.showing, &r.net_off, &r.active, &r.buttons, &r.screen, &r.inverse,
                                   &r.quit, &r.plus };
    graphics_menu(&st, 0, &ops);
    CHECK(shown(&r.fake, "16:9"));
    CHECK(!shown(&r.fake, "21:9"));   /* one step, not two */
}

static void test_page_up_from_page_one_goes_to_page_two(void)
{
    Rig r;
    const int keys[] = { 0x4900 /* page up */, SPACE, ENTER };
    run(&r, keys, 3, 1);
    CHECK(shown(&r.fake, "< 2/2"));
}

static void test_second_page_cancel_puts_back(void)
{
    Rig r;
    const int keys[] = { PAGE_DOWN, SPACE, DOWN, SPACE, ESCAPE };
    run(&r, keys, 5, 1);
    CHECK_EQ_INT(r.fake.saves, 0);
    CHECK_EQ_INT(r.port.format, SCREEN_4_3);
    CHECK_EQ_INT(r.port.refresh_hz, 60);
}

static void test_back_to_page_one_and_cancel_button(void)
{
    Rig r;
    /* page 2, change the format, Left back to page 1, down to Done, Right to Cancel, Enter */
    const int keys[] = { PAGE_DOWN, SPACE, 0x4900 /* page up */, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, RIGHT, ENTER };
    run(&r, keys, 11, 1);
    CHECK_EQ_INT(r.fake.saves, 0);
    CHECK_EQ_INT(r.port.format, SCREEN_4_3);
    CHECK(shown(&r.fake, "1/2 >"));
}

static void test_cancel_button_on_page_two_puts_back(void)
{
    Rig r;
    const int keys[] = { PAGE_DOWN, SPACE, DOWN, DOWN, DOWN, DOWN, DOWN, RIGHT, ENTER };
    run(&r, keys, 9, 1);
    CHECK_EQ_INT(r.fake.saves, 0);
    CHECK_EQ_INT(r.port.format, SCREEN_4_3);
}

static void test_nav_click_on_page_two_goes_back(void)
{
    Rig r;
    /* the first draw reads the buttons once (up); then the indicator clicked on page 1 (down), up, and on page 2
     * (down), up; then Escape on page 1 */
    const int buttons[] = { 0, 1, 0, 1, 0 };
    const int keys[] = { 0, 0, 0, 0, 0, 0, ESCAPE };
    memset(&r, 0, sizeof r);
    r.fake.keys = keys, r.fake.key_count = 7;
    r.fake.button_script = buttons, r.fake.button_count = 5;
    r.fake.pointer_in_nav = 1;
    r.active = 1;
    r.port = port_settings_defaults();
    r.plus.port = &r.port;
    const GraphicsMenuOps ops = make_ops(&r.fake);
    const GraphicsMenuState st = { r.config, &r.showing, &r.net_off, &r.active, &r.buttons, &r.screen, &r.inverse,
                                   &r.quit, &r.plus };
    graphics_menu(&st, 0, &ops);
    CHECK(shown(&r.fake, "Field of view"));
    CHECK_EQ_INT(r.fake.saves, 0);   /* back on page 1 for the Escape: a cancel, not Done */
}

static void test_nav_click_turns_the_page(void)
{
    Rig r;
    const int keys[] = { 0, 0, ESCAPE };
    memset(&r, 0, sizeof r);
    r.fake.keys = keys, r.fake.key_count = 3;
    r.fake.pointer_in_nav = 1, r.fake.pointer_down = 1;   /* the left button down over the nav text */
    r.active = 1;
    r.port = port_settings_defaults();
    r.plus.port = &r.port;
    const GraphicsMenuOps ops = make_ops(&r.fake);
    const GraphicsMenuState st = { r.config, &r.showing, &r.net_off, &r.active, &r.buttons, &r.screen, &r.inverse,
                                   &r.quit, &r.plus };
    graphics_menu(&st, 0, &ops);
    CHECK(shown(&r.fake, "Field of view"));
}

int main(void)
{
    test_value_steps();
    test_fullscreen_takes_the_desktop_format();
    test_value_text();
    test_view_cycle_has_four_values();
    test_page_one_cycles_four_views_and_cancel_puts_back();
    test_first_picture_is_drawn_again_after_the_fade();
    test_page_one_ultra_kept_by_done();
    test_without_plus_the_view_has_three_values();
    test_second_page_edits_and_done_saves();
    test_right_arrow_turns_to_page_two();
    test_the_key_that_turned_the_page_does_not_step();
    test_page_up_from_page_one_goes_to_page_two();
    test_second_page_cancel_puts_back();
    test_back_to_page_one_and_cancel_button();
    test_cancel_button_on_page_two_puts_back();
    test_nav_click_on_page_two_goes_back();
    test_nav_click_turns_the_page();
    return TEST_REPORT();
}

/* The controls screen's port pages (controls_plus.c): capturing a binding, clearing, adding, the tuning rows, Defaults,
 * and how the pages are left. Driven through fake ops with a key script and a pad script. */
#include "game/frontend/controls_plus.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* port_controls.c reaches the platform's files; the test has none */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ENTER = 0xd, ESCAPE = 0x1b, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000, INSERT = 0x5200,
       DELETE = 0x5300 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    const RawPad *pads;      /* what pad_raw answers, one after the other (the last stays); NULL: no pad */
    int pad_count, pad_next;
    int deadline_calls;      /* deadline_passed answers 1 from this many calls on (never when 0) */
    int deadline_next;
    char drawn[256][40];
    int drawn_count;
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static int32_t f_ticks(void *ctx) { (void)ctx; return 100; }
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static void f_void(void *ctx) { (void)ctx; }
static void f_move(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static int32_t f_in_rect(void *ctx, int32_t a, int32_t b, int32_t c, int32_t d)
{
    (void)ctx, (void)a, (void)b, (void)c, (void)d;
    return 0;
}
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static uint8_t shape[16];
    (void)ctx, (void)bank, (void)tag;
    return shape;
}
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
static void f_one(void *ctx, int32_t v) { (void)ctx, (void)v; }
static int32_t f_deadline(void *ctx)
{
    Fake *f = ctx;
    return f->deadline_calls != 0 && ++f->deadline_next >= f->deadline_calls;
}
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    (void)style, (void)x, (void)y;
    if (f->drawn_count < 256) snprintf(f->drawn[f->drawn_count++], sizeof f->drawn[0], "%s", text);
}
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)ctx, (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
}
static int32_t f_pad(void *ctx, RawPad *out)
{
    Fake *f = ctx;
    if (f->pads == NULL) return 0;
    *out = f->pads[f->pad_next < f->pad_count ? f->pad_next++ : f->pad_count - 1];
    return 1;
}
static void f_save(void *ctx, const PortControls *c) { (void)ctx, (void)c; }

static int shown(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return 1;
    return 0;
}

static ControlsMenuOps make_ops(Fake *f)
{
    ControlsMenuOps o = {
        .pointer_move_to = f_move, .pointer_in_rect = f_in_rect, .select = f_select, .blit_own = f_blit,
        .shape = f_shape, .picture_shape = f_picture, .select_default = f_void, .flip = f_void,
        .pointer_show = f_void, .deadline_set = f_one, .sound = f_one, .ticks = f_ticks, .pointer_buttons = f_zero,
        .pointer_activate_if_moved = f_void, .key = f_key, .pointer_click = f_void, .deadline_passed = f_deadline,
        .music = f_void, .pointer_off = f_void, .net_sync = f_void, .ctx = f, .plus_text = f_text,
        .plus_text_width = f_text_width, .plus_panel = f_panel, .plus_save = f_save, .pad_raw = f_pad };
    return o;
}

/* a gamepad at rest, and one with a button, a trigger or the stick pressed */
static RawPad idle(void)
{
    RawPad p;
    memset(&p, 0, sizeof p);
    p.gamepad = 1, p.buttons = 15, p.axes = 6, p.hats = 0;
    return p;
}
static RawPad with_button(int b)
{
    RawPad p = idle();
    p.button[b] = 1;
    return p;
}
static RawPad with_axis(int a, int v)
{
    RawPad p = idle();
    p.axis[a] = (int16_t)v;
    return p;
}

typedef struct Run { ControlsPageResult result; PortControls port; Fake fake; } Run;

static void run(Run *r, const int *keys, int key_count, const RawPad *pads, int pad_count)
{
    int16_t buttons = 0;
    const int16_t pointer_active = 0, net_off = 1, quit = 0;
    const int32_t screen = 1;
    ControlsPlus plus = { &r->port };
    const ControlsMenuState st = { .buttons = &buttons, .pointer_active = &pointer_active, .net_off = &net_off,
                                   .quit = &quit, .screen = &screen, .plus = &plus };
    r->fake.keys = keys, r->fake.key_count = key_count;
    r->fake.pads = pads, r->fake.pad_count = pad_count;
    const ControlsMenuOps o = make_ops(&r->fake);
    uint8_t rgb[16] = { 0 };
    r->result = controls_plus_run(&st, &o, 1, rgb);
}

#define KEYS(...) (const int[]){ __VA_ARGS__ }, (int)(sizeof((const int[]){ __VA_ARGS__ }) / sizeof(int))
#define PADS(...) (const RawPad[]){ __VA_ARGS__ }, (int)(sizeof((const RawPad[]){ __VA_ARGS__ }) / sizeof(RawPad))

static void test_tuning_stays_in_range(void)
{
    PortControls c = port_controls_defaults();
    CHECK_EQ_INT(controls_plus_tune(&c, TUNE_DEADZONE, -1), 0);   /* 0 is the bottom */
    CHECK_EQ_INT(c.deadzone, 0);
    CHECK_EQ_INT(controls_plus_tune(&c, TUNE_DEADZONE, 1), 1);
    CHECK_EQ_INT(c.deadzone, 5);
    for (int i = 0; i < 30; i++) controls_plus_tune(&c, TUNE_DEADZONE, 1);
    CHECK_EQ_INT(c.deadzone, 50);
    for (int i = 0; i < 40; i++) controls_plus_tune(&c, TUNE_SENSITIVITY, 1);
    CHECK_EQ_INT(c.sensitivity, 200);
    for (int i = 0; i < 40; i++) controls_plus_tune(&c, TUNE_SENSITIVITY, -1);
    CHECK_EQ_INT(c.sensitivity, 50);
    for (int i = 0; i < 40; i++) controls_plus_tune(&c, TUNE_FORCE, 1);
    CHECK_EQ_INT(c.ffb_strength, 100);
    char t[16];
    controls_plus_tune_text(&c, TUNE_FORCE, t);
    CHECK(strcmp(t, "100%") == 0);
}

static void test_enter_captures_an_axis_for_the_steering(void)
{
    Run r = { .port = port_controls_defaults() };
    r.port.bind[ACT_STEER][0] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    run(&r, KEYS(ENTER, 0, 0, ESCAPE), PADS(idle(), idle(), with_axis(2, 30000)));   /* start, rest, then moved */
    CHECK_EQ_INT(r.port.bind[ACT_STEER][0].kind, SRC_AXIS_FULL);                      /* steering takes the whole axis */
    CHECK_EQ_INT(r.port.bind[ACT_STEER][0].index, 2);
    CHECK_EQ_INT(r.result, CONTROLS_PAGE_CANCEL);
    CHECK(shown(&r.fake, "Press a button or move an axis..."));
}

static void test_escape_in_a_capture_assigns_nothing(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    run(&r, KEYS(ENTER, ESCAPE, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
    CHECK_EQ_INT(r.result, CONTROLS_PAGE_CANCEL);   /* the second Escape, the page's */
}

static void test_nothing_for_a_while_assigns_nothing(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    r.fake.deadline_calls = 5;
    run(&r, KEYS(ENTER, 0, 0, 0, 0, 0, 0, ESCAPE), PADS(idle()));
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
}

static void test_a_held_button_is_not_a_press(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    r.fake.deadline_calls = 4;
    run(&r, KEYS(ENTER, 0, 0, 0, 0, 0, ESCAPE), PADS(with_button(4)));   /* already down when the capture began */
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
}

static void test_no_pad_waits_and_says_so(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    r.fake.deadline_calls = 3;
    run(&r, KEYS(ENTER, 0, 0, 0, 0, ESCAPE), NULL, 0);
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
    CHECK(shown(&r.fake, "No controller found"));
}

static void test_a_source_moves_from_the_action_that_had_it(void)
{
    Run r = { .port = port_controls_defaults() };   /* gas is axis 5+ */
    /* down to Brake (row 4), capture the right trigger there */
    run(&r, KEYS(DOWN, 0, DOWN, 0, DOWN, 0, DOWN, 0, ENTER, 0, 0, ESCAPE),
        PADS(idle(), idle(), with_axis(5, 32000)));
    CHECK_EQ_INT(r.port.bind[ACT_BRAKE][0].kind, SRC_AXIS_POS);
    CHECK_EQ_INT(r.port.bind[ACT_BRAKE][0].index, 5);
    CHECK_EQ_INT(r.port.bind[ACT_GAS][0].kind, SRC_NONE);
}

static void test_enter_replaces_and_insert_adds(void)
{
    Run r = { .port = port_controls_defaults() };
    r.port.bind[ACT_STEER_LEFT][1] = (ControlSource){ SRC_BUTTON, 7, 0, 0 };
    /* Steer left is row 1: Enter replaces both with button 4 */
    run(&r, KEYS(DOWN, 0, ENTER, 0, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK_EQ_INT(r.port.bind[ACT_STEER_LEFT][0].index, 4);
    CHECK_EQ_INT(r.port.bind[ACT_STEER_LEFT][1].kind, SRC_NONE);
    Run s = { .port = port_controls_defaults() };
    run(&s, KEYS(DOWN, 0, INSERT, 0, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK_EQ_INT(s.port.bind[ACT_STEER_LEFT][0].index, 13);       /* the d-pad stays */
    CHECK_EQ_INT(s.port.bind[ACT_STEER_LEFT][1].index, 4);
}

static void test_delete_clears(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(DOWN, 0, DELETE, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.bind[ACT_STEER_LEFT][0].kind, SRC_NONE);
    CHECK_EQ_INT(r.port.bind[ACT_STEER][0].kind, SRC_AXIS_FULL);   /* the others stay */
}

static void test_the_tuning_page(void)
{
    Run r = { .port = port_controls_defaults() };
    /* Right turns to page 3; Right on Dead zone is 5 %, Down to Sensitivity, Left is 95 % */
    run(&r, KEYS(RIGHT, 0, RIGHT, 0, DOWN, 0, LEFT, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.deadzone, 5);
    CHECK_EQ_INT(r.port.sensitivity, 95);
    CHECK(shown(&r.fake, "< 3/3"));
    CHECK(shown(&r.fake, "Force feedback"));
}

static void test_defaults_restores_the_bindings(void)
{
    Run r = { .port = port_controls_defaults() };
    r.port.bind[ACT_GAS][0] = (ControlSource){ SRC_BUTTON, 9, 0, 0 };
    r.port.deadzone = 20;
    r.port.custom = 1;
    /* page 3, down to Defaults (row 3), Enter */
    run(&r, KEYS(RIGHT, 0, DOWN, 0, DOWN, 0, DOWN, 0, ENTER, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.bind[ACT_GAS][0].kind, SRC_AXIS_POS);
    CHECK_EQ_INT(r.port.deadzone, 0);
    CHECK_EQ_INT(r.port.custom, 1);   /* it was read from a file; still is */
}

static void test_how_the_pages_are_left(void)
{
    Run done = { .port = port_controls_defaults() };
    int keys[ACT_COUNT * 2 + 2], n = 0;
    for (int i = 0; i < ACT_COUNT; i++) keys[n++] = DOWN, keys[n++] = 0;   /* past the rows: Done */
    keys[n++] = ENTER;
    run(&done, keys, n, PADS(idle()));
    CHECK_EQ_INT(done.result, CONTROLS_PAGE_DONE);
    Run back = { .port = port_controls_defaults() };
    run(&back, KEYS(LEFT), PADS(idle()));
    CHECK_EQ_INT(back.result, CONTROLS_PAGE_BACK);
    Run esc = { .port = port_controls_defaults() };
    run(&esc, KEYS(ESCAPE), PADS(idle()));
    CHECK_EQ_INT(esc.result, CONTROLS_PAGE_CANCEL);
}

static void test_a_wheel_shows_its_own_numbers(void)
{
    Run r = { .port = port_controls_defaults() };
    RawPad wheel = idle();
    wheel.gamepad = 0;
    run(&r, KEYS(ESCAPE), PADS(wheel));
    CHECK(shown(&r.fake, "Button 13, Axis 5+") == 0);
    CHECK(shown(&r.fake, "Axis 0"));
    CHECK(shown(&r.fake, "Axis 5+"));
}

int main(void)
{
    test_tuning_stays_in_range();
    test_enter_captures_an_axis_for_the_steering();
    test_escape_in_a_capture_assigns_nothing();
    test_nothing_for_a_while_assigns_nothing();
    test_a_held_button_is_not_a_press();
    test_no_pad_waits_and_says_so();
    test_a_source_moves_from_the_action_that_had_it();
    test_enter_replaces_and_insert_adds();
    test_delete_clears();
    test_the_tuning_page();
    test_defaults_restores_the_bindings();
    test_how_the_pages_are_left();
    test_a_wheel_shows_its_own_numbers();
    return TEST_REPORT();
}

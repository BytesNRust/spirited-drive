/* The controls screen's port pages (controls_plus.c): capturing a binding, clearing, adding, the tuning rows, Defaults,
 * how the pages are left, and how they are drawn (the clean configuration background, the title and the headers, no
 * panel). Driven through fake ops with a key script and a pad script. */
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
/* the band a row's words may stand in (controls_plus.c's ROW_TOP .. the Done band) */
enum { BAND_TOP = 96, BAND_BOTTOM = 430 };
enum { PLAIN = 0, SMOOTH = 1, HEADER = 2, TITLE = 3 };

typedef struct Text { char text[40]; int kind; int32_t y; } Text;

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    const RawPad *pads;      /* what pad_raw answers, one after the other (the last stays); NULL: no pad */
    int pad_count, pad_next;
    int deadline_calls;      /* deadline_passed answers 1 from this many calls on (never when 0) */
    int deadline_next;
    Text drawn[256];
    int drawn_count;
    int panels;              /* plus_panel calls: none on the pages (no panel, no red frame) */
    int composed;            /* plus_bgnd calls: the clean background is composed */
    int put;                 /* plus_bgnd_shape calls: the clean background is put on the screen */
    int legacy_bgnd;         /* shape() calls for the controls' own "bgnd" art: none when the clean one is there */
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
    Fake *f = ctx;
    (void)bank;
    if (strcmp(tag, "bgnd") == 0) f->legacy_bgnd++;
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
static void f_record(Fake *f, int kind, const char *text, int32_t y)
{
    if (f->drawn_count >= 256) return;
    Text *t = &f->drawn[f->drawn_count++];
    snprintf(t->text, sizeof t->text, "%s", text);
    t->kind = kind, t->y = y;
}
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    (void)style, (void)x;
    f_record(ctx, PLAIN, text, y);
}
static void f_smooth(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    (void)style, (void)x;
    f_record(ctx, SMOOTH, text, y);
}
static void f_title(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)x;
    f_record(ctx, TITLE, text, y);
}
static void f_header(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    (void)x, (void)width;
    f_record(ctx, HEADER, text, y);
}
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    Fake *f = ctx;
    (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
    f->panels++;
}
static int32_t f_bgnd(void *ctx, uint8_t pixels[CONTROLS_BGND_W * CONTROLS_BGND_H], uint8_t palette[768])
{
    Fake *f = ctx;
    (void)pixels, (void)palette;
    f->composed++;
    return 1;
}
static int32_t f_bgnd_shape(void *ctx, const uint8_t pixels[CONTROLS_BGND_W * CONTROLS_BGND_H],
                            const uint8_t palette[768])
{
    Fake *f = ctx;
    (void)pixels, (void)palette;
    f->put++;
    return 1;
}
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx, (void)first, (void)count, (void)rgb; }
static void f_fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
static int32_t f_load(void *ctx, const char *name) { (void)ctx, (void)name; return 7; }
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
        if (strcmp(f->drawn[i].text, text) == 0) return 1;
    return 0;
}

/* a word drawn in the given way (PLAIN, SMOOTH, HEADER, TITLE) */
static int shown_as(const Fake *f, const char *text, int kind)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (f->drawn[i].kind == kind && strcmp(f->drawn[i].text, text) == 0) return 1;
    return 0;
}

static ControlsMenuOps make_ops(Fake *f)
{
    ControlsMenuOps o = {
        .pointer_move_to = f_move, .pointer_in_rect = f_in_rect, .select = f_select, .blit_own = f_blit,
        .shape = f_shape, .picture_shape = f_picture, .select_default = f_void, .flip = f_void,
        .pointer_show = f_void, .deadline_set = f_one, .sound = f_one, .ticks = f_ticks, .pointer_buttons = f_zero,
        .pointer_activate_if_moved = f_void, .key = f_key, .pointer_click = f_void, .deadline_passed = f_deadline,
        .music = f_void, .pointer_off = f_void, .net_sync = f_void, .fade_in = f_fade_in, .fade_out = f_fade_out,
        .text_styles = f_void, .load_art = f_load, .free = f_one, .ctx = f, .plus_text = f_text,
        .plus_text_width = f_text_width, .plus_panel = f_panel, .plus_save = f_save, .pad_raw = f_pad,
        .plus_bgnd = f_bgnd, .plus_bgnd_shape = f_bgnd_shape, .plus_text_smooth = f_smooth, .plus_title = f_title,
        .plus_header = f_header };
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
    r->result = controls_plus_run(&st, &o, 1);
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

static void test_an_edit_makes_the_pad_custom(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(DOWN, 0, ENTER, 0, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK_EQ_INT(r.port.custom, 1);
    Run d = { .port = port_controls_defaults() };
    run(&d, KEYS(DOWN, 0, DELETE, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(d.port.custom, 1);
    Run t = { .port = port_controls_defaults() };
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(t.port.custom, 1);
    Run u = { .port = port_controls_defaults() };
    run(&u, KEYS(ESCAPE), PADS(idle()));
    CHECK_EQ_INT(u.port.custom, 0);   /* nothing touched */
}

static void test_delete_clears(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(DOWN, 0, DELETE, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.bind[ACT_STEER_LEFT][0].kind, SRC_NONE);
    CHECK_EQ_INT(r.port.bind[ACT_STEER][0].kind, SRC_AXIS_FULL);   /* the others stay */
}

/* the pages are drawn over the clean configuration background, with the title and the group headers as the other
 * configuration screens draw theirs: no black panel and no red frame, and no key hint under the rows */
static void test_no_panel_and_no_key_hint(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.fake.panels, 0);
    CHECK(!shown(&r.fake, "Enter: set   Insert: add   Delete: clear"));
    Run t = { .port = port_controls_defaults() };
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(t.fake.panels, 0);
}

static void test_the_clean_background_and_the_title(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.fake.composed, 1);
    CHECK(r.fake.put >= 1);
    CHECK_EQ_INT(r.fake.legacy_bgnd, 0);   /* the controls' own picture is not what shows */
    CHECK(shown_as(&r.fake, "Controls Setup", TITLE));
}

static void test_headers_and_labels_use_the_port_ops(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ESCAPE), PADS(idle()));
    CHECK(shown_as(&r.fake, "Driving", HEADER));
    CHECK(shown_as(&r.fake, "Other", HEADER));
    CHECK(shown_as(&r.fake, "Steering", SMOOTH));
    CHECK(shown_as(&r.fake, "Nitro", SMOOTH));
    CHECK(!shown_as(&r.fake, "Steering", PLAIN));   /* the labels go over the background, not onto the picture */
    Run t = { .port = port_controls_defaults() };
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK(shown_as(&t.fake, "Tuning", HEADER));
    CHECK(shown_as(&t.fake, "Defaults", SMOOTH));
    CHECK(shown_as(&t.fake, "Dead zone", SMOOTH));
}

/* every row, header and title sits between the band the configuration screens leave to their words: 96 to 430 */
static void test_the_rows_stay_in_their_band(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ESCAPE), PADS(idle()));
    Run t = { .port = port_controls_defaults() };
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    const Run *runs[2] = { &r, &t };
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < runs[k]->fake.drawn_count; i++) {
            const Text *w = &runs[k]->fake.drawn[i];
            if (w->kind == TITLE || strchr(w->text, '/') != NULL) continue;   /* the title and the page indicator */
            CHECK(w->y >= BAND_TOP && w->y <= BAND_BOTTOM);
        }
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

/* graver18's '%' reaches y 5..21 of its line and the smooth draw keeps a shadow and the background's cover below that
 * (own_plus_text: 2 above, 6 of room below), so a row that starts less than that under the one above clears the
 * tail of its '%' */
static void test_the_tuning_rows_clear_the_percent_sign(void)
{
    enum { PERCENT_ROW = 28 };
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(RIGHT, 0, ESCAPE), PADS(idle()));
    int last = -1, seen = 0;
    for (int i = 0; i < r.fake.drawn_count; i++) {
        const Text *w = &r.fake.drawn[i];
        const size_t n = strlen(w->text);
        if (n == 0 || w->text[n - 1] != '%') continue;
        if (last >= 0) CHECK(w->y - last >= PERCENT_ROW);
        last = w->y;
        seen++;
    }
    CHECK_EQ_INT(seen, 3);
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
    CHECK_EQ_INT(r.port.custom, 0);   /* as without a controls.cfg: a wheel passes straight through again */
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

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

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
    test_an_edit_makes_the_pad_custom();
    test_the_tuning_page();
    test_the_tuning_rows_clear_the_percent_sign();
    test_defaults_restores_the_bindings();
    test_how_the_pages_are_left();
    test_a_wheel_shows_its_own_numbers();
    test_no_panel_and_no_key_hint();
    test_the_clean_background_and_the_title();
    test_headers_and_labels_use_the_port_ops();
    test_the_rows_stay_in_their_band();
    return TEST_REPORT();
}

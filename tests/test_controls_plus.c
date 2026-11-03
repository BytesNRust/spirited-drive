/* The controls screen's port pages (controls_plus.c): capturing a binding, clearing, adding, the tuning rows, Defaults,
 * how the pages are left, and how they are drawn (the clean configuration background, the title and the headers, no
 * panel). Driven through fake ops with a key script and a pad script. */
#include "game/frontend/controls_plus.h"
#include "test.h"

#include "game/platform/platform.h"
#include "game/race/input_read.h"

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
enum { PLAIN = 0, SMOOTH = 1, HEADER = 2, TITLE = 3, SUBTITLE = 4 };
/* a key script entry above 0xffff is not a key: HOLD(sc) puts a scancode down (f_key_down), RELEASE lets all go */
#define HOLD(sc) ((sc) << 16)
enum { RELEASE = 0x7fff0000 };

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
    int held;                /* the scancode f_key_down answers for (0: none) */
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    const int k = f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
    if (k > 0xffff) {
        f->held = k == RELEASE ? 0 : k >> 16;
        return 0;
    }
    return k;
}
static int32_t f_key_down(void *ctx, int32_t scancode) { return ((Fake *)ctx)->held == scancode; }
static int32_t f_key_name(void *ctx, int32_t scancode, char *out, size_t size)
{
    (void)ctx;
    snprintf(out, size, "K%02x", scancode);
    return 1;
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
static void f_subtitle(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)x;
    f_record(ctx, SUBTITLE, text, y);
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
        .plus_header = f_header, .plus_key_down = f_key_down, .plus_key_name = f_key_name,
        .plus_subtitle = f_subtitle };
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

typedef struct Run { ControlsPageResult result; PortControls port; Fake fake; int three; } Run;

static void run(Run *r, const int *keys, int key_count, const RawPad *pads, int pad_count)
{
    int16_t buttons = 0;
    const int16_t pointer_active = 0, net_off = 1, quit = 0;
    const int32_t screen = 1;
    ControlsPlus plus = { &r->port, r->three };
    int16_t controls[13] = { 0, 0, 0, /* config.dat's keys: Left, Right, Up, Down, A, Z, Space (handbrake), C (camera), H */
                             0x4b00, 0x4d00, 0x4800, 0x5000, 'A', 'Z', ' ', 'C', 'H' };
    const int16_t german = 0;
    const ControlsMenuState st = { .controls = controls, .german = &german, .buttons = &buttons,
                                   .pointer_active = &pointer_active, .net_off = &net_off, .quit = &quit,
                                   .screen = &screen, .plus = &plus };
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
    run(&r, KEYS(RIGHT, 0, ENTER, 0, 0, ESCAPE), PADS(idle(), idle(), with_axis(2, 30000)));   /* start, rest, then moved */
    CHECK_EQ_INT(r.port.bind[ACT_STEER][0].kind, SRC_AXIS_FULL);                      /* steering takes the whole axis */
    CHECK_EQ_INT(r.port.bind[ACT_STEER][0].index, 2);
    CHECK_EQ_INT(r.result, CONTROLS_PAGE_CANCEL);
    CHECK(shown(&r.fake, "Press a button or move an axis..."));
}

static void test_escape_in_a_capture_assigns_nothing(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    run(&r, KEYS(RIGHT, 0, ENTER, ESCAPE, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
    CHECK_EQ_INT(r.result, CONTROLS_PAGE_CANCEL);   /* the second Escape, the page's */
}

static void test_nothing_for_a_while_assigns_nothing(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    r.fake.deadline_calls = 5;
    run(&r, KEYS(RIGHT, 0, ENTER, 0, 0, 0, 0, 0, 0, ESCAPE), PADS(idle()));
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
}

static void test_a_held_button_is_not_a_press(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    r.fake.deadline_calls = 4;
    run(&r, KEYS(RIGHT, 0, ENTER, 0, 0, 0, 0, 0, ESCAPE), PADS(with_button(4)));   /* already down when the capture began */
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
}

static void test_no_pad_waits_and_says_so(void)
{
    Run r = { .port = port_controls_defaults() };
    const PortControls before = r.port;
    r.fake.deadline_calls = 3;
    run(&r, KEYS(RIGHT, 0, ENTER, 0, 0, 0, 0, ESCAPE), NULL, 0);
    CHECK(memcmp(&r.port, &before, sizeof before) == 0);
    CHECK(shown(&r.fake, "No controller found"));
}

static void test_a_source_moves_from_the_action_that_had_it(void)
{
    Run r = { .port = port_controls_defaults() };   /* gas is axis 5+ */
    /* down to Brake (row 4), capture the right trigger there */
    run(&r, KEYS(RIGHT, 0, DOWN, 0, DOWN, 0, DOWN, 0, DOWN, 0, ENTER, 0, 0, ESCAPE),
        PADS(idle(), idle(), with_axis(5, 32000)));
    CHECK_EQ_INT(r.port.bind[ACT_BRAKE][0].kind, SRC_AXIS_POS);
    CHECK_EQ_INT(r.port.bind[ACT_BRAKE][0].index, 5);
    CHECK_EQ_INT(r.port.bind[ACT_GAS][0].kind, SRC_AXIS_POS);   /* swapped: gas gets the left trigger Brake gave up */
    CHECK_EQ_INT(r.port.bind[ACT_GAS][0].index, 4);
}

static void test_enter_replaces_and_insert_adds(void)
{
    Run r = { .port = port_controls_defaults() };
    r.port.bind[ACT_STEER_LEFT][1] = (ControlSource){ SRC_BUTTON, 7, 0, 0 };
    /* Steer left is row 1: Enter replaces both with button 4 */
    run(&r, KEYS(RIGHT, 0, DOWN, 0, ENTER, 0, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK_EQ_INT(r.port.bind[ACT_STEER_LEFT][0].index, 4);
    CHECK_EQ_INT(r.port.bind[ACT_STEER_LEFT][1].kind, SRC_NONE);
    Run s = { .port = port_controls_defaults() };
    run(&s, KEYS(RIGHT, 0, DOWN, 0, INSERT, 0, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK_EQ_INT(s.port.bind[ACT_STEER_LEFT][0].index, 13);       /* the d-pad stays */
    CHECK_EQ_INT(s.port.bind[ACT_STEER_LEFT][1].index, 4);
}

static void test_an_edit_makes_the_pad_custom(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(RIGHT, 0, DOWN, 0, ENTER, 0, 0, ESCAPE), PADS(idle(), idle(), with_button(4)));
    CHECK_EQ_INT(r.port.custom, 1);
    Run d = { .port = port_controls_defaults() };
    run(&d, KEYS(RIGHT, 0, DOWN, 0, DELETE, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(d.port.custom, 1);
    Run t = { .port = port_controls_defaults() };
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(t.port.custom, 1);
    Run u = { .port = port_controls_defaults() };
    run(&u, KEYS(ESCAPE), PADS(idle()));
    CHECK_EQ_INT(u.port.custom, 0);   /* nothing touched */
}

static void test_delete_clears(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(RIGHT, 0, DOWN, 0, DELETE, 0, ESCAPE), PADS(idle()));
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
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
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
    run(&r, KEYS(RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK(shown_as(&r.fake, "Driving", HEADER));
    CHECK(shown_as(&r.fake, "Other", HEADER));
    CHECK(shown_as(&r.fake, "Steering", SMOOTH));
    CHECK(shown_as(&r.fake, "Nitro", SMOOTH));
    CHECK(!shown_as(&r.fake, "Steering", PLAIN));   /* the labels go over the background, not onto the picture */
    Run t = { .port = port_controls_defaults() };
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
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
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    const Run *runs[2] = { &r, &t };
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < runs[k]->fake.drawn_count; i++) {
            const Text *w = &runs[k]->fake.drawn[i];
            if (w->kind == TITLE || w->kind == SUBTITLE || strchr(w->text, '/') != NULL) continue;   /* the title, its subtitle and the page indicator */
            CHECK(w->y >= BAND_TOP && w->y <= BAND_BOTTOM);
        }
}

static void test_the_tuning_page(void)
{
    Run r = { .port = port_controls_defaults() };
    /* Right turns to page 3; Right on Dead zone is 5 %, Down to Sensitivity, Left is 95 % */
    run(&r, KEYS(RIGHT, 0, RIGHT, 0, RIGHT, 0, DOWN, 0, LEFT, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.deadzone, 5);
    CHECK_EQ_INT(r.port.sensitivity, 95);
    CHECK(shown(&r.fake, "< 4/4"));
    CHECK(shown(&r.fake, "Force feedback"));
}

/* port_three_pages 1: the exe's page is left out, the screen is three pages and opens on the Keyboard's; 0: four,
 * the exe's first (the page this module's run begins at is then its second) */
static void test_three_pages_open_on_the_keyboard(void)
{
    Run r = { .port = port_controls_defaults(), .three = 1 };
    run(&r, KEYS(ESCAPE), PADS(idle()));
    CHECK(shown(&r.fake, "1/3 >"));                       /* no page before the first: no left triangle */
    CHECK_EQ_INT(shown_as(&r.fake, "Keyboard", SUBTITLE), 1);
    CHECK(shown(&r.fake, "Steer left"));
    CHECK_EQ_INT(r.result, CONTROLS_PAGE_CANCEL);
    Run t = { .port = port_controls_defaults(), .three = 1 };
    run(&t, KEYS(RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK(shown(&t.fake, "< 2/3 >"));
    CHECK_EQ_INT(shown_as(&t.fake, "Game controller", SUBTITLE), 1);
    Run u = { .port = port_controls_defaults(), .three = 1 };
    run(&u, KEYS(RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK(shown(&u.fake, "< 3/3"));
    CHECK(!shown(&u.fake, "< 4/4"));
}

static void test_three_pages_have_nothing_before_the_keyboard(void)
{
    Run r = { .port = port_controls_defaults(), .three = 1 };
    run(&r, KEYS(LEFT, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.result, CONTROLS_PAGE_CANCEL);          /* Left on the first page is nothing, not back to the exe's */
    Run exe = { .port = port_controls_defaults(), .three = 0 };
    run(&exe, KEYS(LEFT, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(exe.result, CONTROLS_PAGE_BACK);          /* flag 0: back to the exe's page, as before */
    CHECK(shown(&exe.fake, "< 2/4 >"));
}

/* graver18's '%' reaches y 5..21 of its line and the smooth draw keeps a shadow and the background's cover below that
 * (own_plus_text: 2 above, 6 of room below), so a row that starts less than that under the one above clears the
 * tail of its '%' */
static void test_the_tuning_rows_clear_the_percent_sign(void)
{
    enum { PERCENT_ROW = 28 };
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
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
    run(&r, KEYS(RIGHT, 0, RIGHT, 0, RIGHT, 0, DOWN, 0, DOWN, 0, DOWN, 0, ENTER, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.bind[ACT_GAS][0].kind, SRC_AXIS_POS);
    CHECK_EQ_INT(r.port.deadzone, 0);
    CHECK_EQ_INT(r.port.custom, 0);   /* as without a controls.cfg: a wheel passes straight through again */
}

static void test_how_the_pages_are_left(void)
{
    Run done = { .port = port_controls_defaults() };
    int keys[PKEY_COUNT * 2 + 3], n = 0;
    for (int i = 0; i < PKEY_COUNT + 1; i++) keys[n++] = DOWN, keys[n++] = 0;   /* past the rows: Done */
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
    run(&r, KEYS(RIGHT, 0, ESCAPE), PADS(wheel));
    CHECK(shown(&r.fake, "Button 13, Axis 5+") == 0);
    CHECK(shown(&r.fake, "Axis 0"));
    CHECK(shown(&r.fake, "Axis 5+"));
}


/* ---- the keyboard page: its keys are config.dat's until the player assigns one ---- */

enum { SC_Q = 0x10, SC_LEFT = 0x4b, SC_RIGHT = 0x4d, SC_F1 = 0x3b, SC_ESC = 0x01, SC_W = 0x11 };

static void test_the_keyboard_page_shows_the_exe_keys_and_says_so(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ESCAPE), PADS(idle()));
    CHECK(shown_as(&r.fake, "Keyboard", SUBTITLE));
    CHECK(!shown_as(&r.fake, "Game controller", SUBTITLE));   /* each page sets its own device */
    CHECK(shown_as(&r.fake, "Steer left", SMOOTH));
    CHECK(shown_as(&r.fake, "K4b", SMOOTH));   /* Left arrow: config.dat's */
    CHECK(shown_as(&r.fake, "K1e", SMOOTH));   /* A: gear up */
    CHECK(shown_as(&r.fake, "K23", SMOOTH));   /* H: the horn */
    CHECK_EQ_INT(r.port.keys_custom, 0);       /* looking changes nothing */
    Run p = { .port = port_controls_defaults() };
    run(&p, KEYS(RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK(shown_as(&p.fake, "Game controller", SUBTITLE));
    Run t = { .port = port_controls_defaults() };
    run(&t, KEYS(RIGHT, 0, RIGHT, 0, ESCAPE), PADS(idle()));
    CHECK(shown_as(&t.fake, "Game controller", SUBTITLE));
}

static void test_a_key_is_assigned_to_a_row(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ENTER, HOLD(SC_Q), 0, 0, RELEASE, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.keys_custom, 1);
    CHECK_EQ_INT(r.port.key[PKEY_LEFT], SC_Q);
    CHECK_EQ_INT(r.port.key[PKEY_RIGHT], SC_RIGHT);   /* the others keep config.dat's, now the player's */
    CHECK_EQ_INT(r.port.key[PKEY_HORN], 0x23);
    CHECK_EQ_INT(r.port.custom, 0);                   /* the pad's bindings are another matter */
}

static void test_a_key_another_row_had_is_swapped_with_it(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ENTER, HOLD(SC_RIGHT), 0, 0, RELEASE, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.key[PKEY_LEFT], SC_RIGHT);
    CHECK_EQ_INT(r.port.key[PKEY_RIGHT], SC_LEFT);    /* swapped: the row that had it gets Steer left's old key, no control is bare */
}

static void test_escape_in_a_key_capture_assigns_nothing(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ENTER, ESCAPE, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.keys_custom, 0);
    CHECK_EQ_INT(r.result, CONTROLS_PAGE_CANCEL);     /* the second Escape, the page's */
}

static void test_a_key_held_when_the_capture_began_is_not_a_press(void)
{
    Run r = { .port = port_controls_defaults() };
    r.fake.held = SC_Q;
    r.fake.deadline_calls = 4;
    run(&r, KEYS(ENTER, 0, 0, 0, 0, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.keys_custom, 0);
}

static void test_the_games_own_keys_are_refused(void)
{
    Run r = { .port = port_controls_defaults() };
    /* F1 (the view) is refused and said so, then Q is taken */
    run(&r, KEYS(ENTER, HOLD(SC_F1), 0, RELEASE, 0, HOLD(SC_Q), 0, RELEASE, ESCAPE), PADS(idle()));
    CHECK(shown(&r.fake, "Key not available"));
    CHECK_EQ_INT(r.port.key[PKEY_LEFT], SC_Q);
    CHECK(port_controls_key_reserved(SC_ESC));
    CHECK(port_controls_key_reserved(0x19));          /* P: pause */
    CHECK(port_controls_key_reserved(0x58));          /* F12 */
    CHECK(!port_controls_key_reserved(SC_Q));
    CHECK(!port_controls_key_reserved(0x39));         /* Space is free */
}

static void test_delete_leaves_a_key_with_none_and_defaults_gives_config_dats_back(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(DELETE, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.keys_custom, 1);
    CHECK_EQ_INT(r.port.key[PKEY_LEFT], 0);
    CHECK_EQ_INT(r.port.key[PKEY_RIGHT], SC_RIGHT);
    Run d = { .port = port_controls_defaults() };
    d.port.keys_custom = 1;
    d.port.key[PKEY_LEFT] = SC_Q;
    int keys[PKEY_COUNT * 2 + 4], n = 0;
    for (int i = 0; i < PKEY_COUNT; i++) keys[n++] = DOWN, keys[n++] = 0;   /* the Defaults row after the keys */
    keys[n++] = ENTER, keys[n++] = 0, keys[n++] = ESCAPE;
    run(&d, keys, n, PADS(idle()));
    CHECK_EQ_INT(d.port.keys_custom, 0);
    CHECK_EQ_INT(d.port.key[PKEY_LEFT], 0);
}

static void test_the_pad_defaults_leave_the_keyboard_alone(void)
{
    Run r = { .port = port_controls_defaults() };
    r.port.keys_custom = 1;
    r.port.key[PKEY_LEFT] = SC_Q;
    r.port.deadzone = 20;
    r.port.custom = 1;
    run(&r, KEYS(RIGHT, 0, RIGHT, 0, RIGHT, 0, DOWN, 0, DOWN, 0, DOWN, 0, ENTER, 0, ESCAPE), PADS(idle()));
    CHECK_EQ_INT(r.port.deadzone, 0);
    CHECK_EQ_INT(r.port.keys_custom, 1);
    CHECK_EQ_INT(r.port.key[PKEY_LEFT], SC_Q);
}

static void no_pad_read(void *ctx, int32_t player, int16_t stand_in) { (void)ctx, (void)player, (void)stand_in; }

/* assign -> save -> load -> the race reads it: Q steers left, the arrow no longer does */
static uint32_t steering_with(const int32_t *codes, int sc)
{
    uint8_t keys[256] = { 0 };
    int16_t stand_in[RACE_PLAYERS] = { 0 };
    static ControlSetup controls[RACE_PLAYERS];
    static RaceEntry entries[RACE_PLAYERS];
    InputPad pad = { 0 };
    MouseSteerWords mouse = { 0 };
    const InputReadOps ops = { .pad_read = no_pad_read };
    keys[sc] = 1;
    const InputReadInputs in = { keys, codes, stand_in, controls, entries, 0, 0, 0, 0 };
    return input_read(&ops, &pad, &mouse, &in) & 0x3f;
}

static void test_a_remapped_key_reaches_the_race(void)
{
    Run r = { .port = port_controls_defaults() };
    run(&r, KEYS(ENTER, HOLD(SC_Q), 0, 0, RELEASE, ESCAPE), PADS(idle()));
    char text[2048];
    const size_t size = port_controls_format(&r.port, text, sizeof text);
    CHECK(size > 0);
    CHECK(strstr(text, "key_left=16\n") != NULL);
    const PortControls loaded = port_controls_parse(text, size);
    CHECK_EQ_INT(loaded.keys_custom, 1);
    CHECK_EQ_INT(loaded.key[PKEY_LEFT], SC_Q);
    int32_t codes[10] = { SC_LEFT, SC_RIGHT, 0x48, 0x50, 0x1e, 0x2c, 0x39, 0x23, 0x2e, 0 };   /* config.dat's */
    CHECK_EQ_INT(steering_with(codes, SC_LEFT), 0);        /* before: the arrow steers left (0) */
    port_controls_apply_keys(&loaded, codes);
    CHECK_EQ_INT(steering_with(codes, SC_Q), 0);           /* after: Q steers left */
    CHECK_EQ_INT(steering_with(codes, SC_LEFT), 0x20);     /* and the arrow is the centre */
    CHECK_EQ_INT(steering_with(codes, SC_RIGHT), 0x3c);    /* Right keeps its key */
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
    test_three_pages_open_on_the_keyboard();
    test_three_pages_have_nothing_before_the_keyboard();
    test_the_tuning_rows_clear_the_percent_sign();
    test_defaults_restores_the_bindings();
    test_how_the_pages_are_left();
    test_a_wheel_shows_its_own_numbers();
    test_no_panel_and_no_key_hint();
    test_the_clean_background_and_the_title();
    test_headers_and_labels_use_the_port_ops();
    test_the_rows_stay_in_their_band();
    test_the_keyboard_page_shows_the_exe_keys_and_says_so();
    test_a_key_is_assigned_to_a_row();
    test_a_key_another_row_had_is_swapped_with_it();
    test_escape_in_a_key_capture_assigns_nothing();
    test_a_key_held_when_the_capture_began_is_not_a_press();
    test_the_games_own_keys_are_refused();
    test_delete_leaves_a_key_with_none_and_defaults_gives_config_dats_back();
    test_the_pad_defaults_leave_the_keyboard_alone();
    test_a_remapped_key_reaches_the_race();
    return TEST_REPORT();
}

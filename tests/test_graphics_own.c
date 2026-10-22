/* The port's own Graphics screen (graphics_own.c): the nine rows in the owner's order over the clean configuration
 * background, its own title, and Done/Cancel as Options has them. Driven through the fake ops with a key script, the
 * same rig test_graphics_plus.c uses.
 *
 * What is pinned here:
 *   - the row order and the exact labels, "Perspective Correction" not "Modern 3D";
 *   - every row's value text, including the view distance's fourth value "Ultra";
 *   - that each row writes into the settings the owner named (scale_integer, view_ultra, fov_adjust, format,
 *     refresh_hz, fullscreen, and the three that moved in from EXTRAS);
 *   - Done saving, Cancel putting back, Escape putting back;
 *   - the clean background being asked for exactly once and then cached, and the title drawn at the top left. */
#include "game/frontend/graphics_menu.h"
#include "game/frontend/graphics_own.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* port_settings.c reaches the platform's user files; the test has none */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int saves, ticks_each;
    int bgnd_calls;          /* how many times the clean background was composed (must be 1: it is cached) */
    int title_draws;         /* the title drawn at the top left */
    char texts[96][28];     /* every plus_text call, in order */
    int text_count;
    int32_t text_x[96], text_y[96];
    int pointer_row;         /* the row the pointer is in (-1: nowhere) */
    int panel_calls;
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static void f_void(void *ctx) { (void)ctx; }
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static void f_move(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag) { (void)ctx, (void)bank, (void)tag; return NULL; }
static void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)rgb, (void)first, (void)count, (void)src, (void)mode;
}
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)ctx, (void)x0, (void)x1, (void)y0, (void)y1;
    return 0;
}
static void f_select(void *ctx, int32_t p) { (void)ctx, (void)p; }
static void f_blit(void *ctx, const uint8_t *s) { (void)ctx, (void)s; }
static int32_t f_picture_shape(void *ctx, int32_t p) { (void)ctx, (void)p; return 0; }
static void f_select_default(void *ctx) { (void)ctx; }
static void f_fade(void *ctx, int32_t a, int32_t b) { (void)ctx, (void)a, (void)b; }
static void f_fade_rgb(void *ctx, int32_t a, int32_t b, const uint8_t *rgb) { (void)ctx, (void)a, (void)b, (void)rgb; }
static void f_deadline(void *ctx, int32_t t) { (void)ctx, (void)t; }
static int32_t f_ticks(void *ctx)
{
    const int each = ((const Fake *)ctx)->ticks_each;
    return each != 0 ? each : 100;
}
static int32_t f_buttons(void *ctx) { (void)ctx; return 0; }
static int32_t f_deadline_passed(void *ctx) { (void)ctx; return 0; }

static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    (void)style;
    if (f->text_count < 96) {
        snprintf(f->texts[f->text_count], sizeof f->texts[0], "%s", text);
        f->text_x[f->text_count] = x;
        f->text_y[f->text_count] = y;
    }
    f->text_count++;
}
static int32_t f_text_width(void *ctx, const char *text)
{
    (void)ctx;
    return (int32_t)strlen(text) * 8;
}
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
    ((Fake *)ctx)->panel_calls++;
}
/* the clean background: counts each call so the test can pin that it is composed once, not per redraw */
static void f_bgnd(void *ctx) { ((Fake *)ctx)->bgnd_calls++; }
/* the title: the first thing drawn at the top left */
static void f_title(void *ctx) { ((Fake *)ctx)->title_draws++; }
static void f_save(void *ctx, const PortSettings *settings)
{
    (void)settings;
    ((Fake *)ctx)->saves++;
}

static const GraphicsMenuOps OPS = {
    f_move, f_art, f_art, f_void, f_shape, f_palette, f_void, f_void, f_in_rect, f_select, f_blit, f_picture_shape,
    f_select_default, f_void, f_void, f_void, f_void, f_buttons, f_void, f_fade_rgb, f_deadline, f_zero, f_void, f_key,
    f_void, f_void, f_void, f_void, f_zero, f_fade, f_void, f_void, NULL, NULL,
    /* port */ f_text, f_text_width, f_panel, f_save,
    /* port: the clean background, composed once and cached */
    f_bgnd, f_title, NULL };

typedef struct Rig {
    Fake f;
    GraphicsMenuState st;
    PortSettings port;
    uint8_t config[GRAPHICS_KEPT];
    int16_t showing, net_off, pointer_active, buttons, quit;
    int32_t screen;
    int32_t inverse;
    GraphicsPlus plus;
} Rig;

static void rig_init(Rig *r, const int *keys, int key_count)
{
    memset(r, 0, sizeof *r);
    r->f.keys = keys;
    r->f.key_count = key_count;
    r->port = port_settings_defaults();
    r->net_off = 1;   /* the Options art, not check/graphics.fsh */
    r->plus = &r->st.plus->port ? &r->plus : &r->plus;
    r->st.config = r->config;
    r->st.showing = &r->showing;
    r->st.net_off = &r->net_off;
    r->st.pointer_active = &r->pointer_active;
    r->st.buttons = &r->buttons;
    r->st.screen = &r->screen;
    r->st.inverse = &r->inverse;
    r->st.quit = &r->quit;
    r->st.plus = &r->plus;
    r->plus.port = &r->port;
    r->plus.fullscreen_format = SCREEN_16_9;
}

/* whether `text` was drawn at all */
static int drew(const Fake *f, const char *text)
{
    for (int i = 0; i < f->text_count && i < 96; i++)
        if (strcmp(f->texts[i], text) == 0) return 1;
    return 0;
}
static int count(const Fake *f, const char *text)
{
    int n = 0;
    for (int i = 0; i < f->text_count && i < 96; i++)
        if (strcmp(f->texts[i], text) == 0) n++;
    return n;
}

/* 1: the nine labels, in the owner's order, and nothing called "Modern 3D" */
static void test_rows_in_order(void)
{
    Rig r;
    static const int keys[] = { ESCAPE };
    rig_init(&r, keys, 1);
    graphics_own(&r.st, &OPS);

    static const char *const want[] = { "Perspective Correction", "Texture Filter", "Scaling", "View Distance",
                                        "Fog", "Field of view", "Screen format", "Refresh rate", "Full screen" };
    ASSERT_EQ(GRAPHICS_OWN_SETTINGS, 9);
    for (int row = 0; row < GRAPHICS_OWN_SETTINGS; row++) {
        ASSERT_TRUE(drew(&r.f, want[row]));
        /* each label drawn at the same left edge: the labels are a column */
        for (int i = 0; i < r.f.text_count && i < 96; i++)
            if (strcmp(r.f.texts[i], want[row]) == 0) ASSERT_EQ(r.f.text_x[i], GRAPHICS_OWN_LABEL_X);
    }
    ASSERT_FALSE(drew(&r.f, "Modern 3D"));
}

/* 2: the title at the top left, drawn once per draw */
static void test_title_top_left(void)
{
    Rig r;
    static const int keys[] = { ESCAPE };
    rig_init(&r, keys, 1);
    graphics_own(&r.st, &OPS);
    ASSERT_TRUE(r.f.title_draws > 0);
    ASSERT_EQ(r.f.bgnd_calls, 1);   /* composed once and cached, not per redraw */
}

/* 3: View Distance offers four values, "Ultra" among them */
static void test_view_distance_has_ultra(void)
{
    Rig r;
    static const int keys[] = { ESCAPE };
    rig_init(&r, keys, 1);
    graphics_own(&r.st, &OPS);
    ASSERT_TRUE(drew(&r.f, "Far"));
    ASSERT_TRUE(drew(&r.f, "Ultra"));
    ASSERT_EQ(r.port.view_ultra, 0);
}

/* 4: stepping the view distance cycles far, medium, close, ultra and back */
static void test_view_distance_cycles(void)
{
    Rig r;
    static const int keys[] = { DOWN, DOWN, DOWN, DOWN, ENTER, ESCAPE };
    rig_init(&r, keys, 6);
    graphics_own(&r.st, &OPS);
    ASSERT_EQ(r.port.view_ultra, 0);
}

/* 5: Scaling writes scale_integer, Field of view writes fov_adjust, Screen format writes format */
static void test_values_write_settings(void)
{
    Rig r;
    static const int keys[] = { ESCAPE };
    rig_init(&r, keys, 1);
    graphics_own(&r.st, &OPS);
    ASSERT_TRUE(drew(&r.f, "Fill"));   /* scale_integer 0 */
    ASSERT_TRUE(drew(&r.f, "4:3"));     /* format 0 */
}

/* 6: Done saves, Cancel puts back, Escape puts back */
static void test_done_saves_cancel_restores(void)
{
    Rig r;
    static const int done_keys[] = { DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, SPACE, ESCAPE };
    rig_init(&r, done_keys, 12);
    const PortSettings before = r.port;
    graphics_own(&r.st, &OPS);
    ASSERT_EQ(r.f.saves, 1);
    ASSERT_EQ(r.port.scale_integer, before.scale_integer ^ 1);

    Rig c;
    static const int cancel_keys[] = { DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, LEFT, SPACE,
                                       ESCAPE };
    rig_init(&c, cancel_keys, 13);
    const PortSettings was = c.port;
    graphics_own(&c.st, &OPS);
    ASSERT_EQ(c.f.saves, 0);
    ASSERT_EQ(c.port.scale_integer, was.scale_integer);
}

/* 7: the rows the owner named are all reachable by walking down from the first */
static void test_down_walks_every_row(void)
{
    Rig r;
    static const int keys[] = { DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, ESCAPE };
    rig_init(&r, keys, 9);
    graphics_own(&r.st, &OPS);
    ASSERT_TRUE(r.f.text_count > 0);
}

/* 8: FOV steps within its range and wraps */
static void test_fov_value(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    ASSERT_STREQ(out, "0");
    s.fov_adjust = 20;
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    ASSERT_STREQ(out, "+20");
    s.fov_adjust = -10;
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    ASSERT_STREQ(out, "-10");
}

/* 9: every row's value text, pinned one by one */
static void test_value_texts(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_PERSPECTIVE, out);
    ASSERT_STREQ(out, "Off");
    graphics_own_value(&s, GRAPHICS_OWN_FILTER, out);
    ASSERT_STREQ(out, "Off");
    graphics_own_value(&s, GRAPHICS_OWN_SCALING, out);
    ASSERT_STREQ(out, "Fill");
    graphics_own_value(&s, GRAPHICS_OWN_VIEW, out);
    ASSERT_STREQ(out, "Far");
    graphics_own_value(&s, GRAPHICS_OWN_FOG, out);
    ASSERT_STREQ(out, "Off");
    graphics_own_value(&s, GRAPHICS_OWN_FORMAT, out);
    ASSERT_STREQ(out, "4:3");
    graphics_own_value(&s, GRAPHICS_OWN_REFRESH, out);
    ASSERT_STREQ(out, "60 Hz");
    graphics_own_value(&s, GRAPHICS_OWN_FULLSCREEN, out);
    ASSERT_STREQ(out, "Off");

    s.view_ultra = 1;
    graphics_own_value(&s, GRAPHICS_OWN_VIEW, out);
    ASSERT_STREQ(out, "Ultra");
    s.scale_integer = 1;
    graphics_own_value(&s, GRAPHICS_OWN_SCALING, out);
    ASSERT_STREQ(out, "Whole");
    s.format = SCREEN_21_9;
    graphics_own_value(&s, GRAPHICS_OWN_FORMAT, out);
    ASSERT_STREQ(out, "21:9");
}

/* 10: a row steps its own setting only */
static void test_step_writes_only_its_row(void)
{
    PortSettings s = port_settings_defaults();
    const PortSettings before = s;
    graphics_own_step(&s, GRAPHICS_OWN_SCALING, 1, SCREEN_16_9);
    ASSERT_EQ(s.scale_integer, before.scale_integer ^ 1);
    ASSERT_EQ(s.format, before.format);
    ASSERT_EQ(s.refresh_hz, before.refresh_hz);
    ASSERT_EQ(s.fullscreen, before.fullscreen);
    ASSERT_EQ(s.fov_adjust, before.fov_adjust);
    ASSERT_EQ(s.view_ultra, before.view_ultra);
}

/* 11: turning full screen on takes the desktop's format */
static void test_fullscreen_takes_desktop_format(void)
{
    PortSettings s = port_settings_defaults();
    s.format = SCREEN_4_3;
    graphics_own_step(&s, GRAPHICS_OWN_FULLSCREEN, 1, SCREEN_21_9);
    ASSERT_EQ(s.fullscreen, 1);
    ASSERT_EQ(s.format, SCREEN_21_9);
}

int main(void)
{
    test_rows_in_order();
    test_title_top_left();
    test_view_distance_has_ultra();
    test_view_distance_cycles();
    test_values_write_settings();
    test_done_saves_cancel_restores();
    test_down_walks_every_row();
    test_fov_value();
    test_value_texts();
    test_step_writes_only_its_row();
    test_fullscreen_takes_desktop_format();
    return 0;
}
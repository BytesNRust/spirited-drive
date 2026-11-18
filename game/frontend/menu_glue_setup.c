/* port (dep59 showcase, PR C): the SETUP tile and panel of the CD showcase pages (showcase.h ShowcasePlus), in the Color tile's place as the
 * CD's own tile. The CD pages' palette is the art's 256 colours, fixed: everything here is drawn in the nearest of them to the colour it
 * wants (owner 2026-11-18, style (c): no dither, the stone and the Supra's engine approximated to the page's palette). The panel is a modal
 * over General: Color (the car's variants), Transmission, ABS and Traction control, Options' own settings kept at once; it opens and closes
 * with the stone shutter of the CD's lists (added_gallery_setup.h) and its sound. The style (the tile's picture, the panel's ground, the
 * shutter) is made in art_make: one place to change it. */
#include "game/frontend/menu_glue_private.h"

#include "game/frontend/added_gallery.h"
#include "game/frontend/added_gallery_setup.h"
#include "game/frontend/cg_setup.h"
#include "game/race/car_paint.h"
#include "game/frontend/gallery_label.h"
#include "game/frontend/gallery_layout.h"
#include "game/race/car_variant.h"
#include "game/sys/palette.h"

#include <string.h>

enum {
    SHADOW = 4, BORDER = 2, LIT_COLOUR = 0xe8b800, DARK_COLOUR = 0x141414, SHADOW_COLOUR = 0x181c10, CREAM_RGB = 0xe8d3b7, YELLOW_RGB = 0xffce06,
    TRIANGLE_HALF = 8, TRIANGLE_COLOUR = 0xbfac98, TRIANGLE_OUTLINE = 0x141414, TRANSPARENT = 0xff, SHUTTER_TICKS = 9, SHUTTER_SOUND = 0x49,
    ROWS = 4, ROW_COLOUR = 0, ROW_TRANSMISSION = 1, ROW_ABS = 2, ROW_TRACTION = 3, NOT_OFFERED = 2, RELEASE_FRAMES = 300
};

static struct {
    SetupArt art;
    uint8_t tile[GL_COL_W * GL_COL_H];
    uint8_t pal[768];     /* the palette the art was made for */
    int made, tile_ok;
    uint8_t *bank_tile;   /* FRONTEND/SHOW/SUPRA.QFS, read once */
    size_t bank_tile_size;
} setup;

static int variant_model(int car) { return race_head_to_head_model((int16_t)car); }

/* the art for `palette` (8 bits a channel): made again when the palette is not the one it was made for */
static void art_make(const uint8_t *palette)
{
    if (setup.made && memcmp(setup.pal, palette, sizeof setup.pal) == 0) return;
    uint8_t usable[256], pointer[256] = { 0 };
    pointer_colours_used(pointer);
    for (int i = 0; i < 256; i++) usable[i] = !pointer[i] && i != TRANSPARENT;
    memcpy(setup.pal, palette, sizeof setup.pal);
    size_t size = 0;
    uint8_t *file = resource_load_art("option/loadanim.qfs", &size);
    if (file == NULL || !added_gallery_setup_art(&setup.art, file, size, palette, usable)) port_log("showcase setup: the panel's stone was not made\n");
    resource_free(file);
    if (setup.bank_tile == NULL) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%ssupra.qfs", resource_root(), resource_german() ? "frontend/gshow/" : "frontend/show/");
        setup.bank_tile = resource_load(path, &setup.bank_tile_size);
    }
    setup.tile_ok = setup.bank_tile != NULL && added_gallery_setup_tile(setup.tile, setup.bank_tile, setup.bank_tile_size, palette, usable);
    setup.made = 1;
}

/* --- drawing on the screen's picture --- */

typedef struct {
    uint8_t *to;
    int w, h;
} Target;

static int target_get(Target *t)
{
    int32_t w = 0, h = 0;
    t->to = ea_picture_pixels(ea_screen(), &w, &h);
    t->w = w, t->h = h;
    return t->to != NULL;
}

static void fill(const Target *t, int x0, int y0, int x1, int y1, int index)
{
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < t->h; y++)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < t->w; x++) t->to[y * t->w + x] = (uint8_t)index;
}

static void edge(const Target *t, int x0, int y0, int x1, int y1, int index)
{
    fill(t, x0, y0, x1, y0, index), fill(t, x0, y1, x1, y1, index), fill(t, x0, y0, x0, y1, index), fill(t, x1, y0, x1, y1, index);
}

/* an arrow GL_ARROW_W x GL_ARROW_H pointing left or right, its top left at (x, y), over a black one a pixel down and right */
static void arrow(const Target *t, int x, int y, int left, int index, int black)
{
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i <= GL_ARROW_H; i++) {
            const int half = i <= GL_ARROW_H / 2 ? i : GL_ARROW_H - i, span = half * GL_ARROW_W / (GL_ARROW_H / 2);
            const int d = pass == 0 ? 1 : 0, x0 = left ? x + GL_ARROW_W - span : x;
            fill(t, x0 + d, y + i + d, x0 + span + d, y + i + d, pass == 0 ? black : index);
        }
}

/* --- the tile --- */

static void setup_draw(void *ctx, int car, int lit, const uint8_t *palette)
{
    (void)ctx, (void)car;
    Target t;
    if (!target_get(&t)) return;
    art_make(palette);
    const int black = ea_colour(0x000000), yellow = ea_colour(YELLOW_RGB);
    fill(&t, VARIANT_RIGHT + 1, VARIANT_TOP + SHADOW, VARIANT_RIGHT + SHADOW, VARIANT_BOTTOM, ea_colour(SHADOW_COLOUR));   /* the photos' look */
    fill(&t, VARIANT_LEFT, VARIANT_TOP, VARIANT_RIGHT, VARIANT_BOTTOM, ea_colour(lit ? LIT_COLOUR : DARK_COLOUR));
    for (int y = 0; y < GL_COL_H; y++)
        for (int x = 0; x < GL_COL_W; x++)
            if (GL_COL_X + x < t.w && GL_COL_Y + y < t.h) t.to[(GL_COL_Y + y) * t.w + GL_COL_X + x] = setup.tile_ok ? setup.tile[y * GL_COL_W + x] : (uint8_t)ea_colour(DARK_COLOUR);
    edge(&t, VARIANT_LEFT, VARIANT_TOP, VARIANT_RIGHT, VARIANT_BOTTOM, ea_colour(lit ? LIT_COLOUR : DARK_COLOUR));
    gallery_label_draw(t.to, t.w, t.h, "SETUP", GL_COL_X, GL_COL_Y, GL_COL_H, black, lit ? yellow : -1);   /* the CD labels' margins (gallery_label.h) */
    arrow(&t, GL_COL_X + GL_COL_W - 24, GL_COL_Y + (GL_COL_H - GL_ARROW_H) / 2, 0, lit ? yellow : ea_colour(TRIANGLE_COLOUR), black);
}

/* --- the panel's state --- */

typedef struct {
    int car, model, choices;
    int transmission, abs, traction;
    int row;
} Panel;

static int row_on(const Panel *p, int row)
{
    switch (row) {
    case ROW_COLOUR: return p->choices > 1;
    case ROW_TRANSMISSION: return 1;
    case ROW_ABS: return p->abs != NOT_OFFERED;
    default: return p->traction != NOT_OFFERED;
    }
}

static int row_step(const Panel *p, int step)
{
    int row = p->row;
    for (int i = 0; i < ROWS; i++) {
        row = ((row + step) % ROWS + ROWS) % ROWS;
        if (row_on(p, row)) return row;
    }
    return p->row;
}

/* the row's value stepped: Color's choice through the variants (the model's), a switch toggled */
static void value_step(Panel *p, int step)
{
    if (!row_on(p, p->row)) return;
    switch (p->row) {
    case ROW_COLOUR: car_variant_step(showcase_variants, showcase_variant_count, p->model, step, 1); break;
    case ROW_TRANSMISSION: p->transmission ^= 1; break;
    case ROW_ABS: p->abs ^= 1; break;
    default: p->traction ^= 1; break;
    }
    if (p->row != ROW_COLOUR) cg_setup_put(p->transmission, p->abs, p->traction);   /* Options' settings kept at once */
}

/* the name of the k-th variant the car is offered (k >= 1), "Default" for 0: a catalogue paint's own name (car_paint.h), not its entry's id */
static const char *variant_name(const Panel *p, int k)
{
    int seen = 0;
    if (k == 0) return "Default";
    for (int i = 0; i < showcase_variant_count; i++) {
        if (!showcase_variants[i].offered(p->model) || ++seen != k) continue;
        if (strncmp(showcase_variants[i].id, "paint_", 6) == 0) {
            const int paint = showcase_variants[i].id[6] - '0';
            return paint >= 0 && paint < car_paint_count(p->model) ? car_paint(p->model, paint)->name : showcase_variants[i].id;
        }
        return strcmp(showcase_variants[i].id, "second_livery") == 0 ? "Console version" : showcase_variants[i].id;
    }
    return "Default";
}

static void value_text(const Panel *p, int row, char *out, int size)
{
    switch (row) {
    case ROW_COLOUR: snprintf(out, (size_t)size, "%s", variant_name(p, car_variant_current(showcase_variants, showcase_variant_count, p->model))); break;
    case ROW_TRANSMISSION: snprintf(out, (size_t)size, "%s", p->transmission == 0 ? "Automatic" : "Manual"); break;
    case ROW_ABS: snprintf(out, (size_t)size, "%s", p->abs == NOT_OFFERED ? "n/a" : p->abs == 0 ? "On" : "Off"); break;
    default: snprintf(out, (size_t)size, "%s", p->traction == NOT_OFFERED ? "n/a" : p->traction == 0 ? "On" : "Off"); break;
    }
}

static void preview_fill(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t rgb)
{
    fill(ctx, x0, y0, x1, y1, ea_colour(rgb));
}

/* the panel at `frame` (0 .. SETUP_FRAMES - 1 the stone opening, SETUP_FRAMES the panel open with its words) over the picture as it is */
static void panel_draw(const Panel *p, int frame)
{
    static const char *const LABEL[ROWS] = { "Color", "Transmission", "ABS", "Traction control" };
    Target t;
    if (!target_get(&t) || !setup.art.ok) return;
    for (int y = 0; y < GL_SETUP_H; y++)
        for (int x = 0; x < GL_SETUP_W; x++) {
            const int at = y * GL_SETUP_W + x, px = GL_SETUP_X + x, py = GL_SETUP_Y + y;
            if (px < t.w && py < t.h) t.to[py * t.w + px] = frame < SETUP_FRAMES && setup.art.stone[frame][at] ? setup.art.slab[at] : setup.art.body[at];
        }
    if (frame < SETUP_FRAMES) return;
    const int black = ea_colour(0x000000), cream = ea_colour(CREAM_RGB), yellow = ea_colour(YELLOW_RGB);
    edge(&t, GL_SETUP_X, GL_SETUP_Y, GL_SETUP_X + GL_SETUP_W - 1, GL_SETUP_Y + GL_SETUP_H - 1, cream);
    text_select(TEXT_RED);
    text_draw_smooth("CAR SETUP", GL_SETUP_X + GL_SETUP_LABEL_X, GL_SETUP_Y + 8, 5, 4);   /* the page's baked "Car Showcase" is this size (owner 2026-11-18) */
    int ink_top, ink_bottom;
    text_ink_scaled("H", GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN, &ink_top, &ink_bottom);
    for (int row = 0; row < ROWS; row++) {
        const int y = GL_SETUP_Y + GL_SETUP_ROW0 + row * GL_SETUP_STEP, on = row_on(p, row), lit = p->row == row;
        const int zone = GL_SETUP_X + GL_SETUP_VALUE_X, right = zone + GL_SETUP_VALUE_W, mid = y + (ink_top + ink_bottom) / 2;
        char value[64];
        value_text(p, row, value, sizeof value);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_CREAM);
        text_draw_smooth(LABEL[row], GL_SETUP_X + GL_SETUP_LABEL_X, y, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
        text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_WHITE);
        if (row != ROW_COLOUR || !on) {   /* a switch's value (or Color while it is off): at the right end of the arrows' zone */
            text_draw_smooth(value, right - text_width_scaled(value, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN), y, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
            continue;
        }
        const int box = zone + GL_ARROW_W + 6;   /* < box   name   >: the box inside the left arrow, the name centred between the arrows */
        arrow(&t, zone, mid - GL_ARROW_H / 2, 1, lit ? yellow : cream, black);
        arrow(&t, right - GL_ARROW_W, mid - GL_ARROW_H / 2, 0, lit ? yellow : cream, black);
        fill(&t, box, mid - GL_SETUP_PREVIEW_H / 2, box + GL_SETUP_PREVIEW_W - 1, mid - GL_SETUP_PREVIEW_H / 2 + GL_SETUP_PREVIEW_H - 1, black);
        const CarVariantCanvas canvas = { &t, preview_fill, box + 1, mid - GL_SETUP_PREVIEW_H / 2 + 1, box + GL_SETUP_PREVIEW_W - 2,
                                          mid - GL_SETUP_PREVIEW_H / 2 + GL_SETUP_PREVIEW_H - 2 };
        car_variant_draw(showcase_variants, showcase_variant_count, p->model, &canvas);
        const int span0 = zone + GL_ARROW_W, span = GL_SETUP_VALUE_W - 2 * GL_ARROW_W;   /* between the arrows: the name's centre */
        text_draw_smooth(value, span0 + (span - text_width_scaled(value, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN)) / 2, y, GL_SETUP_FONT_NUM, GL_SETUP_FONT_DEN);
    }
}

/* the picture on screen: as showcase.c's draw puts it, the pointer back on */
static void present(void)
{
    ea_select_default();
    ea_blit_own((const uint8_t *)(intptr_t)ea_picture_shape(ea_screen()));
    video_flip();
    ea_select(ea_screen());
    pointer_show();
}

/* the row under (x, y) and the step a click means (Color: left or right half of its value; a switch: anywhere on its row, as Options); -1 outside */
static int panel_hit(int x, int y, int *step)
{
    *step = 0;
    if (x < GL_SETUP_X || x >= GL_SETUP_X + GL_SETUP_W || y < GL_SETUP_Y + GL_SETUP_ROW0 - GL_SETUP_STEP / 2) return -1;
    const int row = (y - GL_SETUP_Y - GL_SETUP_ROW0 + GL_SETUP_STEP / 2) / GL_SETUP_STEP;
    if (row < 0 || row >= ROWS) return -1;
    if (row != ROW_COLOUR) *step = 1;
    else if (x >= GL_SETUP_X + GL_SETUP_VALUE_X - 16) *step = x < GL_SETUP_X + GL_SETUP_VALUE_X + GL_SETUP_VALUE_W / 2 ? -1 : 1;
    return row;
}

static void shutter(const Panel *p, const uint8_t *snap, size_t bytes, int opening)
{
    Target t;
    if (!setup.art.ok || !target_get(&t)) return;
    menu_audio_sound(SHUTTER_SOUND);
    for (int k = 0; k < SETUP_FRAMES; k++) {
        memcpy(t.to, snap, bytes);
        panel_draw(p, opening ? k : SETUP_FRAMES - 1 - k);
        present();
        for (int w = 0; w < SHUTTER_TICKS; w++) menu_audio_music_service(), video_wait_vblank();
    }
    memcpy(t.to, snap, bytes);
}

static void panel_redraw(const Panel *p, const uint8_t *snap, size_t bytes)
{
    Target t;
    if (!target_get(&t)) return;
    memcpy(t.to, snap, bytes);
    if (setup.art.ok) panel_draw(p, SETUP_FRAMES);
    present();
}

/* the keys and the button let go (or RELEASE_FRAMES gone) before the screen goes on: a press it acted on is not taken again by it */
static void let_go(void)
{
    for (int i = 0; i < RELEASE_FRAMES; i++) {
        const int key = input_get_key();
        if (key == 0 && (pointer_read_buttons() & POINTER_BUTTON_SELECT) == 0) return;
        menu_audio_music_service();
        video_wait_vblank();
    }
}

static void setup_open(void *ctx, int car, const uint8_t *palette)
{
    (void)ctx;
    Target t;
    if (!target_get(&t)) return;
    art_make(palette);
    if (!text_small_init()) port_log("showcase setup: the small font is not loaded\n");
    Panel p = { car, variant_model(car), car_variant_choices(showcase_variants, showcase_variant_count, variant_model(car)), 0, 0, 0, ROW_TRANSMISSION };
    cg_setup_get(&p.transmission, &p.abs, &p.traction);
    p.row = row_on(&p, ROW_COLOUR) ? ROW_COLOUR : ROW_TRANSMISSION;
    const size_t bytes = (size_t)t.w * t.h;
    uint8_t *snap = malloc(bytes);
    if (snap == NULL) return;
    memcpy(snap, t.to, bytes);
    ea_select(ea_screen());
    shutter(&p, snap, bytes, 1);
    panel_redraw(&p, snap, bytes);
    int last_key = input_get_key(), last_buttons = pointer_read_buttons(), close = 0;   /* what opened it is still held: not a press */
    while (!close) {
        const int key = input_get_key(), buttons = pointer_read_buttons();
        int press = key != last_key && key != 0 ? key : 0, changed = 0;
        if (buttons != last_buttons) {
            if (buttons & POINTER_BUTTON_SELECT) {
                int x, y, step;
                pointer_position(&x, &y);
                const int row = panel_hit(x, y, &step);
                if (row < 0) close = 1;
                else {
                    if (row_on(&p, row)) p.row = row, changed = 1;
                    if (step != 0) value_step(&p, step), changed = 1;
                }
            } else if (buttons & POINTER_BUTTON_BACK) close = 1;
        }
        last_key = key, last_buttons = buttons;
        switch (press) {
        case GALLERY_KEY_ESCAPE: close = 1; break;
        case GALLERY_KEY_UP: p.row = row_step(&p, -1), changed = 1; break;
        case GALLERY_KEY_DOWN: p.row = row_step(&p, 1), changed = 1; break;
        case GALLERY_KEY_LEFT: value_step(&p, -1), changed = 1; break;
        case GALLERY_KEY_RIGHT:
        case GALLERY_KEY_ENTER: value_step(&p, 1), changed = 1; break;
        default: break;
        }
        if (changed && !close) panel_redraw(&p, snap, bytes);
        menu_audio_music_service();
        video_wait_vblank();
    }
    shutter(&p, snap, bytes, 0);
    present();
    free(snap);
    let_go();
}

const ShowcasePlus *glue_setup_plus(void)
{
    static const ShowcasePlus plus = { setup_draw, setup_open, NULL };
    return &plus;
}

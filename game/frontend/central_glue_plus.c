/* The Control Central glue's plus text and own panel: the port's text and dark panels over the original menus' art (central_glue.c's fifth part). */
#include "game/frontend/central_glue_private.h"

/* port: the text and the dark panels of graphics_plus.c, controls_plus.c and extras_menu.c, through the game's own
 * routines (0046376c's styles, the name box's shade table and frame); the text in the menu art's look
 * (text_draw_smooth), safe as each of those screens repaints its background and panel before writing */
void cg_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, 1, 1);
}


void cg_plus_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    enum { DARK = 0x1000, RED = 0xff0000, CORNERS = 4 };
    static uint8_t shade_table[256];  /* ea_shade_set keeps the pointer */
    (void)ctx;
    ea_shade_table(DARK, rgb, shade_table);
    ea_shade_set(shade_table);
    const int32_t corners[2 * CORNERS] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    ea_shade_polygon(CORNERS, corners);
    if (!framed) return;
    ea_frame(x0 + 2, y0 + 2, x1 - 2, y1 - 2, ea_colour(RED));
    ea_frame(x0 + 3, y0 + 3, x1 - 3, y1 - 3, ea_colour(RED));
}

/* port: the text and panel the additions draw with, for the EXTRAS screen's glue in menu_glue.c */
void glue_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    cg_plus_text(ctx, style_id, text, x, y);
}
int32_t glue_plus_text_width(void *ctx, const char *text) { return plus_text_width(ctx, text); }
void glue_plus_text_scaled(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)ctx;
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, num, den);
}
int32_t glue_plus_text_width_scaled(void *ctx, const char *text, int32_t num, int32_t den)
{
    (void)ctx;
    return text_width_scaled(text, num, den);
}
void glue_plus_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    cg_plus_panel(ctx, rgb, x0, y0, x1, y1, framed);
}
void cg_plus_save(void *ctx, const PortSettings *settings)
{
    (void)ctx;
    if (!port_settings_save(settings)) port_log("graphics: port.cfg could not be saved\n");
}

/* port: the Graphics screen's own background (graphics_own.c). config_bgnd_load reads the ten configuration screens
 * off the CD and composes them, which takes about half a second, so it is done once per process and kept -- it does
 * not depend on anything but the CD.
 *
 * The screen always puts this palette on, in OPTIONS' palette, whatever screen brought it up: entered from Game Mode
 * its art is the Game Mode picture, and blitting the Options background over that with Game Mode's palette is what
 * left DONE/CANCEL in rubbish colours. `own_palette` is the palette to hand back on the way out. */
static uint8_t *own_shape;
static uint8_t own_pixels[CONFIG_BGND_W * CONFIG_BGND_H];
static uint8_t own_palette[768];
static int own_tried, own_ok;

/* compose once, into the caller's buffers, and keep a copy: the EXTRAS screen asks for the same background
 * (menu_glue.c), and reading the ten CD screens twice would cost another half second for nothing */
static void own_compose(uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], uint8_t palette[768])
{
    if (!own_tried) {
        own_tried = 1;
        own_ok = config_bgnd_load(resource_load_art, CONFIG_BGND_OPTIONS, 1, own_pixels, own_palette);
        if (own_ok) {
            own_shape = malloc(CONFIG_BGND_SHAPE_HEADER + CONFIG_BGND_W * CONFIG_BGND_H);
            own_ok = own_shape != NULL
                     && config_bgnd_shape(own_pixels, 0, 0, CONFIG_BGND_W, CONFIG_BGND_H, own_shape) != 0;
        }
        if (!own_ok) {
            port_log("graphics: no clean configuration background, the burnt-in one stays\n");
            free(own_shape);
            own_shape = NULL;
        }
    }
    memcpy(pixels, own_pixels, sizeof own_pixels);
    memcpy(palette, own_palette, sizeof own_palette);
}

int32_t cg_own_bgnd(uint8_t pixels[640 * 480], uint8_t palette[768])
{
    own_compose(pixels, palette);
    return own_ok;
}

/* the shape, already a game shape, put on the screen; the palette goes with it, then the text styles are built from
 * that palette -- they are a ramp matched to what is showing, and they were last built for another screen's */
int32_t cg_own_bgnd_shape(void *ctx, const uint8_t pixels[640 * 480], const uint8_t palette[768])
{
    (void)ctx, (void)pixels;
    if (own_shape == NULL) return 0;
    /* RAW, into the screen's own copy rather than back into own_palette: config_bgnd.c read this palette through
     * fsh_palette_rgb, which shifts the CD's 6-bit values left by one on the way, so it is already display-ready
     * and any other mode shifts it twice; and palette_put reads and writes the same buffer, so aliasing the two
     * would rewrite the bytes it is reading, a little further every redraw. */
    memcpy(cg_screen_palette, own_palette, sizeof cg_screen_palette);
    cg_palette_set(ctx, cg_screen_palette, 0, 0x100, own_palette, 0x24 /* FSH_TYPE_PALETTE_8BIT */);
    text_build_styles_from(own_palette);
    ea_blit_own(own_shape);
    (void)palette;
    return 1;
}

/* the same background for the EXTRAS screen (menu_glue.c): one composed copy, shared by both screens rather than
 * read off the CD twice */
int32_t glue_own_bgnd(uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], uint8_t palette[768])
{
    own_compose(pixels, palette);
    return own_ok;
}

int32_t glue_own_bgnd_shape(void *ctx, const uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H],
                            const uint8_t palette[768])
{
    return cg_own_bgnd_shape(ctx, pixels, palette);
}

/* the smooth text for the Graphics screen: the letters with the background put back under them first, taken from the
 * composed copy in own_pixels -- the same picture the page is blitted with, not Options' bank. This is
 * options_plus_text's draw over the Graphics screen's own background instead of the Options one. */
void own_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    enum { MARGIN = 2, SHADOW_ROOM = 6 };
    (void)ctx;
    if (own_shape != NULL) {
        const int left = x - MARGIN, top = y - MARGIN;
        const int w = text_width(text) + MARGIN + SHADOW_ROOM, h = text_line_height() + MARGIN + SHADOW_ROOM;
        uint8_t *const cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * h);
        if (cover != NULL && config_bgnd_shape(own_pixels, left, top, w, h, cover) != 0)
            ea_blit_own(cover);
        free(cover);
    }
    own_panel_text(ctx, style_id, text, x, y);
}

/* port: a small note over the Graphics-style background: own_plus_text's cover, at 3/5 of the size */
void cg_own_note_text(void *ctx, const char *text, int32_t x, int32_t y)
{
    enum { NUM = 3, DEN = 5, MARGIN = 2, SHADOW_ROOM = 6 };
    if (own_shape != NULL) {
        const int w = text_width_scaled(text, NUM, DEN) + MARGIN + SHADOW_ROOM, h = text_line_height() * NUM / DEN + MARGIN + SHADOW_ROOM;
        uint8_t *const cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * h);
        if (cover != NULL && config_bgnd_shape(own_pixels, x - MARGIN, y - MARGIN, w, h, cover) != 0) ea_blit_own(cover);
        free(cover);
    }
    glue_plus_text_scaled(ctx, 0, text, x, y, NUM, DEN);
}

/* port: the titles in the original titles' letters (title_art.h), cut from the CD the first time each is drawn and
 * kept; a title the CD cannot give is drawn in the font */
static const uint8_t *title_mask(const char *text, int *w)
{
    static uint8_t masks[TITLE_ART_COUNT][TITLE_ART_MAX_W * TITLE_ART_H];
    static int widths[TITLE_ART_COUNT], tried[TITLE_ART_COUNT], ok[TITLE_ART_COUNT];
    const TitleArtId id = title_art_for(text);
    if (id == TITLE_ART_COUNT) return NULL;
    if (!tried[id]) {
        tried[id] = 1;
        ok[id] = title_art_load(resource_load_art, id, masks[id], &widths[id]);
        if (!ok[id]) port_log("title: \"%s\" not on the CD, drawn in the font\n", text);
    }
    *w = widths[id];
    return ok[id] ? masks[id] : NULL;
}

/* port: whether the display is already dark, so the Graphics screen skips the fade of a display Options has already
 * faded out. video_display_is_dark answers from the palette on the display, so it is right whoever faded it. */
int32_t cg_display_dark(void *ctx)
{
    (void)ctx;
    return video_display_is_dark();
}

/* the screen's title as the original screens draw theirs ("Game Mode", "Control Central"): red, at their items' size */
void own_title_text(void *ctx, const char *text, int32_t x, int32_t y)
{
    enum { TITLE_NUM = 33, TITLE_DEN = 20 };   /* menu_glue.c's MODE_TEXT_NUM / DEN */
    (void)ctx;
    text_build_styles_from(own_palette);
    text_select(TEXT_RED);
    int w = 0;
    const uint8_t *const art = title_mask(text, &w);
    if (art != NULL)   /* where the originals stand theirs, taken from where the font's would stand */
        text_draw_mask(art, w, TITLE_ART_H, x - TITLE_ART_FONT_X + TITLE_ART_LEFT, y - TITLE_ART_FONT_Y + TITLE_ART_ROW0);
    else
        text_draw_smooth(text, x, y, TITLE_NUM, TITLE_DEN);
}

/* the device a controls page is for, under its title: in the title's red, at the items' size */
void cg_own_subtitle_text(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    text_build_styles_from(own_palette);
    text_select(TEXT_RED);
    text_draw_smooth(text, x, y, 1, 1);
}

/* a group's header as the credits draw theirs ("EA Seattle"), at their size: about 2/3 of the game's text */
void own_header_text(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    enum { HEADER_NUM = 2, HEADER_DEN = 3 };
    (void)ctx;
    text_build_styles_from(own_palette);
    text_draw_header(text, x, y, HEADER_NUM, HEADER_DEN, width);
}

void own_panel_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    /* the styles from the background that is showing: they are a ramp matched to it, and the last ones were built
     * for another screen's palette */
    text_build_styles_from(own_palette);
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, 1, 1);
}

/* the triangle as own_panel_text draws a word: in the styles of the background that is showing */
void own_panel_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y)
{
    text_build_styles_from(own_palette);
    central_page_arrow(ctx, right, lit, x, y);
}


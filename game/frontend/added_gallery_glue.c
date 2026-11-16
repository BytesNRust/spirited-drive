/* port (dep59 step 3): an added car's gallery drawn and run (added_gallery.h holds its state and keys). A page of its own palette: the
 * picture (gallery.png, 320x200, or card.png in the middle of that box when the piece has none) quantised once (boot_quant), the port's
 * text colours (BOOT_TEXT_RGB), one entry for the Color swatch, and Control Central's last entries kept as they were for the pointer.
 * Its name and place in the list on top; the picture on the left, the author, version and description on the right; Skin and Color
 * under them with the figures; Previous, Next and Done at the foot. The pair picked is kept by id as the page closes (car_looks.h). */
#include "game/frontend/added_gallery.h"

#include "formats/png.h"
#include "game/frontend/boot_quant.h"
#include "game/frontend/boot_screen.h"
#include "game/frontend/car_list.h"
#include "game/frontend/central_glue_private.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/text_wrap.h"
#include "game/platform/platform.h"
#include "game/race/car_registry.h"
#include "game/sys/car_looks.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SCREEN_W = 640, SCREEN_H = 480, PICTURE_W = 320, PICTURE_H = 200, PICTURE_X = 24, PICTURE_Y = 76,
    KEPT = 18,                                       /* Control Central's last entries, kept for the pointer */
    TEXT_FIRST = 256 - KEPT - BOOT_TEXT_COLOURS,     /* the text's colours */
    SWATCH = TEXT_FIRST - 1,                         /* the Color swatch's entry */
    PICTURE_COLOURS = SWATCH,                        /* the picture's: 0 .. SWATCH - 1 */
    BLACK = TEXT_FIRST + 5,
    WORDS_X = 364, WORDS_W = 252, WORDS_LINES = 9,
    SKIN_Y = 316, COLOUR_Y = 356, FIGURES_Y = 292, FOOT_Y = 436,
    ARROW_LEFT_X = 150, ARROW_RIGHT_X = 470, ARROW_W = 30, ROW_H = 30
};

typedef struct {
    CarPieceCard card;
    GalleryState state;
    int car, place, cars;            /* the car, its place in the menus' list (1..) and the list's length */
    uint8_t *pixels;                 /* the picture in the page's palette, PICTURE_W x PICTURE_H */
    uint8_t palette[768];
} Page;

/* `name` of the piece as RGB in a PICTURE_W x PICTURE_H box (a smaller picture in its middle, the rest black); 0 when it is not there or
 * is larger than the box */
static int picture_rgb(const char *dir, const char *name, uint8_t *rgb)
{
    char path[CAR_PIECE_DIR_MAX + 16];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    size_t size = 0;
    uint8_t *data = platform_read_file(path, &size);
    uint32_t w = 0, h = 0;
    const int sized = data != NULL && png_size(data, size, &w, &h) == PNG_OK;
    const int fits = sized && w <= PICTURE_W && h <= PICTURE_H;   /* before the pixels: a hostile size is never decoded */
    PngImage img;
    const int read = fits && png_read(data, size, &img) == PNG_OK;
    if (data != NULL) platform_free_file(data);
    if (sized && !fits) port_log("car piece: %s is larger than %dx%d: not shown\n", name, PICTURE_W, PICTURE_H);
    if (!read) return 0;
    if (img.width != w || img.height != h) return png_free(img.pixels), 0;
    const int x0 = (PICTURE_W - (int)img.width) / 2, y0 = (PICTURE_H - (int)img.height) / 2;
    memset(rgb, 0, (size_t)PICTURE_W * PICTURE_H * 3);
    for (int y = 0; y < (int)img.height; y++)
        for (int x = 0; x < (int)img.width; x++) {
            const size_t at = (size_t)y * img.width + (size_t)x;
            const uint8_t *s = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            memcpy(rgb + 3 * ((size_t)(y0 + y) * PICTURE_W + (size_t)(x0 + x)), s, 3);
        }
    png_free(img.pixels);
    return 1;
}

/* the page's picture and palette (Control Central's palette `screen` gives the kept entries) */
static void page_picture(Page *p, const uint8_t screen[768])
{
    uint8_t *rgb = malloc((size_t)PICTURE_W * PICTURE_H * 3);
    p->pixels = calloc((size_t)PICTURE_W * PICTURE_H, 1);
    memset(p->palette, 0, sizeof p->palette);
    if (rgb != NULL && p->pixels != NULL) {
        if (!picture_rgb(p->card.dir, "gallery.png", rgb) && !picture_rgb(p->card.dir, "card.png", rgb))
            memset(rgb, 0, (size_t)PICTURE_W * PICTURE_H * 3);
        if (boot_quant(rgb, PICTURE_W * PICTURE_H, PICTURE_COLOURS, p->palette, p->pixels) == 0) memset(p->pixels, 0, (size_t)PICTURE_W * PICTURE_H);
    }
    free(rgb);
    memcpy(p->palette + 3 * TEXT_FIRST, BOOT_TEXT_RGB, sizeof BOOT_TEXT_RGB);
    memcpy(p->palette + 3 * (256 - KEPT), screen + 3 * (256 - KEPT), 3 * KEPT);
}

static int small_width(void *ctx, const char *s, int len)
{
    (void)ctx;
    char run[MOD_INI_DESCRIPTION_MAX + 1];
    snprintf(run, sizeof run, "%.*s", len, s);
    return text_small_width(run);
}

static void say(TextStyle style, const char *text, int x, int y)
{
    text_select(style);
    text_draw(text, x, y);
}

/* a selector's row: its label, the arrows (none when off) and the value */
static void selector(const char *label, const char *value, int y, int lit, int on)
{
    const TextStyle style = !on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_CREAM;
    say(style, label, PICTURE_X, y);
    if (on) say(style, "<", ARROW_LEFT_X, y), say(style, ">", ARROW_RIGHT_X, y);
    say(style, value, ARROW_LEFT_X + ARROW_W + 8, y);
}

/* the words: author, version, the description in lines, and the figures */
static void words(const Page *p)
{
    char line[160];
    int y = PICTURE_Y;
    text_select(TEXT_CREAM);
    char said[sizeof p->card.info.description];   /* the mod's words as the small face draws them (text_small_words) */
    if (text_small_words(p->card.info.author, said, sizeof said) > 0)
        snprintf(line, sizeof line, "by %s", said), text_small_draw(line, WORDS_X, y), y += text_small_line_height();
    text_small_words(p->card.info.version, said, sizeof said);
    snprintf(line, sizeof line, "version %s", said);
    text_small_draw(line, WORDS_X, y), y += text_small_line_height() * 3 / 2;
    TextLine lines[WORDS_LINES];
    text_small_words(p->card.info.description, said, sizeof said);
    const char *const d = said;
    const int n = text_wrap(d, WORDS_W, small_width, NULL, lines, WORDS_LINES);
    text_select(TEXT_WHITE);
    for (int i = 0; i < n; i++, y += text_small_line_height()) {
        snprintf(line, sizeof line, "%.*s", lines[i].len, d + lines[i].start);
        text_small_draw(line, WORDS_X, y);
    }
    const char *fig[3] = { p->card.info.year, p->card.info.power, p->card.info.weight };
    line[0] = '\0';
    for (int i = 0; i < 3; i++)
        if (text_small_words(fig[i], said, sizeof said) > 0)
            snprintf(line + strlen(line), sizeof line - strlen(line), "%s%s", line[0] != '\0' ? "  /  " : "", said);
    text_select(TEXT_CREAM);
    text_small_draw(line, PICTURE_X, FIGURES_Y);
}

/* the page into the back buffer */
static void draw(const Page *p)
{
    int w, h;
    uint8_t *to = video_target(&w, &h);
    for (int y = 0; y < SCREEN_H && y < h; y++) memset(&to[y * w], BLACK, (size_t)(w < SCREEN_W ? w : SCREEN_W));
    for (int y = 0; p->pixels != NULL && w >= PICTURE_X + PICTURE_W && y < PICTURE_H && PICTURE_Y + y < h; y++)
        memcpy(&to[(PICTURE_Y + y) * w + PICTURE_X], &p->pixels[y * PICTURE_W], PICTURE_W);
    char line[96];
    say(TEXT_YELLOW, p->card.info.name, PICTURE_X, 24);
    snprintf(line, sizeof line, "< %d / %d >", p->place, p->cars);
    say(TEXT_CREAM, line, SCREEN_W - 24 - text_width(line), 24);
    words(p);
    const GalleryState *s = &p->state;
    const int on = added_gallery_colour_on(&p->card, s);
    selector("Skin", s->skin == 0 ? "As it comes" : p->card.skin[s->skin], SKIN_Y, s->item == GALLERY_SKIN, 1);
    selector("Color", s->paint == 0 ? "Own colour" : p->card.info.paints[s->paint - 1].name, COLOUR_Y, s->item == GALLERY_COLOUR, on);
    if (on && s->paint > 0) {
        const int x = ARROW_RIGHT_X - 28;
        for (int y = COLOUR_Y + 4; y < COLOUR_Y + 20 && y < h; y++) memset(&to[y * w + x], SWATCH, 18);
    }
    say(s->item == GALLERY_PREVIOUS ? TEXT_YELLOW : TEXT_CREAM, "Previous", PICTURE_X, FOOT_Y);
    say(s->item == GALLERY_NEXT ? TEXT_YELLOW : TEXT_CREAM, "Next", 280, FOOT_Y);
    say(s->item == GALLERY_DONE ? TEXT_YELLOW : TEXT_CREAM, "Done", SCREEN_W - 24 - text_width("Done"), FOOT_Y);
}

/* the palette with the swatch's colour, on screen */
static void palette_now(Page *p)
{
    const GalleryState *s = &p->state;
    const uint8_t *rgb = s->paint > 0 ? p->card.info.paints[s->paint - 1].rgb : (const uint8_t[3]){ 0, 0, 0 };
    memcpy(p->palette + 3 * SWATCH, rgb, 3);
    video_set_palette(0, 256, p->palette);
}

/* a click: the key it stands for, the item moved to the one clicked; 0 for none */
static int click_key(GalleryState *s)
{
    int x, y;
    pointer_position(&x, &y);
    const int rows[2] = { SKIN_Y, COLOUR_Y };
    for (int i = 0; i < 2; i++) {
        if (y < rows[i] - 4 || y >= rows[i] + ROW_H) continue;
        s->item = i == 0 ? GALLERY_SKIN : GALLERY_COLOUR;
        if (x >= ARROW_LEFT_X - 6 && x < ARROW_LEFT_X + ARROW_W) return GALLERY_KEY_LEFT;
        if (x >= ARROW_RIGHT_X - 6 && x < ARROW_RIGHT_X + ARROW_W) return GALLERY_KEY_RIGHT;
        return 0;
    }
    if (y < FOOT_Y - 6 || y >= FOOT_Y + ROW_H) return 0;
    s->item = x < 200 ? GALLERY_PREVIOUS : x < 420 ? GALLERY_NEXT : GALLERY_DONE;
    return GALLERY_KEY_ENTER;
}

/* the pick kept for `id`, as the page's state */
static void state_from_look(Page *p)
{
    const CarLook l = car_looks_get(p->card.info.id);
    p->state = (GalleryState){ GALLERY_SKIN, 0, 0 };
    for (int k = 1; k < p->card.skins; k++)
        if (strcmp(p->card.skin[k], l.skin) == 0) p->state.skin = k;
    for (int k = 0; k < p->card.info.paint_count; k++)
        if (strcmp(p->card.info.paints[k].name, l.paint) == 0) p->state.paint = k + 1;
}

static void look_keep(const Page *p)
{
    CarLook l = { { 0 }, { 0 } };
    if (p->state.skin > 0) snprintf(l.skin, sizeof l.skin, "%s", p->card.skin[p->state.skin]);
    if (p->state.paint > 0) snprintf(l.paint, sizeof l.paint, "%s", p->card.info.paints[p->state.paint - 1].name);
    if (!car_looks_set(p->card.info.id, &l)) port_log("car looks: %s's skin and colour were not kept\n", p->card.info.id);
}

/* one car's page until it is left or another is asked for: GALLERY_LEAVE, GALLERY_GO_PREVIOUS or GALLERY_GO_NEXT */
static int run_page(Page *p)
{
    int last_key = -1, last_buttons = -1, result = GALLERY_STAY, changed = 1;
    while (result == GALLERY_STAY) {
        if (changed) {
            ea_select_default();
            palette_now(p);
            text_build_styles_from(p->palette);   /* before the words: their colours are the page's */
            draw(p);
            video_flip();
            pointer_show();
            changed = 0;
            /* port (test harness): the page as shown, for a walk to wait on after each key (tests/car_card_menus.sh); the log only */
            port_log("mark:gallery=%d %d %d\n", p->state.item, p->state.skin, p->state.paint);   /* item skin paint: a WAIT's text is < 32 */
        }
        const int key = input_get_key();   /* first: it pumps the events the pointer's buttons are read from */
        const int buttons = pointer_read_buttons();
        int press = key != last_key && key != 0 ? key : 0;
        if (buttons != last_buttons && last_buttons >= 0) {
            if (buttons & POINTER_BUTTON_SELECT) press = click_key(&p->state);
            else if (buttons & POINTER_BUTTON_BACK) press = GALLERY_KEY_ESCAPE;
        }
        last_key = key, last_buttons = buttons;
        if (press != 0) {
            const GalleryState was = p->state;
            result = added_gallery_input(&p->card, &p->state, press);
            changed = memcmp(&was, &p->state, sizeof was) != 0;
        }
        video_wait_vblank();
    }
    return result;
}

int32_t cg_added_gallery(void *ctx, int16_t *car, int warrior, const uint8_t screen[768])
{
    (void)ctx;
    if (!text_small_init()) port_log("added gallery: the small font is not loaded\n");
    for (;;) {
        Page p;
        memset(&p, 0, sizeof p);
        if (!car_pieces_card(*car, &p.card)) return 0;
        CarList l;
        car_list_build(&l, 1, warrior);
        p.car = *car, p.cars = l.count;
        for (int i = 0; i < l.count; i++)
            if (l.car[i] == *car) p.place = i + 1;
        state_from_look(&p);
        page_picture(&p, screen);
        port_log("mark:screen=added_gallery\n");   /* port: the headless walk waits for it */
        const int result = run_page(&p);
        look_keep(&p);
        free(p.pixels);
        if (result == GALLERY_LEAVE) return 0;
        *car = car_list_step(&l, *car, result == GALLERY_GO_NEXT ? 1 : -1);
        if (!car_registry_is_added(*car)) return 1;   /* an original's: its own gallery */
    }
}

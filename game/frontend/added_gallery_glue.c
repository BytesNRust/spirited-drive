/* port (dep59 step 3; the owner's v3b, notes/dep59-added-gallery-showcase-plan.md sections 5-7): an added car's gallery drawn and run
 * (added_gallery.h holds its state and keys), laid out like the Car Showcase (gallery_layout.h). A page of its own palette: the still
 * part (the clean configuration background, the picture, gallery.png 320x200 or card.png in the middle of that box, its frame and the
 * panels) quantised once, the picture's entries apart from the rest's; the port's text colours (BOOT_TEXT_RGB), one entry for the Color
 * swatch, and Control Central's last entries kept as they were for the pointer. Its name big and leant on top, "Car Showcase" under it;
 * the picture on the left; the technical data, Skin and Color on the right; the description, author and version in the foot's band;
 * DONE at the bottom left, "< NEXT CAR >" in the middle. The pair picked is kept by id as the page closes (car_looks.h). */
#include "game/frontend/added_gallery.h"
#include "game/frontend/added_gallery_run.h"

#include "formats/fsh.h"
#include "formats/pbs.h"
#include "formats/png.h"
#include "formats/stock_content.h"
#include "game/frontend/boot_quant.h"
#include "game/frontend/boot_screen.h"
#include "game/frontend/car_figures.h"
#include "game/frontend/car_list.h"
#include "game/frontend/central_glue_private.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/text_wrap.h"
#include "game/platform/platform.h"
#include "game/race/car_registry.h"
#include "game/sys/car_looks.h"
#include "game/sys/edition_file.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    KEPT = 18,                                       /* Control Central's last entries, kept for the pointer */
    TEXT_FIRST = 256 - KEPT - BOOT_TEXT_COLOURS,     /* the text's colours (BOOT_TEXT_RGB's order) */
    SWATCH = TEXT_FIRST - 1,                         /* the Color swatch's entry */
    STILL_COLOURS = SWATCH,                          /* the still part's: 0 .. SWATCH - 1 */
    WHITE = TEXT_FIRST, CREAM = TEXT_FIRST + 1, YELLOW = TEXT_FIRST + 2, BLACK = TEXT_FIRST + 5, DIM = TEXT_FIRST + 6,
    LEAN_NUM = 2, LEAN_DEN = 9                       /* the name's slant: 2 px across for 9 up */
};

static const char DONE_WORD[] = "DONE", NEXT_WORD[] = "Next car";

/* port (dep59 section 8, the owner's): the Warrior's page. Its year as the owner gave it; its power and weight from its physics
 * (CARSPECS/TRAFFC.PBS, the race's car_model_names[11]); its look fixed: the skin as it comes and its own dark purple, sampled from its
 * Control Central card (CONTROL/WAR1.QFS on Central's palette: the mean of the body's purple, #581C54); its picture that card */
static const char WARRIOR_YEAR[] = "1994", WARRIOR_SPECS[] = "SIMDATA/CARSPECS/TRAFFC.PBS", WARRIOR_CARD[] = "control/war1.qfs";
static const char WARRIOR_PAINT[] = "Purple";
static const uint8_t WARRIOR_RGB[3] = { 0x58, 0x1c, 0x54 };

typedef struct {
    CarPieceCard card;
    GalleryState state;
    int car;
    int warrior;                     /* the Warrior's page: its card from the CD, its look fixed */
    uint8_t *still;                  /* the still part in the page's palette, GL_W x GL_H */
    uint8_t palette[768];
} Page;

/* `name` of the piece as RGB in a GL_PICTURE_W x GL_PICTURE_H box (a smaller picture in its middle, the rest black); 0 when it is not
 * there or is larger than the box */
static int picture_rgb(const char *dir, const char *name, uint8_t *rgb)
{
    char path[CAR_PIECE_DIR_MAX + 16];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    size_t size = 0;
    uint8_t *data = platform_read_file(path, &size);
    uint32_t w = 0, h = 0;
    const int sized = data != NULL && png_size(data, size, &w, &h) == PNG_OK;
    const int fits = sized && w <= GL_PICTURE_W && h <= GL_PICTURE_H;   /* before the pixels: a hostile size is never decoded */
    PngImage img;
    const int read = fits && png_read(data, size, &img) == PNG_OK;
    if (data != NULL) platform_free_file(data);
    if (sized && !fits) port_log("car piece: %s is larger than %dx%d: not shown\n", name, GL_PICTURE_W, GL_PICTURE_H);
    if (!read) return 0;
    if (img.width != w || img.height != h) return png_free(img.pixels), 0;
    const int x0 = (GL_PICTURE_W - (int)img.width) / 2, y0 = (GL_PICTURE_H - (int)img.height) / 2;
    memset(rgb, 0, (size_t)GL_PICTURE_W * GL_PICTURE_H * 3);
    for (int y = 0; y < (int)img.height; y++)
        for (int x = 0; x < (int)img.width; x++) {
            const size_t at = (size_t)y * img.width + (size_t)x;
            const uint8_t *s = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            memcpy(rgb + 3 * ((size_t)(y0 + y) * GL_PICTURE_W + (size_t)(x0 + x)), s, 3);
        }
    png_free(img.pixels);
    return 1;
}

/* the configuration screens' clean background as RGB; a flat brown when it cannot be composed */
static void background_rgb(uint8_t *rgb)
{
    uint8_t *pixels = malloc((size_t)GL_W * GL_H), palette[768];
    const int ok = pixels != NULL && config_bgnd_load(resource_load_art, CONFIG_BGND_OPTIONS, 0, pixels, palette);
    if (!ok) port_log("added gallery: no clean background: a flat one\n");
    for (int i = 0; i < GL_W * GL_H; i++) {
        static const uint8_t FLAT[3] = { 58, 44, 30 };
        memcpy(rgb + 3 * i, ok ? palette + 3 * pixels[i] : FLAT, 3);
    }
    free(pixels);
}

/* the Warrior's Control Central card (an 8-bit picture on Central's palette `screen`) as RGB in the middle of the picture's box; 0 when
 * it cannot be read */
static int warrior_picture(const uint8_t screen[768], uint8_t *rgb)
{
    size_t size = 0;
    uint8_t *data = resource_load_art(WARRIOR_CARD, &size);
    Fsh fsh;
    uint32_t index;
    FshEntry e;
    const int ok = data != NULL && fsh_open(&fsh, data, size) == FSH_OK && fsh_find(&fsh, "0000", &index) == FSH_OK &&
                   fsh_entry(&fsh, index, &e) == FSH_OK && e.width <= GL_PICTURE_W && e.height <= GL_PICTURE_H &&
                   e.payload_size >= (size_t)e.width * e.height;
    if (ok) {
        const int x0 = (GL_PICTURE_W - e.width) / 2, y0 = (GL_PICTURE_H - e.height) / 2;
        memset(rgb, 0, (size_t)GL_PICTURE_W * GL_PICTURE_H * 3);
        for (int y = 0; y < e.height; y++)
            for (int x = 0; x < e.width; x++)
                memcpy(rgb + 3 * ((size_t)(y0 + y) * GL_PICTURE_W + (size_t)(x0 + x)), screen + 3 * e.payload[(size_t)y * e.width + x], 3);
    } else {
        port_log("added gallery: the Warrior's card %s was not read\n", WARRIOR_CARD);
    }
    resource_free(data);
    return ok;
}

/* the Warrior's page's card: its name, year, figures from its physics, one skin and its one colour */
static void warrior_card(Page *p)
{
    CarPieceCard *c = &p->card;
    memset(c, 0, sizeof *c);
    snprintf(c->info.id, sizeof c->info.id, "%s", stock_car_id(STOCK_CAR_WARRIOR));
    snprintf(c->info.name, sizeof c->info.name, "%s", stock_car_name(STOCK_CAR_WARRIOR));
    snprintf(c->info.year, sizeof c->info.year, "%s", WARRIOR_YEAR);
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", resource_root(), WARRIOR_SPECS);
    size_t size = 0;
    uint8_t *data = resource_load(path, &size), record[PBS_SIZE];
    int hp, kg;
    if (data != NULL && pbs_read(record, data, size) && car_figures_pbs(record, &hp, &kg)) {
        snprintf(c->info.power, sizeof c->info.power, "%d hp", hp);
        snprintf(c->info.weight, sizeof c->info.weight, "%d kg", kg);
    } else {
        port_log("added gallery: the Warrior's figures were not read from %s\n", WARRIOR_SPECS);
    }
    resource_free(data);
    c->skins = 1;
    c->info.paint_count = 1;
    snprintf(c->info.paints[0].name, sizeof c->info.paints[0].name, "%s", WARRIOR_PAINT);
    memcpy(c->info.paints[0].rgb, WARRIOR_RGB, 3);
    p->warrior = 1;
    p->state = (GalleryState){ GALLERY_NEXT, 0, 1, 1 };
}

/* the page's still part and palette (Control Central's palette `screen` gives the kept entries) */
static void page_still(Page *p, const uint8_t screen[768])
{
    uint8_t *picture = malloc((size_t)GL_PICTURE_W * GL_PICTURE_H * 3), *bgnd = malloc((size_t)GL_W * GL_H * 3);
    uint8_t *rgb = malloc((size_t)GL_W * GL_H * 3);
    p->still = calloc((size_t)GL_W * GL_H, 1);
    memset(p->palette, 0, sizeof p->palette);
    if (picture != NULL && bgnd != NULL && rgb != NULL && p->still != NULL) {
        const int read = p->warrior ? warrior_picture(screen, picture)
                                    : picture_rgb(p->card.dir, "gallery.png", picture) || picture_rgb(p->card.dir, "card.png", picture);
        if (!read) memset(picture, 0, (size_t)GL_PICTURE_W * GL_PICTURE_H * 3);
        background_rgb(bgnd);
        gallery_compose(bgnd, picture, rgb);
        if (!gallery_quantise(rgb, STILL_COLOURS, p->palette, p->still)) port_log("added gallery: the page was not quantised\n");
    }
    free(picture), free(bgnd), free(rgb);
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

typedef struct {
    uint8_t *to;
    int w, h;
} Target;

/* x0..x1, y0..y1 (ends included) in entry `index` */
static void fill(const Target *t, int x0, int y0, int x1, int y1, int index)
{
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < t->h; y++)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < t->w; x++) t->to[y * t->w + x] = (uint8_t)index;
}

static void edge(const Target *t, int x0, int y0, int x1, int y1, int width, int index)
{
    fill(t, x0, y0, x1, y0 + width - 1, index), fill(t, x0, y1 - width + 1, x1, y1, index);
    fill(t, x0, y0, x0 + width - 1, y1, index), fill(t, x1 - width + 1, y0, x1, y1, index);
}

/* an arrow GL_ARROW_W x GL_ARROW_H pointing left or right, its top left at (x, y), over a black one a pixel down and right */
static void arrow(const Target *t, int x, int y, int left, int index)
{
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i <= GL_ARROW_H; i++) {
            const int half = i <= GL_ARROW_H / 2 ? i : GL_ARROW_H - i, span = half * GL_ARROW_W / (GL_ARROW_H / 2);
            const int d = pass == 0 ? 1 : 0, x0 = left ? x + GL_ARROW_W - span : x;
            fill(t, x0 + d, y + i + d, x0 + span + d, y + i + d, pass == 0 ? BLACK : index);
        }
}

/* `text` in the small face centred on cx */
static void small_centred(const char *text, int cx, int y) { text_small_draw(text, cx - text_small_width(text) / 2, y); }

/* the name big and leant, white, with the original titles' shadow; smaller when it would not fit the screen */
static void name(const Page *p)
{
    char said[sizeof p->card.info.name];
    text_words(p->card.info.name, said, sizeof said);   /* the main face: its ',' is a comma */
    static const int SIZES[3][2] = { { GL_NAME_NUM, GL_NAME_DEN }, { 3, 2 }, { 1, 1 } };
    for (int k = 0; k < 3; k++) {
        const int num = SIZES[k][0], den = SIZES[k][1], w = text_width_scaled(said, num, den) + 2, h = text_mask_rows(said, num, den);
        const int ow = w + h * LEAN_NUM / LEAN_DEN + 2;
        if (h <= 0 || (GL_NAME_X + ow > GL_W - GL_NAME_X && k < 2)) continue;
        uint8_t *mask = malloc((size_t)w * h), *leant = malloc((size_t)ow * h);
        if (mask != NULL && leant != NULL && text_mask_scaled(said, num, den, mask, w, h)) {
            gallery_lean(mask, w, h, LEAN_NUM, LEAN_DEN, leant, ow);
            text_select(TEXT_WHITE);
            text_draw_mask(leant, ow, h, GL_NAME_X, GL_NAME_Y);
        }
        free(mask), free(leant);
        return;
    }
}

/* the technical data, labelled, in its panel */
static void figures(const Page *p)
{
    static const char *const LABEL[GL_FIGURES_ROWS] = { "Year", "Power", "Weight" };
    const char *value[GL_FIGURES_ROWS] = { p->card.info.year, p->card.info.power, p->card.info.weight };
    char said[64];
    for (int i = 0; i < GL_FIGURES_ROWS; i++) {
        const int y = GL_FIGURES_Y + GL_FIGURES_PAD + i * GL_FIGURES_ROW - 2;
        text_select(TEXT_DIM);
        text_small_draw(LABEL[i], GL_COLUMN_X + 10, y);
        if (text_small_words(value[i], said, sizeof said) == 0) snprintf(said, sizeof said, "-");
        text_select(TEXT_CREAM);
        text_small_draw(said, GL_COLUMN_X + GL_COLUMN_W - 10 - text_small_width(said), y);
    }
}

/* a selector's box: its frame (yellow when lit), its label and value centred, its arrows at its sides (none when it is off, dim when
 * the look is fixed) */
static void box(const Target *t, int y, const char *label, const char *value, int lit, int on, int swatch, int fixed)
{
    const int x1 = GL_COLUMN_X + GL_COLUMN_W - 1, cx = GL_COLUMN_X + GL_COLUMN_W / 2;
    if (swatch) fill(t, GL_COLUMN_X, y, x1, y + GL_BOX_H - 1, SWATCH);
    edge(t, GL_COLUMN_X, y, x1, y + GL_BOX_H - 1, 2, lit ? YELLOW : BLACK);
    text_select(!on ? TEXT_DIM : lit ? TEXT_YELLOW : TEXT_CREAM);
    text_draw_smooth(label, cx - text_width_scaled(label, 3, 4) / 2, y + 5, 3, 4);
    char said[64];
    text_small_words(value, said, sizeof said);
    text_select(!on ? TEXT_DIM : TEXT_WHITE);
    small_centred(said, cx, y + GL_BOX_H - 23);
    if (!on) return;
    const int ay = y + (GL_BOX_H - GL_ARROW_H) / 2, index = fixed ? DIM : lit ? YELLOW : CREAM;
    arrow(t, GL_COLUMN_X + GL_ARROW_IN, ay, 1, index);
    arrow(t, x1 - GL_ARROW_IN - GL_ARROW_W, ay, 0, index);
}

/* the foot's band: the description in lines, the author at its left, the version at its right */
static void foot(const Page *p)
{
    char said[sizeof p->card.info.description], line[160];
    TextLine lines[GL_FOOT_LINES];
    text_small_words(p->card.info.description, said, sizeof said);
    const int n = text_wrap(said, GL_FOOT_X1 - GL_FOOT_X0 - 2 * GL_FOOT_PAD, small_width, NULL, lines, GL_FOOT_LINES);
    text_select(TEXT_WHITE);
    for (int i = 0; i < n; i++) {
        snprintf(line, sizeof line, "%.*s", lines[i].len, said + lines[i].start);
        text_small_draw(line, GL_FOOT_X0 + GL_FOOT_PAD, GL_FOOT_Y0 + 6 + i * text_small_line_height());
    }
    const int y = GL_FOOT_Y1 - 22;
    text_select(TEXT_DIM);
    if (text_small_words(p->card.info.author, said, sizeof said) > 0)
        snprintf(line, sizeof line, "by %s", said), text_small_draw(line, GL_FOOT_X0 + GL_FOOT_PAD, y);
    if (text_small_words(p->card.info.version, said, sizeof said) > 0) {
        snprintf(line, sizeof line, "version %s", said);
        text_small_draw(line, GL_FOOT_X1 - GL_FOOT_PAD - text_small_width(line), y);
    }
}

/* DONE at the left; "< NEXT CAR >" in the middle, its arrows level with the words' middle */
static void bottom(const Target *t, int item)
{
    text_select(item == GALLERY_DONE ? TEXT_YELLOW : TEXT_CREAM);
    text_draw_shadowed(DONE_WORD, GL_DONE_X, GL_BOTTOM_Y);
    int l, x, r, top, under;
    gallery_next_place(text_width(NEXT_WORD), &l, &x, &r);
    text_select(item == GALLERY_NEXT ? TEXT_YELLOW : TEXT_CREAM);
    text_draw_shadowed(NEXT_WORD, x, GL_BOTTOM_Y);
    text_ink_scaled(NEXT_WORD, 1, 1, &top, &under);
    const int ay = GL_BOTTOM_Y + (top + under) / 2 - GL_ARROW_H / 2;
    arrow(t, l, ay, 1, item == GALLERY_PREVIOUS ? YELLOW : CREAM);
    arrow(t, r, ay, 0, item == GALLERY_NEXT ? YELLOW : CREAM);
}

/* the page into the back buffer */
static void draw(const Page *p)
{
    Target t;
    t.to = video_target(&t.w, &t.h);
    for (int y = 0; y < GL_H && y < t.h; y++)
        if (p->still != NULL) memcpy(&t.to[y * t.w], &p->still[y * GL_W], (size_t)(t.w < GL_W ? t.w : GL_W));
        else memset(&t.to[y * t.w], BLACK, (size_t)(t.w < GL_W ? t.w : GL_W));
    name(p);
    text_select(TEXT_RED);
    text_draw_smooth("Car Showcase", GL_HEADING_X, GL_HEADING_Y, GL_HEADING_NUM, GL_HEADING_DEN);
    figures(p);
    const GalleryState *s = &p->state;
    const int on = s->fixed || added_gallery_colour_on(&p->card, s);
    box(&t, GL_SKIN_Y, "Skin", s->skin == 0 ? "Default" : p->card.skin[s->skin], s->item == GALLERY_SKIN, 1, 0, s->fixed);
    box(&t, GL_COLOUR_Y, "Color", s->paint == 0 ? "Default" : p->card.info.paints[s->paint - 1].name, s->item == GALLERY_COLOUR, on,
        on && s->paint > 0, s->fixed);
    foot(p);
    bottom(&t, s->item);
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
    int x, y, step;
    pointer_position(&x, &y);
    const int item = gallery_hit(x, y, text_width(NEXT_WORD), text_width(DONE_WORD), &step);
    if (item < 0 || (s->fixed && item < GALLERY_PREVIOUS)) return 0;   /* a fixed page's boxes take no click */
    s->item = item;
    if (item == GALLERY_SKIN || item == GALLERY_COLOUR) return step < 0 ? GALLERY_KEY_LEFT : step > 0 ? GALLERY_KEY_RIGHT : 0;
    return GALLERY_KEY_ENTER;
}

/* the pick kept for `id`, as the page's state */
static void state_from_look(Page *p)
{
    const CarLook l = car_looks_get(p->card.info.id);
    p->state = (GalleryState){ GALLERY_SKIN, 0, 0, 0 };
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

/* GalleryRunOps over the page */
static void run_show(void *ctx, const GalleryState *s)
{
    Page *p = ctx;
    (void)s;
    ea_select_default();
    palette_now(p);
    text_build_styles_from(p->palette);   /* before the words: their colours are the page's */
    draw(p);
    video_flip();
    pointer_show();
    /* port (test harness): the page as shown, for a walk to wait on after each key (tests/car_card_menus.sh); the log only */
    port_log("mark:gallery=%d %d %d\n", p->state.item, p->state.skin, p->state.paint);   /* item skin paint: a WAIT's text is < 32 */
}
static int run_key(void *ctx) { (void)ctx; return input_get_key(); }
static int run_buttons(void *ctx) { (void)ctx; return pointer_read_buttons(); }
static int run_click(void *ctx, GalleryState *s) { (void)ctx; return click_key(s); }
static void run_music(void *ctx) { (void)ctx; menu_audio_music_service(); }
static void run_wait(void *ctx) { (void)ctx; video_wait_vblank(); }

/* one car's page until it is left or another is asked for: GALLERY_LEAVE, GALLERY_GO_PREVIOUS or GALLERY_GO_NEXT */
static int run_page(Page *p)
{
    const GalleryRunOps ops = { run_show, run_key, run_buttons, run_click, run_music, run_wait, p };
    return added_gallery_run(&p->card, &p->state, &ops);
}

int32_t cg_added_gallery(void *ctx, int16_t *car, int warrior, const uint8_t screen[768])
{
    (void)ctx;
    if (!text_small_init()) port_log("added gallery: the small font is not loaded\n");
    for (;;) {
        Page p;
        memset(&p, 0, sizeof p);
        if (*car == STOCK_CAR_WARRIOR) warrior_card(&p);   /* port (dep59 section 8): the Warrior's page */
        else if (!car_pieces_card(*car, &p.card)) return 0;
        else state_from_look(&p);
        CarList l;
        Edition e;   /* port (dep59 step 4): the arrows' cars are the edition's */
        car_list_build(&l, edition_active(&e) == 1 ? &e : NULL, warrior);
        p.car = *car;
        page_still(&p, screen);
        port_log("mark:screen=added_gallery\n");   /* port: the headless walk waits for it */
        const int result = run_page(&p);
        if (!p.warrior) look_keep(&p);   /* the Warrior's look is fixed: nothing kept */
        free(p.still);
        if (result == GALLERY_LEAVE) return 0;
        *car = car_list_step_gallery(&l, *car, result == GALLERY_GO_NEXT ? 1 : -1);
        if (!car_registry_is_added(*car) && *car != STOCK_CAR_WARRIOR) return 1;   /* an original's: its own gallery */
    }
}

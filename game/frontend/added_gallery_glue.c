/* port (dep59 step 3, PR B of the showcase redesign): an added car's gallery drawn and run (added_gallery.h holds its state and keys), laid out
 * like the Car Showcase pages of the CD (gallery_layout.h, added_gallery_draw.c). A page of its own palette: the still part (the clean
 * configuration background, the two photos in their tiles, the panels) quantised once, the photos' entries apart from the rest's, again when
 * another big photo is shown; the port's text colours (BOOT_TEXT_RGB), one entry for the Color swatch, and Control Central's last entries kept
 * as they were for the pointer. The bottom row as the CD showcase pages have it: DONE at the left, "< Next car >" in the middle. The pair
 * picked is kept by id as the page closes (car_looks.h). */
#include "game/frontend/added_gallery.h"
#include "game/frontend/added_gallery_page.h"
#include "game/frontend/added_gallery_run.h"

#include "formats/fsh.h"
#include "formats/pbs.h"
#include "formats/png.h"
#include "formats/showcase_tint.h"
#include "formats/stock_content.h"
#include "game/frontend/boot_quant.h"
#include "game/frontend/boot_screen.h"
#include "game/frontend/car_records_run.h"
#include "game/frontend/car_records_view.h"
#include "game/frontend/car_figures.h"
#include "game/frontend/car_list.h"
#include "game/frontend/central_glue_private.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/gallery_photo.h"
#include "game/frontend/showcase.h"
#include "game/frontend/menu_audio.h"
#include "game/platform/platform.h"
#include "game/race/car_registry.h"
#include "game/sys/car_looks.h"
#include "game/sys/edition_file.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SHUTTER_TICKS = 9 };
#define TILE_BANK "supra.qfs"   /* owner 2026-11-18: the closed SETUP tile's background is the Supra's (one place to change it) */
   /* vblanks a shutter frame stays (about 150 ms at 60 Hz) */

/* port (dep59 section 8, the owner's): the Warrior's page. Its year as the owner gave it; its power and weight from its physics
 * (CARSPECS/TRAFFC.PBS, the race's car_model_names[11]); its look fixed: the skin as it comes and its own dark purple, sampled from its
 * Control Central card (CONTROL/WAR1.QFS on Central's palette: the mean of the body's purple, #581C54); its picture that card */
static const char WARRIOR_YEAR[] = "1994", WARRIOR_SPECS[] = "SIMDATA/CARSPECS/TRAFFC.PBS", WARRIOR_CARD[] = "control/war1.qfs";
static const char WARRIOR_PAINT[] = "Purple";
static const uint8_t WARRIOR_RGB[3] = { 0x58, 0x1c, 0x54 };

/* port (owner 2026-11-17): the bottom row as the CD showcase pages have it, cut from the 512TR's showcase art (any car's has it; the
 * 512TR's has nothing else light in the row) once a gallery opens; `row_cut` 0 when it could not be read: no bottom row drawn */
static const char ROW_BANK[] = "512tr.qfs";
static GalleryRow row;
static int row_cut;

static uint8_t *tile_data;   /* the closed SETUP tile's bank, read once */
static size_t tile_size;

static uint8_t *flag_data;   /* WINNER.QFS, read once: the RECORDS tile's flag */
static size_t flag_size;
#define FLAG_BANK "winner.qfs"

static void flag_load(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s%s", resource_root(), resource_german() ? "frontend/gart/" : "frontend/art/", FLAG_BANK);
    if (flag_data == NULL) flag_data = resource_load(path, &flag_size);
    if (flag_data == NULL) port_log("added gallery: %s was not read\n", path);
}

static void tile_load(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s%s", resource_root(), resource_german() ? "frontend/gshow/" : "frontend/show/", TILE_BANK);
    if (tile_data == NULL) tile_data = resource_load(path, &tile_size);
    if (tile_data == NULL) port_log("added gallery: %s was not read\n", path);
}

static void row_load(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s%s", resource_root(), resource_german() ? "frontend/gshow/" : "frontend/show/", ROW_BANK);
    size_t size = 0;
    uint8_t *data = resource_load(path, &size);
    row_cut = data != NULL && gallery_row_cut(&row, data, size);
    if (!row_cut) port_log("added gallery: the bottom row was not cut from %s\n", path);
    resource_free(data);
}

/* the row's most used colours into their entries, the rest to the nearest of the page's palette (the pointer's and the swatch's apart) */
static void row_colours(Page *p)
{
    int taken[GL_ROW_COLOURS] = { 0 };
    for (int e = 0; e < ROW_EXACT; e++) {
        int best = -1;
        for (int k = 0; k < row.colours; k++)
            if (!taken[k] && (best < 0 || row.count[k] > row.count[best])) best = k;
        if (best < 0) break;
        taken[best] = 1;
        memcpy(p->palette + 3 * (ROW_FIRST + e), row.rgb[best], 3);
        p->row_map[best] = (uint8_t)(ROW_FIRST + e);
    }
    uint8_t usable[256];
    for (int i = 0; i < 256; i++) usable[i] = i < 256 - KEPT && i != SWATCH && i != SKIN_SWATCH;
    for (int k = 0; k < row.colours; k++)
        if (!taken[k]) p->row_map[k] = (uint8_t)palette_nearest(p->palette, usable, row.rgb[k][0], row.rgb[k][1], row.rgb[k][2]);
}

/* `name` of the piece as RGB (malloc'd, *w x *h); NULL when it is not there, is not a PNG or is outside the photos' sizes (car_piece.h) */
static uint8_t *photo_rgb(const char *dir, const char *name, int *w, int *h)
{
    char path[CAR_PIECE_DIR_MAX + 64];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    size_t size = 0;
    uint8_t *data = platform_read_file(path, &size);
    uint32_t pw = 0, ph = 0;
    const int sized = data != NULL && png_size(data, size, &pw, &ph) == PNG_OK;
    const int fits = sized && pw <= CAR_PIECE_PHOTO_MAX_W && ph <= CAR_PIECE_PHOTO_MAX_H;   /* before the pixels: a hostile size is never decoded */
    PngImage img;
    const int read = fits && png_read(data, size, &img) == PNG_OK;
    if (data != NULL) platform_free_file(data);
    if (sized && !fits) port_log("car piece: %s is larger than %dx%d: not shown\n", name, CAR_PIECE_PHOTO_MAX_W, CAR_PIECE_PHOTO_MAX_H);
    if (!read) return NULL;
    uint8_t *rgb = img.width == pw && img.height == ph ? malloc((size_t)pw * ph * 3) : NULL;
    for (size_t i = 0; rgb != NULL && i < (size_t)pw * ph; i++)
        memcpy(rgb + 3 * i, img.indexed ? img.palette + 3 * img.pixels[i] : img.pixels + 4 * i, 3);
    png_free(img.pixels);
    *w = (int)pw, *h = (int)ph;
    return rgb;
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

/* the Warrior's Control Central card (an 8-bit picture on Central's palette `screen`) as RGB (malloc'd, *w x *h); NULL when it cannot be read */
static uint8_t *warrior_picture(const uint8_t screen[768], int *w, int *h)
{
    size_t size = 0;
    uint8_t *data = resource_load_art(WARRIOR_CARD, &size), *rgb = NULL;
    Fsh fsh;
    uint32_t index;
    FshEntry e;
    const int ok = data != NULL && fsh_open(&fsh, data, size) == FSH_OK && fsh_find(&fsh, "0000", &index) == FSH_OK &&
                   fsh_entry(&fsh, index, &e) == FSH_OK && e.width > 0 && e.height > 0 && e.payload_size >= (size_t)e.width * e.height;
    if (ok && (rgb = malloc((size_t)e.width * e.height * 3)) != NULL) {
        for (int i = 0; i < e.width * e.height; i++) memcpy(rgb + 3 * (size_t)i, screen + 3 * e.payload[i], 3);
        *w = e.width, *h = e.height;
    } else if (!ok) {
        port_log("added gallery: the Warrior's card %s was not read\n", WARRIOR_CARD);
    }
    resource_free(data);
    return rgb;
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
    p->state = (GalleryState){ GALLERY_NEXT, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
}

/* a tile's picture (dw x dh x 3) from `src` (w x h) cropped to fill, else the placeholder on `ground` */
static void tile_picture(const uint8_t *src, int w, int h, const uint8_t *ground, uint8_t *dst, int dw, int dh)
{
    if (src == NULL || !gallery_cover(src, w, h, dst, dw, dh)) gallery_placeholder(ground, GL_W, GL_H - GL_COL_Y, dst, dw, dh);
}

/* the big photos' names: photos= or, with none, the piece's gallery.png else card.png (the pictures it had before photos=); `names` of 8 */
static int big_names(const CarPieceCard *c, const char *names[MOD_INI_PHOTOS_MAX])
{
    int n = 0;
    for (; n < c->info.photo_count && n < MOD_INI_PHOTOS_MAX; n++) names[n] = c->info.photo[n];
    return n;
}

/* how many big photos the page has: photos=, or one (the piece's own picture, a placeholder when it has none) */
static int photo_total(const CarPieceCard *c, int warrior)
{
    const char *names[MOD_INI_PHOTOS_MAX];
    return warrior ? 1 : big_names(c, names) > 0 ? big_names(c, names) : 1;
}

/* the big photo number `n` and the small one as RGB (malloc'd); NULL where there is none: the page's placeholder */
static uint8_t *big_rgb(const Page *p, int n, int *w, int *h)
{
    if (p->warrior) return warrior_picture(p->screen, w, h);
    const char *names[MOD_INI_PHOTOS_MAX];
    if (big_names(&p->card, names) > 0) return photo_rgb(p->card.dir, names[n], w, h);
    uint8_t *rgb = photo_rgb(p->card.dir, "gallery.png", w, h);
    return rgb != NULL ? rgb : photo_rgb(p->card.dir, "card.png", w, h);
}

static uint8_t *small_rgb(const Page *p, int *w, int *h)
{
    if (p->warrior) return warrior_picture(p->screen, w, h);
    return p->card.info.photo_small[0] != 0 ? photo_rgb(p->card.dir, p->card.info.photo_small, w, h) : photo_rgb(p->card.dir, "card.png", w, h);
}

/* the page's still part and palette: the big photo number p->state.photo, the small one, the background (Control Central's palette `screen`
 * gives the kept entries) */
static void page_still(Page *p)
{
    uint8_t *big = malloc((size_t)GL_PERF_W * GL_PERF_H * 3), *small = malloc((size_t)GL_VID_W * GL_VID_H * 3);
    uint8_t *bgnd = malloc((size_t)GL_W * GL_H * 3), *rgb = malloc((size_t)GL_W * GL_H * 3);
    free(p->still);
    p->still = calloc((size_t)GL_W * GL_H, 1);
    memset(p->palette, 0, sizeof p->palette);
    if (big != NULL && small != NULL && bgnd != NULL && rgb != NULL && p->still != NULL) {
        int w = 0, h = 0;
        background_rgb(bgnd);
        if (p->has_tint) showcase_tint_apply(bgnd, (size_t)GL_W * GL_H, p->tint);   /* owner 2026-11-18: the mod's first paint tints the sepia */
        const uint8_t *ground = bgnd + 3 * (size_t)GL_COL_Y * GL_W;
        uint8_t *src = big_rgb(p, p->state.photo, &w, &h);
        tile_picture(src, w, h, ground, big, GL_PERF_W, GL_PERF_H);
        free(src);
        if (flag_data == NULL || !added_gallery_flag_rgb(small, flag_data, flag_size)) {   /* the flag; the card's picture when it cannot be had */
            src = small_rgb(p, &w, &h);
            tile_picture(src, w, h, ground, small, GL_VID_W, GL_VID_H);
            free(src);
        }
        gallery_compose(bgnd, big, small, rgb);
        if (!gallery_quantise(rgb, STILL_COLOURS, p->palette, p->still)) port_log("added gallery: the page was not quantised\n");
    }
    free(big), free(small), free(bgnd), free(rgb);
    p->shown = p->state.photo;
    memcpy(p->palette + 3 * TEXT_FIRST, BOOT_TEXT_RGB, sizeof BOOT_TEXT_RGB);
    memcpy(p->palette + 3 * (256 - KEPT), p->screen + 3 * (256 - KEPT), 3 * KEPT);
    uint8_t usable[256];
    for (int i = 0; i < 256; i++) usable[i] = i < STILL_COLOURS;
    p->panel_fill = palette_nearest(p->palette, usable, 0x2a, 0x1c, 0x0e);   /* the panel's dark sepia, the still part's nearest */
    if (row_cut) row_colours(p);
    p->tile_ok = tile_data != NULL && added_gallery_setup_tile(p->tile, tile_data, tile_size, p->palette, usable);
    if (!p->tile_ok) port_log("added gallery: the SETUP tile's picture was not made: a flat tile\n");
    free(p->art);
    p->art = calloc(1, sizeof *p->art);
    size_t size = 0;
    uint8_t *file = p->art != NULL ? resource_load_art("option/loadanim.qfs", &size) : NULL;
    if (file == NULL) port_log("added gallery: option/loadanim.qfs was not read: the panel is flat\n");
    else if (!added_gallery_setup_art(p->art, file, size, p->palette, usable)) port_log("added gallery: the panel's stone was not made: the panel is flat\n");
    free(file);
}

/* the bottom row as the CD showcase pages have it (owner 2026-11-17): DONE, Previous or Next lit when it is the item */
static void bottom(const Page *p, uint8_t *to, int w, int h)
{
    if (row_cut) gallery_row_draw(&row, gallery_row_look(p->state.item), to, w, h, p->row_map, BLACK);
}

/* the page into the back buffer */
static void draw(const Page *p)
{
    int w, h;
    uint8_t *to = video_target(&w, &h);
    for (int y = 0; y < GL_H && y < h; y++)
        if (p->still != NULL) memcpy(&to[y * w], &p->still[y * GL_W], (size_t)(w < GL_W ? w : GL_W));
        else memset(&to[y * w], BLACK, (size_t)(w < GL_W ? w : GL_W));
    added_gallery_draw_title(p);
    added_gallery_draw_tiles(p, to, w, h);
    bottom(p, to, w, h);
}

/* the swatch's colour into the page's palette */
static void palette_fill(Page *p)
{
    const GalleryState *s = &p->state;
    const uint8_t *rgb = s->paint > 0 ? p->card.info.paints[s->paint - 1].rgb : p->default_rgb;
    memcpy(p->palette + 3 * SWATCH, rgb, 3);
    memcpy(p->palette + 3 * SKIN_SWATCH, p->skin_rgb[s->skin], 3);
}

/* a click: the key it stands for, the item moved to the one clicked; 0 for none. With the SETUP panel open a click on a row's value steps it
 * and one outside the panel only closes it */
static int click_key(GalleryState *s)
{
    int x, y, step;
    pointer_position(&x, &y);
    if (s->setup) {
        const int row = gallery_setup_hit(x, y, &step);
        if (row < 0) return GALLERY_KEY_ESCAPE;
        s->row = row;
        return step < 0 ? GALLERY_KEY_LEFT : step > 0 ? GALLERY_KEY_RIGHT : 0;
    }
    const int item = gallery_hit(x, y, &step);
    if (item < 0) return 0;
    if (item == GALLERY_PHOTOS && s->photos < 2) return 0;   /* a photo that is alone has nothing to step */
    s->item = item;
    if (item == GALLERY_PHOTOS) return step < 0 ? GALLERY_KEY_LEFT : GALLERY_KEY_RIGHT;   /* its middle goes on to the next photo */
    return item == GALLERY_HISTORY ? 0 : GALLERY_KEY_ENTER;                                /* SETUP opens, the bottom row's act */
}

/* the pick kept for `id`, as the page's state */
static void state_from_look(Page *p)
{
    const CarLook l = car_looks_get(p->card.info.id);
    p->state = (GalleryState){ GALLERY_SKIN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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
static void run_clear(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}
static void run_black(void *ctx)
{
    (void)ctx;
    static const uint8_t BLACK_PALETTE[768];
    video_set_palette(0, 256, BLACK_PALETTE);
}
static void run_draw(void *ctx, const GalleryState *s)
{
    Page *p = ctx;
    if (p->shown != s->photo) page_still(p);   /* another big photo: the still part and its palette made again */
    ea_select_default();
    /* the words' colours are the page's, not the previews': the text styles pick their ramps from a palette whose two preview entries are black,
     * so the words look the same whatever colour or skin the panel shows */
    uint8_t pal[768];
    memcpy(pal, p->palette, sizeof pal);
    memcpy(pal + 3 * SWATCH, pal + 3 * BLACK, 3);
    memcpy(pal + 3 * SKIN_SWATCH, pal + 3 * BLACK, 3);
    text_build_styles_from(pal);
    palette_fill(p);
    draw(p);
}
static void run_palette(void *ctx)
{
    const Page *p = ctx;
    video_set_palette(0, 256, p->palette);
}
static void run_flip(void *ctx, const GalleryState *s)
{
    (void)ctx;
    video_flip();
    pointer_show();
    /* port (test harness): the page as shown, for a walk to wait on after each key (tests/car_card_menus.sh); the log only */
    port_log("mark:gallery=%d %d %d\n", s->item, s->skin, s->paint);   /* item skin paint: a WAIT's text is < 32 */
}
static void run_fade_in(void *ctx)
{
    const Page *p = ctx;
    video_fade_in(0, 256, p->palette);
}
static void run_fade_out(void *ctx) { (void)ctx; video_fade_out(0, 256); }
static int run_key(void *ctx) { (void)ctx; return input_get_key(); }
static int run_buttons(void *ctx) { (void)ctx; return pointer_read_buttons(); }
static void run_pointer(void *ctx, int *x, int *y) { (void)ctx; pointer_position(x, y); }
static int run_click(void *ctx, GalleryState *s) { (void)ctx; return click_key(s); }
static void run_music(void *ctx) { (void)ctx; menu_audio_music_service(); }
static void run_wait(void *ctx) { (void)ctx; video_wait_vblank(); }
static void run_apply(void *ctx, const GalleryState *s)
{
    (void)ctx;
    cg_setup_put(s->transmission, s->abs, s->traction);   /* the panel's change is Options' */
}
static int run_wheel(void *ctx) { (void)ctx; return pointer_take_wheel(); }
static unsigned run_now(void *ctx) { (void)ctx; return platform_ticks_ms(); }
static int run_lines(void *ctx, int *visible)
{
    const Page *p = ctx;
    *visible = added_gallery_history_window();
    return p->line_count * added_gallery_history_line_px() + 2;
}

/* one car's page until it is left or another is asked for: GALLERY_LEAVE, GALLERY_GO_PREVIOUS or GALLERY_GO_NEXT */
/* the showcase's sound for a tile lit (showcase.c's MOVED) */
static void run_moved(void *ctx) { (void)ctx; menu_audio_sound(0x47); }

/* the panel's shutter: opn1 .. opn7 (closing: backwards), the sound at the start; the page drawn each frame, the open one by the run loop */
static void run_shutter(void *ctx, const GalleryState *s, int opening)
{
    Page *p = ctx;
    if (p->art == NULL || !p->art->ok) return;
    menu_audio_sound(0x49);
    p->moving = 1;
    for (int k = 0; k < SETUP_FRAMES; k++) {
        p->frame = opening ? k : SETUP_FRAMES - 1 - k;
        run_draw(p, s);
        video_flip();   /* no mark: the walks wait on the page as it settles, after the last frame */
        pointer_show();
        for (int w = 0; w < SHUTTER_TICKS; w++) run_music(p), run_wait(p);
    }
    p->moving = 0;
}

/* --- the RECORDS screen (owner 2026-11-18): the car's records on every track over the page, darkened --- */

typedef struct {
    const Page *page;
    const uint8_t *ground;
} RecordsHost;

/* one frame of the table: the ground, the car's name, the table, DONE, the page's palette */
static void records_show(void *ctx, const CarRecordsView *v)
{
    const RecordsHost *h = ctx;
    const Page *p = h->page;
    int w, hh;
    ea_select_default();
    uint8_t *to = video_target(&w, &hh);
    for (int y = 0; y < GL_H && y < hh; y++) memcpy(&to[y * w], &h->ground[y * GL_W], (size_t)(w < GL_W ? w : GL_W));
    added_gallery_draw_name(p);
    car_records_view_draw(v, resource_kmh());
    if (row_cut) {
        gallery_row_draw(&row, gallery_row_look(GALLERY_DONE), to, w, hh, p->row_map, BLACK);   /* DONE only: the car arrows' place is the ground again */
        for (int y = SHOWCASE_BOTTOM_TOP; y < GL_H && y < hh; y++)
            for (int x = SHOWCASE_PREVIOUS_LEFT - 30; x <= SHOWCASE_NEXT_RIGHT + 30 && x < w; x++) to[y * w + x] = h->ground[y * GL_W + x];
    }
    video_set_palette(0, 256, p->palette);
    video_flip();
    pointer_show();
}

/* the screen until DONE: the page darkened under the table */
static void records_screen(Page *p)
{
    CarRecordsView *v = malloc(sizeof *v);
    uint8_t *ground = malloc((size_t)GL_W * GL_H), usable[256];
    TrackList l;
    if (v == NULL || ground == NULL) {
        free(v), free(ground);
        return;
    }
    cg_records_tracks(&l);
    car_records_view_make(v, p->card.info.id, &l);
    memcpy(ground, p->still, (size_t)GL_W * GL_H);
    for (int i = 0; i < 256; i++) usable[i] = i < STILL_COLOURS;
    car_records_ground(ground, p->palette, usable, p->has_tint ? p->tint : NULL);
    uint8_t pal[768];
    memcpy(pal, p->palette, sizeof pal);
    memcpy(pal + 3 * SWATCH, pal + 3 * BLACK, 3);
    memcpy(pal + 3 * SKIN_SWATCH, pal + 3 * BLACK, 3);
    text_build_styles_from(pal);
    const RecordsHost host = { p, ground };
    car_records_run(v, records_show, (void *)&host);
    free(v), free(ground);
}

static int run_page(Page *p)
{
    const GalleryRunOps ops = { run_clear, run_black, run_draw, run_palette, run_flip, run_fade_in, run_fade_out,
                                run_key, run_buttons, run_pointer, run_click, run_music, run_wait, run_wheel, run_now, run_lines, run_apply, run_shutter, run_moved, p };
    return added_gallery_run(&p->card, &p->state, &ops);
}

int32_t cg_added_gallery(void *ctx, int16_t *car, int warrior, const uint8_t screen[768], int entry)
{
    (void)ctx;
    if (!text_small_init()) port_log("added gallery: the small font is not loaded\n");
    row_load();
    tile_load();
    flag_load();
    int resume_records = 0;   /* the page comes back on the RECORDS tile after its screen */
    for (;;) {
        Page p;
        memset(&p, 0, sizeof p);
        if (*car == STOCK_CAR_WARRIOR) warrior_card(&p);   /* port (dep59 section 8): the Warrior's page */
        else if (!car_pieces_card(*car, &p.card)) return 0;
        else state_from_look(&p);
        added_gallery_enter(&p.state, entry);   /* owner 2026-11-17: paging goes on from the item it was done by */
        if (resume_records) p.state.item = GALLERY_RECORDS, resume_records = 0;
        CarList l;
        Edition e;   /* port (dep59 step 4): the arrows' cars are the edition's */
        car_list_build(&l, edition_active(&e) == 1 ? &e : NULL, warrior);
        p.car = *car;
        p.state.photos = photo_total(&p.card, p.warrior);
        cg_setup_get(&p.state.transmission, &p.state.abs, &p.state.traction);   /* Options' settings, for the panel */
        memcpy(p.screen, screen, sizeof p.screen);
        added_gallery_history_make(&p);
        added_gallery_tints_make(&p);   /* before the still part: the tint is its background's */
        page_still(&p);
        port_log("mark:screen=added_gallery\n");   /* port: the headless walk waits for it */
        const int result = run_page(&p);
        if (!p.warrior) look_keep(&p);   /* the Warrior's look is fixed: nothing kept */
        if (result == GALLERY_OPEN_RECORDS) {   /* owner 2026-11-18: the car's records, then the page again on the same tile */
            records_screen(&p);
            free(p.still), free(p.art);
            entry = 0, resume_records = 1;
            continue;
        }
        free(p.still), free(p.art);
        if (result == GALLERY_LEAVE) return 0;
        entry = result == GALLERY_GO_NEXT ? 1 : -1;
        *car = car_list_step_gallery(&l, *car, entry);
        if (!car_registry_is_added(*car) && *car != STOCK_CAR_WARRIOR) return entry;   /* an original's: its own gallery, entered by this step */
    }
}

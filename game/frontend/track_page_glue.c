/* port (dep59 step 5, T7d): an added track's page drawn and run (track_page.h holds its state and keys, track_page_layout.h its look). A
 * page of its own palette, as the added car's gallery has (added_gallery_glue.c): the still part (the configuration screens' sepia
 * background, turned to the mod's showcase_tint when it gives one, the big box with the track's card) quantised once, the card's colours
 * apart from the rest's; the port's text colours (BOOT_TEXT_RGB); Control Central's last entries kept for the pointer; the bottom row the
 * CD's track page has, cut from CITYSEL.QFS. The rows at the left are the settings the race reads, written back as the page closes. */
#include "game/frontend/track_page.h"
#include "game/frontend/track_page_layout.h"
#include "game/frontend/track_page_row.h"

#include "formats/fsh.h"
#include "formats/png.h"
#include "game/frontend/added_card.h"
#include "game/frontend/added_card_text.h"
#include "game/frontend/boot_quant.h"
#include "game/frontend/boot_screen.h"
#include "game/frontend/central_glue_private.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/gallery_layout.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/text_wrap.h"
#include "game/platform/platform.h"
#include "game/race/weather.h"
#include "game/sys/edition_file.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/track_registry.h"
#include "game/sys/video.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    KEPT = 18,                                       /* Control Central's last entries, kept for the pointer */
    TEXT_FIRST = 256 - KEPT - BOOT_TEXT_COLOURS,     /* the text's colours (BOOT_TEXT_RGB's order) */
    ROW_EXACT = 8, ROW_FIRST = TEXT_FIRST - ROW_EXACT,   /* the bottom row's most used colours, the CD art's own; its others the nearest */
    STILL_COLOURS = ROW_FIRST, CARD_COLOURS = 100,   /* the still part's: the card's 0 .. CARD_COLOURS - 1, the rest's after them */
    BLACK = TEXT_FIRST + 5,
    LEAN_NUM = 2, LEAN_DEN = 9,                      /* the title's slant, as the car page's name */
    TITLE_X = 12, TITLE_Y = 8, TITLE_X1 = TPL_W - 12,   /* the title has its whole strip: the box starts under it */
    ROW_LABEL_X = 6, SWITCH_VALUE_X = 0x82, VALUE_X = 0xc3, LENGTH_Y = 363, LENGTH_VALUE_X = 0xaf,
    DESC_LINES_MAX = 64, DESC_PAD = 4, NIGHT_DY = 3   /* the night's text on the time row, the CD's: 0x185 */
};

static const char CITY_BANK[] = "tracksel/citysel.qfs";   /* a stock track's page: its bottom row is every one's */
static TrackRow row;
static TrackWords words;   /* the bank's own words (the German bank's are German) */
static int row_cut, words_cut;

typedef struct {
    TrackPage page;
    TrackPageValues opened;           /* the values it opened with: only a row the player changed is written back */
    int track;                        /* the added track's number */
    const TrackPiece *piece;
    int line;                         /* the description's first visible line */
    uint8_t *still;                   /* the still part in the page's palette, TPL_W x TPL_H */
    uint8_t palette[768];
    uint8_t row_map[TR_COLOURS];      /* the bottom row's colours' entries */
    uint8_t words_map[TR_COLOURS];    /* the bank's words' colours' entries */
} Page;

static void row_load(void)
{
    size_t size = 0;
    uint8_t *data = resource_load_art(CITY_BANK, &size);
    row_cut = data != NULL && track_row_cut(&row, data, size);
    if (!row_cut) port_log("track page: the bottom row was not cut from %s\n", CITY_BANK);
    words_cut = data != NULL && track_words_cut(&words, data, size);
    if (!words_cut) port_log("track page: the bank's words were not cut from %s: the page says its own\n", CITY_BANK);
    resource_free(data);
}

/* the row's most used colours into their entries, the rest to the nearest of the page's palette (the pointer's apart) */
static void row_colours(Page *p)
{
    int taken[TR_COLOURS] = { 0 };
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
    for (int i = 0; i < 256; i++) usable[i] = i < 256 - KEPT;
    for (int k = 0; k < row.colours; k++)
        if (!taken[k]) p->row_map[k] = (uint8_t)palette_nearest(p->palette, usable, row.rgb[k][0], row.rgb[k][1], row.rgb[k][2]);
    for (int k = 0; k < words.colours; k++)
        p->words_map[k] = (uint8_t)palette_nearest(p->palette, usable, words.rgb[k][0], words.rgb[k][1], words.rgb[k][2]);
}

/* the configuration screens' clean background as RGB; a flat brown when it cannot be composed */
static void background_rgb(uint8_t *rgb)
{
    uint8_t *pixels = malloc((size_t)TPL_W * TPL_H), palette[768];
    const int ok = pixels != NULL && config_bgnd_load(resource_load_art, CONFIG_BGND_OPTIONS, 0, pixels, palette);
    if (!ok) port_log("track page: no clean background: a flat one\n");
    for (int i = 0; i < TPL_W * TPL_H; i++) {
        static const uint8_t FLAT[3] = { 58, 44, 30 };
        memcpy(rgb + 3 * i, ok ? palette + 3 * pixels[i] : FLAT, 3);
    }
    free(pixels);
}

/* the track's card as RGB (TPL_CARD_W x TPL_CARD_H x 3), its name over it as Control Central's has it: its folder's card.png or the flat card */
static void card_rgb(const TrackPiece *t, uint8_t *rgb)
{
    PngImage img;
    int have = 0;
    if (t->dir[0] != '\0') {
        char path[sizeof t->dir + 16];
        snprintf(path, sizeof path, "%s/card.png", t->dir);
        size_t size = 0;
        uint8_t *data = platform_read_file(path, &size);
        if (data != NULL) {
            have = png_read(data, size, &img) == PNG_OK;
            platform_free_file(data);
        }
    }
    char why[160] = "", text[MOD_INI_NAME_MAX + 4];
    AddedCardName name;
    uint8_t *mask = NULL;
    const int named = !t->card_name_off && added_track_card_name_letters(t->name, &name, &mask, text, (int)sizeof text);
    uint8_t *rgba = added_track_card_rgba(have ? &img : NULL, named ? &name : NULL, why, sizeof why);
    if (have) png_free(img.pixels);
    free(mask);
    if (rgba == NULL) {
        port_log("track page: %s: %s\n", t->id, why);
        memset(rgb, 0, (size_t)TPL_CARD_W * TPL_CARD_H * 3);
        return;
    }
    for (int i = 0; i < TPL_CARD_W * TPL_CARD_H; i++) memcpy(rgb + 3 * i, rgba + 4 * i, 3);
    free(rgba);
}

/* `rgb` (the still part) into `pixels` (TPL_W x TPL_H): the card's box to entries 0 .. CARD_COLOURS - 1, the rest to the entries after them */
static int quantise(const uint8_t *rgb, uint8_t palette[768], uint8_t *pixels)
{
    enum { CARD = TPL_CARD_W * TPL_CARD_H, REST = TPL_W * TPL_H - CARD };
    uint8_t *card = malloc((size_t)CARD * 3), *rest = malloc((size_t)REST * 3), *card_i = malloc(CARD), *rest_i = malloc(REST);
    const int ok = card != NULL && rest != NULL && card_i != NULL && rest_i != NULL;
    memset(pixels, 0, (size_t)TPL_W * TPL_H);
    if (ok) {
        int nc = 0, nr = 0;
        for (int y = 0; y < TPL_H; y++)
            for (int x = 0; x < TPL_W; x++) {
                const int in_card = x >= TPL_CARD_X && x < TPL_CARD_X + TPL_CARD_W && y >= TPL_CARD_Y && y < TPL_CARD_Y + TPL_CARD_H;
                memcpy(in_card ? card + 3 * nc++ : rest + 3 * nr++, rgb + 3 * (y * TPL_W + x), 3);
            }
        uint8_t rest_palette[768];
        boot_quant(card, CARD, CARD_COLOURS, palette, card_i);
        boot_quant(rest, REST, STILL_COLOURS - CARD_COLOURS, rest_palette, rest_i);
        memcpy(palette + 3 * CARD_COLOURS, rest_palette, (size_t)(STILL_COLOURS - CARD_COLOURS) * 3);
        nc = nr = 0;
        for (int y = 0; y < TPL_H; y++)
            for (int x = 0; x < TPL_W; x++) {
                const int in_card = x >= TPL_CARD_X && x < TPL_CARD_X + TPL_CARD_W && y >= TPL_CARD_Y && y < TPL_CARD_Y + TPL_CARD_H;
                pixels[y * TPL_W + x] = in_card ? card_i[nc++] : (uint8_t)(CARD_COLOURS + rest_i[nr++]);
            }
    }
    free(card), free(rest), free(card_i), free(rest_i);
    return ok;
}

/* the page's still part and palette (Control Central's palette `screen` gives the kept entries) */
static void page_still(Page *p, const uint8_t screen[768])
{
    uint8_t *bgnd = malloc((size_t)TPL_W * TPL_H * 3), *rgb = malloc((size_t)TPL_W * TPL_H * 3);
    uint8_t *card = malloc((size_t)TPL_CARD_W * TPL_CARD_H * 3);
    p->still = calloc((size_t)TPL_W * TPL_H, 1);
    memset(p->palette, 0, sizeof p->palette);
    if (bgnd != NULL && rgb != NULL && card != NULL && p->still != NULL) {
        background_rgb(bgnd);
        card_rgb(p->piece, card);
        track_page_compose(bgnd, card, p->piece->tinted ? p->piece->tint : NULL, rgb);
        if (!quantise(rgb, p->palette, p->still)) port_log("track page: the page was not quantised\n");
    }
    free(bgnd), free(rgb), free(card);
    memcpy(p->palette + 3 * TEXT_FIRST, BOOT_TEXT_RGB, sizeof BOOT_TEXT_RGB);
    memcpy(p->palette + 3 * (256 - KEPT), screen + 3 * (256 - KEPT), 3 * KEPT);
    if (row_cut) row_colours(p);
}

static int small_width(void *ctx, const char *s, int len)
{
    (void)ctx;
    char run[MOD_INI_DESCRIPTION_MAX + 1];
    snprintf(run, sizeof run, "%.*s", len, s);
    return text_small_width(run);
}

/* the track's name big and leant, white, in capitals; smaller when it would not fit the screen */
static void title(const TrackPiece *t)
{
    char said[MOD_INI_NAME_MAX + 4];
    text_words(t->name, said, sizeof said);   /* the main face: its ',' is a comma */
    for (char *c = said; *c != '\0'; c++) *c = (char)toupper((unsigned char)*c);
    static const int SIZES[3][2] = { { 2, 1 }, { 3, 2 }, { 1, 1 } };
    for (int k = 0; k < 3; k++) {
        const int num = SIZES[k][0], den = SIZES[k][1], w = text_width_scaled(said, num, den) + 2, h = text_mask_rows(said, num, den);
        const int ow = w + h * LEAN_NUM / LEAN_DEN + 2;
        if (h <= 0 || (TITLE_X + ow > TITLE_X1 && k < 2)) continue;
        uint8_t *mask = malloc((size_t)w * h), *leant = malloc((size_t)ow * h);
        if (mask != NULL && leant != NULL && text_mask_scaled(said, num, den, mask, w, h)) {
            gallery_lean(mask, w, h, LEAN_NUM, LEAN_DEN, leant, ow);
            text_select(TEXT_WHITE);
            text_draw_mask(leant, ow, h, TITLE_X, TITLE_Y);
        }
        free(mask), free(leant);
        return;
    }
}

static const char *const TIME_NAME[4] = { "Midday", "Morning", "Evening", "Night" };   /* LIGHT_TIME_DAY, DAWN, DUSK, NIGHT */
static const char *const WEATHER_NAME[WEATHER_COUNT] = { "Clear", "Fog", "Rain", "Storm" };

/* a row of the left: its label, and its value where `value_x` says; lit when it is the item */
static void row_text(const Page *p, int item, const char *label, const char *value, int value_x)
{
    const int y = track_page_row_y(&p->page, item);
    if (y < 0) return;
    text_select(p->page.item == item ? TEXT_YELLOW : TEXT_CREAM);   /* the CD's rows: cream, the one lit yellow */
    text_draw_smooth(label, ROW_LABEL_X, y, 1, 1);
    text_draw_smooth(value, value_x, y, 1, 1);
}

static void rows(const Page *p)
{
    const TrackPageValues *v = &p->page.v;
    row_text(p, TP_MIRROR, "Mirror", v->mirror ? "On" : "Off", SWITCH_VALUE_X);
    row_text(p, TP_RALLY, "Rally", v->rally ? "On" : "Off", SWITCH_VALUE_X);
    row_text(p, TP_WEATHER, "Weather", WEATHER_NAME[v->weather % WEATHER_COUNT], VALUE_X);
    char said[MOD_INI_FIGURE_MAX + 1];
    if (text_small_words(p->piece->length, said, sizeof said) > 0) {   /* the CD's page's Track length row, its figure as the author wrote it */
        if (!words_cut) {
            text_select(TEXT_CREAM);
            text_draw_smooth("Track length", ROW_LABEL_X, LENGTH_Y, 1, 1);
        }
        text_select(TEXT_CREAM);
        text_draw_smooth(said, LENGTH_VALUE_X, LENGTH_Y, 1, 1);
    }
}

/* the bank's own words (its language's): the Track length label, the time of day's label and value; the night is the game's text */
static void words_draw(const Page *p, uint8_t *to, int w, int h)
{
    if (!words_cut) {
        const int y = track_page_row_y(&p->page, TP_TIME);
        if (y >= 0) row_text(p, TP_TIME, "Time of day", TIME_NAME[track_page_time_shown(&p->page.offer, p->page.v.light_time) & 3], VALUE_X);
        return;
    }
    char said[MOD_INI_FIGURE_MAX + 1];
    if (text_small_words(p->piece->length, said, sizeof said) > 0) track_words_draw(&words, TW_LENGTH, to, w, h, p->words_map, BLACK);
    if (!track_page_shown(&p->page, TP_TIME)) return;
    const int lit = p->page.item == TP_TIME, time = track_page_time_shown(&p->page.offer, p->page.v.light_time);
    track_words_draw(&words, lit ? TW_TIME_LIT : TW_TIME_DIM, to, w, h, p->words_map, BLACK);
    const int layer = track_words_time_layer(time, lit);
    if (layer >= 0) {
        track_words_draw(&words, layer, to, w, h, p->words_map, BLACK);
    } else {
        text_select(lit ? TEXT_YELLOW : TEXT_CREAM);
        text_draw_smooth("Night", VALUE_X, track_page_row_y(&p->page, TP_TIME) - 1 + NIGHT_DY, 1, 1);
    }
}

/* the name, author and length under the card, and the description in its window, a line each step (track_page_scroll_line) */
static void box(const Page *p)
{
    char said[MOD_INI_DESCRIPTION_MAX + 1], line[200];
    text_select(TEXT_WHITE);
    text_words(p->piece->name, said, sizeof said);
    text_draw_smooth(said, TPL_TEXT_X, TPL_NAME_Y, 1, 1);
    text_select(TEXT_YELLOW);
    if (text_small_words(p->piece->author, said, sizeof said) > 0) {
        snprintf(line, sizeof line, "BY %s", said);
        text_small_draw(line, TPL_TEXT_X, TPL_AUTHOR_Y);
    }
    if (text_small_words(p->piece->length, said, sizeof said) > 0) {
        snprintf(line, sizeof line, "LENGTH %s", said);
        text_small_draw(line, TPL_TEXT_X, TPL_LENGTH_Y);
    }
    TextLine lines[DESC_LINES_MAX];
    text_small_words(p->piece->description, said, sizeof said);
    const int n = text_wrap(said, TPL_TEXT_X1 - TPL_TEXT_X - 2 * DESC_PAD, small_width, NULL, lines, DESC_LINES_MAX);
    const int pitch = text_small_line_height(), visible = (TPL_DESC_Y1 - TPL_DESC_Y0) / pitch;
    text_select(TEXT_CREAM);
    for (int i = 0; i < visible && p->line + i < n; i++) {
        const TextLine *l = &lines[p->line + i];
        snprintf(line, sizeof line, "%.*s", l->len, said + l->start);
        text_small_draw(line, TPL_TEXT_X + DESC_PAD, TPL_DESC_Y0 + i * pitch);
    }
}

/* the description's line count and window, for the scroll */
static void description_size(const Page *p, int *total, int *visible)
{
    char said[MOD_INI_DESCRIPTION_MAX + 1];
    TextLine lines[DESC_LINES_MAX];
    text_small_words(p->piece->description, said, sizeof said);
    *total = text_wrap(said, TPL_TEXT_X1 - TPL_TEXT_X - 2 * DESC_PAD, small_width, NULL, lines, DESC_LINES_MAX);
    *visible = (TPL_DESC_Y1 - TPL_DESC_Y0) / text_small_line_height();
}

/* the page into the back buffer */
static void draw(const Page *p)
{
    int w, h;
    uint8_t *to = video_target(&w, &h);
    for (int y = 0; y < TPL_H && y < h; y++)
        if (p->still != NULL) memcpy(&to[y * w], &p->still[y * TPL_W], (size_t)(w < TPL_W ? w : TPL_W));
        else memset(&to[y * w], BLACK, (size_t)(w < TPL_W ? w : TPL_W));
    title(p->piece);
    rows(p);
    words_draw(p, to, w, h);
    box(p);
    if (row_cut) track_row_draw(&row, track_row_look(p->page.item), to, w, h, p->row_map, BLACK);
}

static void run_clear(void) { for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip(); }

static void run_black(void)
{
    static const uint8_t BLACK_PALETTE[768];
    video_set_palette(0, 256, BLACK_PALETTE);
}

static void run_draw(Page *p)
{
    ea_select_default();
    text_build_styles_from(p->palette);   /* before the words: their colours are the page's */
    draw(p);
}

static void run_flip(const Page *p)
{
    video_flip();
    pointer_show();
    const TrackPageValues *v = &p->page.v;
    port_log("mark:tpage=%d %d%d%d%d\n", p->page.item, v->mirror, v->rally, v->light_time, v->weather);   /* port (test harness): a WAIT's text is < 32 */
}

/* the keys and the pointer's button let go (or the frames gone) before the page closes: a press it acted on, still held, is not taken by
 * the screen after it */
static void let_go(void)
{
    for (int i = 0; i < 300; i++) {
        const int key = input_get_key();   /* first: it pumps the events the buttons are read from */
        if (key == 0 && (pointer_read_buttons() & POINTER_BUTTON_SELECT) == 0) return;
        menu_audio_music_service();
        video_wait_vblank();
    }
}

/* the page until it is left or another track or Best times is asked for: TRACK_PAGE_LEAVE, _GO_PREVIOUS, _GO_NEXT or _BEST */
static int run_page(Page *p)
{
    run_clear();
    run_black();
    run_draw(p);
    run_flip(p);
    video_fade_in(0, 256, p->palette);
    int last_key = input_get_key(), last_buttons = -1, result = TRACK_PAGE_STAY, changed = 0, last_x = 0, last_y = 0, frame = 0;
    pointer_position(&last_x, &last_y);
    while (result == TRACK_PAGE_STAY) {
        int total, visible;
        description_size(p, &total, &visible);
        const int line = track_page_scroll_line(frame++, total, visible);
        if (line != p->line) p->line = line, changed = 1;
        if (changed) {
            run_draw(p);
            video_set_palette(0, 256, p->palette);
            run_flip(p);
            changed = 0;
        }
        const int key = input_get_key(), buttons = pointer_read_buttons();
        int press = key != last_key && key != 0 ? key : 0;
        int x, y;
        pointer_position(&x, &y);
        if (buttons != last_buttons && last_buttons >= 0) {
            if (buttons & POINTER_BUTTON_SELECT) {
                const int item = track_page_hit(&p->page, x, y);
                if (item >= 0) p->page.item = item, press = TRACK_PAGE_KEY_ENTER, changed = 1;
            } else if (buttons & POINTER_BUTTON_BACK) {
                press = TRACK_PAGE_KEY_ESCAPE;
            }
        }
        last_key = key, last_buttons = buttons;
        if (x != last_x || y != last_y) {   /* the item under the pointer lit, as the CD's page lights its own */
            const int item = track_page_hit(&p->page, x, y);
            if (item >= 0 && item != p->page.item) p->page.item = item, changed = 1;
        }
        last_x = x, last_y = y;
        if (press != 0) {
            const TrackPage was = p->page;
            result = track_page_input(&p->page, press);
            changed |= memcmp(&was, &p->page, sizeof was) != 0;
        }
        menu_audio_music_service();   /* the showcase's music plays on here */
        video_wait_vblank();
    }
    let_go();
    video_fade_out(0, 256);
    return result;
}

/* 1 when the file exists, as its folder spells it (a zip's folders are any case: the name in upper case or lower) */
static int file_there(const char *dir, const char *folder, const char *name)
{
    for (int lower = 0; lower < 2; lower++) {
        char path[sizeof((TrackPiece *)0)->dir + 64], f[16], n[24];
        snprintf(f, sizeof f, "%s", folder), snprintf(n, sizeof n, "%s", name);
        for (char *c = f; lower && *c != '\0'; c++) *c = (char)tolower((unsigned char)*c);
        for (char *c = n; lower && *c != '\0'; c++) *c = (char)tolower((unsigned char)*c);
        snprintf(path, sizeof path, "%s/SIMDATA/%s/%s", dir, f, n);
        if (lower) snprintf(path, sizeof path, "%s/simdata/%s/%s", dir, f, n);
        FILE *fp = fopen(path, "rb");
        if (fp != NULL) return fclose(fp), 1;
    }
    return 0;
}

int cg_track_has_rally_art(const TrackPiece *t)
{
    for (int stage = 1; stage <= t->spec.segments; stage++) {
        char name[24];
        snprintf(name, sizeof name, "%s%d_R01.FAM", t->stem, stage);
        if (!file_there(t->dir, "NTRACKFM", name)) return 0;
    }
    return t->spec.segments > 0;
}

/* the page's values from the pick as it is now */
static void values_open(const TrackPiece *t, TrackPage *page)
{
    const TrackPageOffer offer = { t->spec.times, 0, port_light_effects(), 1, !cg_g.unlock_m };
    const TrackPageValues values = { cg_g.variant_m != 0, cg_g.variant_r != 0, port_light_time(), port_weather() };
    TrackPageOffer with = offer;
    with.rally = cg_g.unlock_r && cg_track_has_rally_art(t);
    track_page_open(page, &with, &values);
}


/* the values the page was left with into the pick (Central's settings and port.cfg), as the track screen puts its own */
static void values_keep(const TrackPage *page, const TrackPageValues *opened)
{
    const TrackPageValues *v = &page->v;
    const int kept_m = v->mirror, kept_r = v->rally;
    cg_g.variant_m = (int16_t)(page->offer.no_mirror ? cg_g.variant_m : v->mirror);
    cg_g.variant_r = (int16_t)(page->offer.rally ? v->rally : cg_g.variant_r);
    PortSettings saved = port_settings_load();
    const int light_time = track_page_time_shown(&page->offer, v->light_time);
    const int time_changed = track_page_shown(page, TP_TIME) && v->light_time != opened->light_time;
    const int weather_changed = track_page_shown(page, TP_WEATHER) && v->weather != opened->weather;
    if (time_changed) saved.light_time = light_time;
    if (weather_changed) saved.weather = v->weather;
    if ((time_changed || weather_changed) && !port_settings_save(&saved)) port_log("track page: port.cfg could not be saved\n");
    RaceSettings s = cg_settings_of();
    s.variant_m = cg_g.variant_m, s.variant_r = cg_g.variant_r;
    cg_settings_put(&s);
    cg_variant_choice_keep(kept_m, kept_r, NULL);
    cg_lunar_settle();
}

int32_t cg_track_page(void *ctx, const uint8_t screen[768])
{
    (void)ctx;
    if (!text_small_init()) port_log("track page: the small font is not loaded\n");
    row_load();
    int resume = -1;   /* the item the page comes back on after Best Times */
    for (;;) {
        const TrackPiece *t = track_registry_piece(cg_g.track);
        if (t == NULL) return 0;
        Page p;
        memset(&p, 0, sizeof p);
        p.track = cg_g.track, p.piece = t;
        values_open(t, &p.page);
        p.opened = p.page.v;
        if (resume >= 0) p.page.item = resume, resume = -1;
        page_still(&p, screen);
        port_log("mark:screen=track_page\n");   /* port: the headless walk waits for it */
        const int result = run_page(&p);
        if (result != TRACK_PAGE_CANCEL) values_keep(&p.page, &p.opened);   /* Escape: as the track screen's, nothing kept */
        free(p.still);
        if (result == TRACK_PAGE_BEST) {
            int x, y;
            pointer_position(&x, &y);
            DialogPointer dp = { pointer_active(), 0, x, y, x, y };
            cg_best_times_added(&dp, p.track);
            resume = TP_BEST;
            continue;
        }
        if (result == TRACK_PAGE_LEAVE || result == TRACK_PAGE_CANCEL) return 0;
        cg_track_arrow(result == TRACK_PAGE_GO_NEXT ? 1 : -1);   /* Central's own step: the page goes on when the next is an added track too */
        if (!track_registry_is_added(cg_g.track)) return 0;
    }
}

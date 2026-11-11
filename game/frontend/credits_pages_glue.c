/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: draws our credits pages (credits_pages.h) over the credits' relief without its text (credits_relief.h), composed once per run from the player's CD:
 * each page faded in, held until a key or a click (or about 12 seconds), faded out. Over black when the relief cannot be composed. */
#include "game/frontend/credits_pages.h"

#include "formats/fsh.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/credits_relief.h"
#include "game/frontend/ea_lib.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdlib.h>
#include <string.h>

enum { SCREEN_W = 640, SCREEN_H = 480, PALETTE_COLOURS = 256, PAGE_FRAMES = 300, RAMP = 128 };   /* video runs at 25 frames a second: 12 s a page */

static uint8_t *relief;   /* SCREEN_W x SCREEN_H, NULL when it could not be composed */
static uint8_t relief_pal[768];

/* credits.fsh's five pages and their palette, read off the player's CD; 1 when all are there */
static int read_pages(uint8_t **data, const uint8_t *pages[CREDITS_RELIEF_PAGES], uint8_t pal[768])
{
    size_t size = 0;
    *data = resource_load_art("credits.fsh", &size);
    Fsh fsh;
    FshEntry e;
    uint32_t idx;
    if (*data == NULL || fsh_open(&fsh, *data, size) != FSH_OK) return 0;
    if (fsh_find(&fsh, "!pal", &idx) != FSH_OK || fsh_entry(&fsh, idx, &e) != FSH_OK || fsh_palette_rgb(&e, pal) != FSH_OK) return 0;
    for (int p = 0; p < CREDITS_RELIEF_PAGES; p++) {
        const char tag[4] = { 'b', 'g', 'n', (char)('1' + p) };
        if (fsh_find(&fsh, tag, &idx) != FSH_OK || fsh_entry(&fsh, idx, &e) != FSH_OK) return 0;
        if (e.type != FSH_TYPE_BITMAP || e.width != SCREEN_W || e.height != SCREEN_H || e.payload_size < (size_t)SCREEN_W * SCREEN_H) return 0;
        pages[p] = e.payload;
    }
    return 1;
}

/* the relief, once per run */
static void relief_compose(void)
{
    static int tried;
    if (tried) return;
    tried = 1;
    uint8_t *data = NULL, page_pal[768], menu_pal[768];
    const uint8_t *pages[CREDITS_RELIEF_PAGES];
    uint8_t *const menu = malloc((size_t)SCREEN_W * SCREEN_H), *const menu_rgb = malloc((size_t)SCREEN_W * SCREEN_H * 3);
    relief = malloc((size_t)SCREEN_W * SCREEN_H);
    int ok = menu != NULL && menu_rgb != NULL && relief != NULL && read_pages(&data, pages, page_pal) &&
             config_bgnd_load(resource_load_art, CONFIG_BGND_OPTIONS, 0, menu, menu_pal);
    if (ok) {
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) memcpy(&menu_rgb[3 * i], &menu_pal[3 * menu[i]], 3);
        ok = credits_relief_compose(pages, page_pal, menu_rgb, relief, relief_pal);
    }
    if (!ok) {
        port_log("credits: no relief for our pages (credits.fsh or the configuration screens), they go over black\n");
        free(relief);
        relief = NULL;
    }
    resource_free(data);
    free(menu);
    free(menu_rgb);
}

/* over black: a white and a gold ramp, what the text's styles match to */
static void black_palette(uint8_t rgb[768])
{
    for (int i = 0; i < RAMP; i++) {
        const int v = i * 255 / (RAMP - 1);
        rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = (uint8_t)v;
        rgb[3 * (RAMP + i)] = (uint8_t)v, rgb[3 * (RAMP + i) + 1] = (uint8_t)(v * 206 / 255), rgb[3 * (RAMP + i) + 2] = (uint8_t)(v * 6 / 255);
    }
}

static void draw_item(const CreditsItem *it)
{
    static const struct { int num, den; } SIZE[] = {
        [CREDITS_BIG] = { 2, 1 }, [CREDITS_TITLE] = { 1, 1 }, [CREDITS_LINE] = { 5, 4 }, [CREDITS_NAME] = { 4, 5 }, [CREDITS_HEADER] = { 2, 3 }, [CREDITS_NOTE] = { 3, 5 } };
    const int num = SIZE[it->kind].num, den = SIZE[it->kind].den;
    if (it->kind == CREDITS_HEADER) {
        text_draw_header_column(it->text, it->cx - CREDITS_COLUMN_W / 2, CREDITS_COLUMN_W, it->y, num, den);
        return;
    }
    text_select(it->colour == CREDITS_YELLOW ? TEXT_YELLOW : TEXT_CREAM);
    text_draw_smooth(it->text, it->cx - text_width_scaled(it->text, num, den) / 2, it->y, num, den);
}

static int skipped(void)
{
    input_pump();
    return input_next_char() != 0 || pointer_read_buttons() != 0;
}

static void show_page(const CreditsItem *items, int n, void (*tick)(void))
{
    uint8_t rgb[768], black[768] = { 0 };
    if (relief != NULL)
        memcpy(rgb, relief_pal, sizeof rgb);
    else
        black_palette(rgb);
    ea_select_default();
    video_set_palette(0, PALETTE_COLOURS, black);
    video_clear_back();
    if (relief != NULL) {
        int w, h;
        uint8_t *const to = video_target(&w, &h);
        for (int y = 0; y < SCREEN_H && y < h; y++) memcpy(&to[y * w], &relief[y * SCREEN_W], (size_t)(w < SCREEN_W ? w : SCREEN_W));
    }
    text_build_styles_from(rgb);
    for (int i = 0; i < n; i++) draw_item(&items[i]);
    video_flip();
    video_fade_in(0, PALETTE_COLOURS, rgb);
    for (int frame = 0; frame < PAGE_FRAMES && !skipped(); frame++) {
        if (tick != NULL) tick();
        video_wait_vblank();
    }
    video_fade_out(0, PALETTE_COLOURS);
    ea_select_default();
    video_clear_back();
    video_flip();
}

void credits_pages_show(const char *version, void (*tick)(void))
{
    port_log("mark:screen=credits_ours\n");   /* port: a headless walk can wait for it */
    relief_compose();
    CreditsItem items[CREDITS_ITEMS_MAX];
    for (int page = 0; page < CREDITS_PAGES; page++) {
        const int n = credits_page(page, version, items);
        if (n > 0) show_page(items, n, tick);
    }
}

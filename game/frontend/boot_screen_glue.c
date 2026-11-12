/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: draws the boot screen (boot_screen.h). The picture is made in RGB: the red relief rebuilt from the player's CD (boot_relief.h; black when it
 * cannot be), the banner (data/logo/spirited_logo.png, RGBA, its soft shadow included) laid over it; then quantised once to the palette's first entries
 * (boot_quant.h), the last ones kept for the text's colours, and the words drawn in the menus' font. A fade in, BOOT_HOLD_TICKS on the timer, a fade
 * out; any key, click or pad button ends it early (the game's first screen: a press counts from its first look, screen_hold.h), and then it waits for
 * that press to be let go. */
#include "game/frontend/boot_screen.h"

#include "formats/png.h"
#include "game/frontend/boot_quant.h"
#include "game/frontend/boot_relief.h"
#include "game/frontend/ea_lib.h"
#include "game/sys/art_cache.h"
#include "game/sys/control_map.h"
#include "game/sys/data_path.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "platform/platform.h"
#include "tnfs_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SCREEN_W = 640, SCREEN_H = 480, PALETTE_COLOURS = 256, TEXT_COLOURS = 18, PRESENTS_GAP = 8, SCANCODES = 0x80 };

/* the text's colours and their antialiasing ramps toward the red, kept out of the quantiser's reach (the approved mockup's) */
static const uint8_t TEXT_RGB[TEXT_COLOURS][3] = {
    { 255, 255, 255 }, { 232, 211, 183 }, { 255, 206, 6 }, { 255, 6, 6 },     { 165, 133, 97 }, { 0, 0, 0 },     { 140, 130, 115 },
    { 200, 200, 200 }, { 150, 150, 150 }, { 100, 100, 100 }, { 60, 60, 60 },  { 30, 30, 30 },   { 240, 170, 20 }, { 200, 120, 20 },
    { 180, 20, 20 },   { 120, 10, 10 },   { 220, 190, 160 }, { 190, 160, 130 } };

static const char LOGO_FILE[] = "logo/spirited_logo.png";

static uint8_t *read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    long n = 0;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n > 0 && fseek(f, 0, SEEK_SET) == 0) data = malloc((size_t)n);
    if (data != NULL && fread(data, 1, (size_t)n, f) == (size_t)n) *size = (size_t)n;
    else free(data), data = NULL;
    fclose(f);
    return data;
}

/* the banner over rgb at its place, its alpha blended; 0 when there is none of the right size in the port's data */
static int lay_banner(uint8_t *rgb)
{
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(LOGO_FILE, found);
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) {
        if (found[i][0] == '\0' || !data_path_exists(found[i])) continue;
        size_t size = 0;
        uint8_t *file = read_file(found[i], &size);
        PngImage img;
        const int read = file != NULL && png_read(file, size, &img) == PNG_OK;
        free(file);
        if (!read) continue;
        const int fits = !img.indexed && img.width == BOOT_BANNER_W && img.height == BOOT_BANNER_H;
        for (int y = 0; fits && y < BOOT_BANNER_H; y++)
            for (int x = 0; x < BOOT_BANNER_W; x++) {
                const uint8_t *s = img.pixels + 4 * (y * BOOT_BANNER_W + x);
                uint8_t *d = rgb + 3 * ((BOOT_BANNER_Y + y) * SCREEN_W + BOOT_BANNER_X + x);
                for (int c = 0; c < 3; c++) d[c] = (uint8_t)((s[c] * s[3] + d[c] * (255 - s[3]) + 127) / 255);
            }
        png_free(img.pixels);
        if (fits) return 1;
        port_log("boot: %s is not a %dx%d RGBA picture\n", found[i], BOOT_BANNER_W, BOOT_BANNER_H);
    }
    port_log("boot: no %s in the port's data, the name is drawn in the font\n", LOGO_FILE);
    return 0;
}

/* the picture: pixels and palette (the text's colours last); MADE_BANNER when the banner is in it, MADE_RELIEF when the relief is */
enum { MADE_BANNER = 1, MADE_RELIEF = 2 };
static int make(uint8_t *pixels, uint8_t palette[768])
{
    uint8_t *rgb = calloc((size_t)SCREEN_W * SCREEN_H, 3);
    int banner = 0;
    memset(palette, 0, 768);
    if (rgb == NULL) {
        memset(pixels, 0, (size_t)SCREEN_W * SCREEN_H);
        return 0;
    }
    const int relief = boot_relief_load(rgb);
    if (!relief) port_log("boot: the red relief cannot be rebuilt from the CD, over black\n");
    banner = lay_banner(rgb);
    const int quantised = boot_quant(rgb, SCREEN_W * SCREEN_H, PALETTE_COLOURS - TEXT_COLOURS, palette, pixels) != 0;
    if (!quantised) memset(pixels, 0, (size_t)SCREEN_W * SCREEN_H);
    memcpy(palette + 3 * (PALETTE_COLOURS - TEXT_COLOURS), TEXT_RGB, sizeof TEXT_RGB);
    free(rgb);
    return (banner ? MADE_BANNER : 0) | (relief && quantised ? MADE_RELIEF : 0);
}

/* the picture's key in the player's cache (art_cache.h): the relief's sources and the banner's file */
enum { BOOT_SCREEN_ART_VERSION = 1 };   /* raise it when boot_relief.c, boot_quant.c or the banner's place change what is made */
static void picture_key(ArtCacheKey *key)
{
    art_cache_key_init(key, "boot_screen", BOOT_SCREEN_ART_VERSION);
    boot_relief_key(key);
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(LOGO_FILE, found);
    uint8_t *logo = NULL;
    size_t size = 0;
    for (int i = 0; i < DATA_PATH_CANDIDATES && logo == NULL; i++)
        if (found[i][0] != '\0' && data_path_exists(found[i])) logo = read_file(found[i], &size);
    art_cache_key_add(key, logo, size);
    free(logo);
}

/* the picture from the cache when it was made before from the same files, else made and kept there; 1 when the banner is in it */
static int compose(uint8_t *pixels, uint8_t palette[768])
{
    enum { PIXELS = SCREEN_W * SCREEN_H, BLOB = PIXELS + 768 + 1 };
    ArtCacheKey key;
    picture_key(&key);
    uint8_t *blob = malloc(BLOB);
    if (blob != NULL && art_cache_read("boot_screen.bin", &key, blob, BLOB)) {
        memcpy(pixels, blob, PIXELS), memcpy(palette, blob + PIXELS, 768);
        const int banner = blob[BLOB - 1];
        free(blob);
        return banner;
    }
    const int made = make(pixels, palette), banner = (made & MADE_BANNER) != 0;
    if (blob != NULL && (made & MADE_RELIEF)) {   /* a black stand-in is not kept */
        memcpy(blob, pixels, PIXELS), memcpy(blob + PIXELS, palette, 768), blob[BLOB - 1] = (uint8_t)banner;
        (void)art_cache_write("boot_screen.bin", &key, blob, BLOB);
    }
    free(blob);
    return banner;
}

static void centred(const char *text, TextStyle style, int y, int num, int den)
{
    text_select(style);
    text_draw_smooth(text, (SCREEN_W - text_width_scaled(text, num, den)) / 2, y, num, den);
}

/* graver18 letter by letter, PRESENTS_GAP more between letters */
static void spread(const char *text, int y, int num, int den)
{
    char c[2] = { 0, 0 };
    int w = 0;
    for (const char *p = text; *p; p++) c[0] = *p, w += text_width_scaled(c, num, den) + (p[1] ? PRESENTS_GAP : 0);
    int x = (SCREEN_W - w) / 2;
    text_select(TEXT_WHITE);
    for (const char *p = text; *p; p++) {
        c[0] = *p;
        if (*p != ' ') text_draw_smooth(c, x, y, num, den);
        x += text_width_scaled(c, num, den) + PRESENTS_GAP;
    }
}

static void draw(const BootLine *lines, int n, int banner)
{
    for (int i = 0; i < n; i++) {
        const BootLine *l = &lines[i];
        switch (l->style) {
        case BOOT_HOUSE: centred(l->text, TEXT_RED, l->y, 2, 1); break;
        case BOOT_PRESENTS: spread(l->text, l->y, 4, 5); break;
        case BOOT_SUBTITLE: centred(l->text, TEXT_WHITE, l->y, 4, 5); break;
        case BOOT_SMALL: centred(l->text, TEXT_WHITE, l->y, 3, 5); break;
        case BOOT_NAME:
            if (!banner) centred(l->text, TEXT_YELLOW, l->y - text_line_height() * 3 / 2, 3, 1);
            break;
        }
    }
}

/* any key held now: 00464bd0's (it pumps the events) or any other, by its set-1 scancode (the extended ones by their BIOS numbers, platform.h) */
static int any_key(void)
{
    if (input_get_key() != 0) return 1;
    for (int sc = 1; sc < SCANCODES; sc++)
        if (platform_key_down(sc)) return 1;
    return 0;
}

/* any button of any pad or wheel held now */
static int any_pad_button(void)
{
    const int pads = platform_joy_count();
    for (int d = 0; d < pads; d++) {
        RawPad raw;
        if (!platform_joy_raw(d, &raw)) continue;
        for (int b = 0; b < raw.buttons && b < RAW_MAX_BUTTONS; b++)
            if (raw.button[b]) return 1;
    }
    return 0;
}

/* held BOOT_HOLD_TICKS or until a press; the ticks it stayed. The game's first screen: nothing is held from a screen before, so a press counts from
 * the first look, a key tapped while the picture was made or faded in too (latched until that look) */
static uint32_t hold(void)
{
    const uint32_t start = timer_ticks();
    ScreenHold h = screen_hold_start_armed(start, BOOT_HOLD_TICKS);
    for (;;) {
        HoldSource src[HOLD_SOURCES];
        const int key = any_key();   /* first: it pumps the events the pointer's buttons are read from */
        boot_screen_sources(key, pointer_read_buttons(), any_pad_button(), src);
        const uint32_t now = timer_ticks();
        if (screen_hold_step(&h, now, src) != HOLD_STAY) return now - start;
        video_wait_vblank();
    }
}

/* after a press ended it: until it is let go (at most BOOT_RELEASE_TICKS), so the same press does not also stop EA's first movie; the ticks waited */
static uint32_t let_go(void)
{
    const uint32_t start = timer_ticks();
    for (;;) {
        HoldSource src[HOLD_SOURCES];
        const int key = any_key();
        boot_screen_sources(key, pointer_read_buttons(), any_pad_button(), src);
        const uint32_t now = timer_ticks();
        if (screen_hold_all_up(src) || now - start >= BOOT_RELEASE_TICKS) return now - start;
        video_wait_vblank();
    }
}

void boot_screen_show(void)
{
    static int shown;
    if (shown) return;
    shown = 1;
    BootLine lines[BOOT_LINES_MAX];
    const int n = boot_screen_lines(TNFS_VERSION, lines);
    uint8_t *picture = malloc((size_t)SCREEN_W * SCREEN_H), palette[768], black[768] = { 0 };
    if (picture == NULL || !text_small_init()) {
        free(picture);
        return;
    }
    port_log("mark:screen=boot\n");   /* port: a headless walk can wait for it */
    const int banner = compose(picture, palette);
    ea_select_default();
    video_set_palette(0, PALETTE_COLOURS, black);
    int w, h;
    uint8_t *to = video_target(&w, &h);
    for (int y = 0; y < SCREEN_H && y < h; y++) memcpy(&to[y * w], &picture[y * SCREEN_W], (size_t)(w < SCREEN_W ? w : SCREEN_W));
    free(picture);
    text_build_styles_from(palette);
    draw(lines, n, banner);
    video_flip();
    video_fade_in(0, PALETTE_COLOURS, palette);
    port_log("mark:hold screen=boot ticks=%u\n", (unsigned)hold());   /* port: what a headless walk measures it by */
    video_fade_out(0, PALETTE_COLOURS);
    ea_select_default();
    video_clear_back();
    video_flip();
    port_log("mark:let_go screen=boot ticks=%u\n", (unsigned)let_go());
}

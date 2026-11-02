/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: draws the boot screen (boot_screen.h): black, the words centred in the menus' fonts, a fade in, about 3 seconds, a fade out. */
#include "game/frontend/boot_screen.h"

#include "game/frontend/ea_lib.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/text.h"
#include "game/sys/video.h"
#include "tnfs_version.h"

#include <string.h>

enum { SCREEN_W = 640, SCREEN_H = 480, HOLD_FRAMES = 60, PALETTE_COLOURS = 256, RAMP = 128 };   /* video runs at 25 frames a second: the fades take 17 each, so about 4 s in all */

/* black, then two ramps: white (0..127) and gold (128..255), what TEXT_WHITE and TEXT_YELLOW match to */
static void palette_make(uint8_t rgb[PALETTE_COLOURS * 3])
{
    for (int i = 0; i < RAMP; i++) {
        const int v = i * 255 / (RAMP - 1);
        rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = (uint8_t)v;
        rgb[3 * (RAMP + i)] = (uint8_t)v, rgb[3 * (RAMP + i) + 1] = (uint8_t)(v * 206 / 255), rgb[3 * (RAMP + i) + 2] = (uint8_t)(v * 6 / 255);
    }
}

static int width_of(const BootLine *line)
{
    return line->size == BOOT_SMALL ? text_small_width(line->text) : line->size == BOOT_HEADLINE ? text_width_scaled(line->text, 1, 1) : text_width_scaled(line->text, 2, 1);
}

static int height_of(const BootLine *line)
{
    return line->size == BOOT_SMALL ? text_small_line_height() : line->size == BOOT_HEADLINE ? text_line_height() : text_line_height() * 2;
}

static void draw(const BootLine *lines, int n)
{
    int total = 0;
    for (int i = 0; i < n; i++) total += lines[i].gap + height_of(&lines[i]);
    int y = (SCREEN_H - total) / 2;
    for (int i = 0; i < n; i++) {
        y += lines[i].gap;
        const int x = (SCREEN_W - width_of(&lines[i])) / 2;
        if (lines[i].size == BOOT_SMALL) {
            text_select(TEXT_WHITE);
            text_ramp(0);
            text_small_draw(lines[i].text, x, y);
        } else {
            text_select(lines[i].size == BOOT_HEADLINE ? TEXT_YELLOW : TEXT_WHITE);
            text_draw_smooth(lines[i].text, x, y, lines[i].size == BOOT_TITLE ? 2 : 1, 1);
        }
        y += height_of(&lines[i]);
    }
}

static int skipped(void)
{
    input_pump();
    return input_next_char() != 0 || pointer_read_buttons() != 0;
}

void boot_screen_show(void)
{
    static int shown;
    if (shown) return;
    shown = 1;
    BootLine lines[BOOT_LINES_MAX];
    const int n = boot_screen_lines(TNFS_VERSION, lines);
    uint8_t rgb[PALETTE_COLOURS * 3], black[PALETTE_COLOURS * 3] = { 0 };
    palette_make(rgb);
    if (!text_small_init()) return;
    ea_select_default();
    video_set_palette(0, PALETTE_COLOURS, black);
    video_clear_back();
    text_build_styles_from(rgb);
    draw(lines, n);
    video_flip();
    video_fade_in(0, PALETTE_COLOURS, rgb);
    for (int frame = 0; frame < HOLD_FRAMES && !skipped(); frame++) video_wait_vblank();
    video_fade_out(0, PALETTE_COLOURS);
    ea_select_default();
    video_clear_back();
    video_flip();
}

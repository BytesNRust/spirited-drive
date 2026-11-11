/* port (light_edit): see light_edit.h. */
#include "viewer/light_edit.h"

#include "viewer/drive_light.h"
#include "game/viewer/view_pick.h"
#include "game/platform/platform.h"
#include "game/sys/port_settings.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MARGIN_X = 4, MARGIN_Y = 26, PAD = 4, MAX_WIDTH = 3360, MAX_HEIGHT = 1440 };

static uint8_t scratch[MAX_WIDTH * MAX_HEIGHT];

int light_edit_wanted(void) { return port_light_edit(); }   /* port.cfg's light_edit, or TNFS_LIGHT_EDIT */

void light_edit_start(LightEdit *e, int on)
{
    *e = (LightEdit){ .on = on != 0 };
    view_pick_set(e->on);   /* the aim: the texture at the cross, for the lamps' marks */
}

/* the aim's line: the mask file that names the texture at the cross (slot_NN.png / scenery_NN.png in track_masks/<TRACK>/) */
static void aim_line(char line[96])
{
    int slot = 0, variant = 0;
    switch (view_pick_found(&slot, &variant)) {
    case VIEW_PICK_SLOT: snprintf(line, 96, "aim (+): road texture slot_%02d (variant %d)", slot, variant); break;
    case VIEW_PICK_SCENERY: snprintf(line, 96, "aim (+): scenery_%02d", slot); break;
    case VIEW_PICK_OTHER: snprintf(line, 96, "aim (+): a car or an object, no track texture"); break;
    default: snprintf(line, 96, "aim (+): nothing of the track"); break;
    }
}

/* the aim's cross (at the pointer) over the frame (`width` x `height`, the 3D's target scaled to it) in entry `ink` */
static void draw_cross(uint8_t *pixels, int width, int height, uint8_t ink)
{
    enum { ARM = 8, GAP = 3 };
    int x = 0, y = 0, tw = 0, th = 0;
    if (!view_pick_aim_on(&x, &y, &tw, &th) || tw <= 0 || th <= 0) return;
    const int cx = (int)((long long)x * width / tw), cy = (int)((long long)y * height / th);
    for (int d = GAP; d <= ARM; d++)
        for (int s = -1; s <= 1; s += 2) {
            const int px = cx + s * d, py = cy + s * d;
            if (px >= 0 && px < width && cy >= 0 && cy < height) pixels[cy * width + px] = ink;
            if (py >= 0 && py < height && cx >= 0 && cx < width) pixels[py * width + cx] = ink;
        }
}

static void say(LightEdit *e, const char *text, uint32_t now_ms)
{
    snprintf(e->message, sizeof e->message, "%s", text);
    e->message_ms = now_ms;
}

void light_edit_keys(LightEdit *e, const uint8_t *keys, uint32_t node, uint32_t now_ms)
{
    if (!e->on) return;
    const int freeze = keys[LIGHT_EDIT_KEY_FREEZE] != 0, copy = keys[LIGHT_EDIT_KEY_COPY] != 0;
    if (freeze && !e->freeze_down) {
        e->frozen = !e->frozen;
        say(e, e->frozen ? "held at this node (F10 lets go)" : "going on", now_ms);
    }
    if (copy && !e->copy_down) {
        char line[64];
        snprintf(line, sizeof line, "brightness %u %u 100\n", node, node);
        say(e, platform_set_clipboard_text(line) ? "copied: brightness (node)" : "no clipboard here", now_ms);
    }
    e->freeze_down = freeze, e->copy_down = copy;
}

int light_edit_poll(LightEdit *e, uint32_t now_ms)
{
    if (!e->on || now_ms - e->checked_ms < LIGHT_EDIT_CHECK_MS) return 0;
    e->checked_ms = now_ms;
    uint32_t hash = 0;
    char path[1024];
    if (!drive_light_file_state(path, sizeof path, &hash)) return 0;
    if (!e->hash_known) {
        e->hash = hash, e->hash_known = 1;
        return 0;
    }
    if (hash == e->hash) return 0;
    e->hash = hash;
    if (!drive_light_reload()) return 0;
    say(e, "a file changed: the light is made again (snow: restart)", now_ms);
    return 1;
}

const uint8_t *light_edit_compose(LightEdit *e, const uint8_t *frame, int width, int height, const uint8_t palette[768], uint32_t node)
{
    if (!e->on || width <= 0 || width > MAX_WIDTH || height <= 0 || height > MAX_HEIGHT) return frame;
    int mouse_x = 0, mouse_y = 0;
    unsigned buttons = 0;
    platform_mouse(&mouse_x, &mouse_y, &buttons);
    view_pick_pointer(mouse_x, mouse_y, width, height);   /* the aim: the polygon under the pointer, from the next frame on */
    char lines[LIGHT_EDIT_LINES][96];
    int n = drive_light_edit_lines(node, lines, LIGHT_EDIT_LINES - 2);   /* room for the aim and a message */
    if (n <= 0) return frame;
    if (n < LIGHT_EDIT_LINES) aim_line(lines[n++]);
    if (n < LIGHT_EDIT_LINES && e->message[0] != '\0' && platform_ticks_ms() - e->message_ms < LIGHT_EDIT_MESSAGE_MS) snprintf(lines[n++], 96, "%s", e->message);
    if (!e->text_ready) e->text_ready = text_small_init();
    if (!e->text_ready) return frame;
    if (!e->palette_known || memcmp(e->palette, palette, 768) != 0) {
        memcpy(e->palette, palette, 768);
        text_build_styles_from(palette);
        int best = 0, best_sum = 1 << 30, light = 0, light_sum = -1;
        for (int i = 0; i < 256; i++) {
            const int sum = palette[3 * i] + palette[3 * i + 1] + palette[3 * i + 2];
            if (sum < best_sum) best = i, best_sum = sum;
            if (sum > light_sum) light = i, light_sum = sum;
        }
        e->dark = (uint8_t)best, e->light = (uint8_t)light;
        e->palette_known = 1;
    }
    memcpy(scratch, frame, (size_t)width * (size_t)height);
    draw_cross(scratch, width, height, e->light);
    const int line_h = text_small_line_height();
    int widest = 0;
    for (int i = 0; i < n; i++) {
        const int w = text_small_width(lines[i]);
        if (w > widest) widest = w;
    }
    for (int y = MARGIN_Y; y < MARGIN_Y + n * line_h + 2 * PAD && y < height; y++)
        for (int x = MARGIN_X; x < MARGIN_X + widest + 2 * PAD && x < width; x++) scratch[y * width + x] = e->dark;
    video_select_target(scratch, width, height);
    text_select(TEXT_CREAM);
    text_ramp(0);
    for (int i = 0; i < n; i++) text_small_draw(lines[i], MARGIN_X + PAD, MARGIN_Y + PAD + i * line_h);
    video_select_target(NULL, 0, 0);
    return scratch;
}

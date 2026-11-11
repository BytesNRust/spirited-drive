/* port (wipers): the windscreen's glass and blades of the dash's art. Split from drive_wide.c. */
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "game/gfx/blit.h"
#include "game/sys/backdrop.h"
#include "viewer/layer_compose.h"
#include "viewer/class_cache.h"
#include "viewer/drive_light.h"
#include "viewer/dash_box.h"
#include "game/sys/bands.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/race/car_draw.h"
#include "game/race/dash_color.h"
#include "game/race/dash_mask.h"
#include "game/race/wiper.h"
#include "viewer/wiper_scene.h"
#include "viewer/ported_view.h"
#include "game/race/tach_dim.h"
#include "game/race/wheel_front.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "viewer/drive_wide_private.h"

/* port (wipers): the windscreen's glass of the dash's art (wiper_scene.h), flags (1 = glass) cached per dash art */
static const uint8_t *wiper_glass_flags(const uint8_t *art, const uint8_t *mask)
{
    static uint8_t flags[SCREEN_W * SCREEN_H];
    static int stack[SCREEN_W * SCREEN_H];
    static const uint8_t *made_for;
    if (made_for == drive_hud.cockpit.dash) return flags;
    wiper_scene_glass_flags(art, mask, flags, stack);
    made_for = drive_hud.cockpit.dash;
    return flags;
}

int drive_wide_wiper_scene(const RaceState *race, WiperBlade *out)
{
    ported_view_set_wiper_glass(NULL, 0.0f, 0.0f, 1.0f);
    const RaceCamera *camera = race != NULL ? race->chase.camera : NULL;
    if (!port_wipers() || camera == NULL || camera->layout != 0 || drive_hud.police.screen != NULL || drive_hud.cockpit.dash == NULL) return 0;
    const uint8_t *const art = dash_art_layer(&drive_hud.cockpit), *const mask = dash_mask_now();
    const float PI = 3.14159265f, deg = PI / 180.0f;
    const uint8_t *const flags = wiper_glass_flags(art, mask);
    static const void *measured_for;   /* the geometry on the layer is the dash's alone: measured once per dash */
    static WiperBlade layer[WIPER_BLADES];
    static float span;
    if (measured_for != drive_hud.cockpit.dash) {
        wiper_scene_measure(art, mask, flags, layer, &span);
        measured_for = drive_hud.cockpit.dash;
    }
    const int wide = drive_wide_active();   /* the 640x480 path (the exe's) has its dash at the layer's own coordinates */
    const float scale = wide ? (float)drive_wide.scale : 1.0f, x0 = wide ? (float)((drive_wide.width - SCREEN_W) / 2) : 0.0f, y0 = wide && drive_wide.width > SCREEN_W ? (float)DASH_DROP : 0.0f;
    for (int i = 0; i < WIPER_BLADES; i++) {
        WiperBlade b = layer[i];
        b.top = b.park + span * deg;
        b.x = (x0 + b.x) * scale, b.y = (y0 + b.y) * scale, b.len *= scale, b.xmax = (x0 + b.xmax) * scale;
        out[i] = b;
    }
    ported_view_set_wiper_glass(flags, x0 * scale, y0 * scale, scale);
    return WIPER_BLADES;
}

/* port (wipers): `frame` with the pixels the dash's mask marks as a wiper blade (dash_mask.h BLADE) made nothing, as the windscreen is; `frame` itself when the dash has none */
const uint8_t *blades_hidden(const uint8_t *frame)
{
    static uint8_t copy[SCREEN_W * SCREEN_H];
    static const uint8_t *listed_for;   /* the mask's blade pixels, listed once per mask */
    static int *blade_at;
    static size_t blade_count;
    const uint8_t *const mask = dash_mask_now();
    if (mask == NULL) return frame;
    if (mask != listed_for) {
        free(blade_at);
        blade_at = malloc(sizeof(int) * SCREEN_W * SCREEN_H);
        blade_count = 0;
        if (blade_at != NULL)
            for (size_t i = 0; i < sizeof copy; i++)
                if (mask[i] == DASH_MASK_BLADE) blade_at[blade_count++] = (int)i;
        listed_for = mask;
    }
    if (blade_count == 0) return frame;
    memcpy(copy, frame, sizeof copy);
    for (size_t k = 0; k < blade_count; k++) copy[blade_at[k]] = LAYER_KEY;
    return copy;
}

/* port (wipers): on the 640x480 path the 3D (`view`) reaches the screen through the windscreen's run lists only, so the art's own blades stay: where the dash's mask marks a blade, the 3D's pixel goes on the screen
 * (the sweeping blades are in it). The frame's mirror, gauges and the rest are drawn after, as ever. */
void drive_wide_wipers_reveal(uint8_t *screen, const uint8_t *view)
{
    const uint8_t *const mask = dash_mask_now();
    if (mask == NULL || drive_wide_active() || !ported_view_wipers_active()) return;
    for (size_t i = 0; i < (size_t)SCREEN_W * SCREEN_H; i++)
        if (mask[i] == DASH_MASK_BLADE) screen[i] = view[i];
}

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
#include "game/race/dash_nose.h"
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

/* port (wipers): the steering wheel's middle on the 640 dash (wiper_scene_wheel_x), cached per dash art; -1 with no wheel */
static float wiper_wheel_x(const uint8_t *art)
{
    static uint8_t zone[SCREEN_W * SCREEN_H];
    static const uint8_t *made_for;
    static float wheel_x = -1.0f;
    if (made_for == drive_hud.cockpit.dash) return wheel_x;
    wheel_front_zone(&drive_hud.cockpit, art, zone);
    wheel_x = wiper_scene_wheel_x(zone);
    made_for = drive_hud.cockpit.dash;
    return wheel_x;
}

/* port (wipers, dep49): the nose under the blades of the dash on screen (dash_nose.h), 640x480 (LAYER_KEY: none), made again when the dash, its mask, the palette (the body
 * colour), the settings (a change of mode or of a row, port_follow.h) or the wipers' switch change; NULL when there is none. `*builds` (may be NULL) moves on with each new one. */
static const uint8_t *wiper_nose_now(unsigned *builds)
{
    static DashNoseCache cache;
    const uint8_t *const mask = dash_mask_now();
    const uint8_t *nose = NULL;
    if (drive_hud.cockpit.dash != NULL && mask != NULL && drive_hud.rgb != NULL) {
        const DashNoseKey key = dash_nose_key(drive_hud.cockpit.dash, mask, drive_hud.rgb);
        nose = dash_nose_cached(&cache, &key, dash_art_layer(&drive_hud.cockpit), mask, SCREEN_W, SCREEN_H, LAYER_KEY);
    } else if (cache.made) dash_nose_cache_free(&cache);
    if (builds != NULL) *builds = cache.builds;
    return nose;
}

/* port (wipers, dep49): the nose as the frame shows it (wiper_scene_frame_glass's grid), handed to the 3D's wiper pass with its extent; made once per nose and width */
static void wiper_nose_frame(int width)
{
    static uint8_t *map;
    static unsigned made_for;
    static int made_width, box[4];
    unsigned builds = 0;
    const uint8_t *const nose = wiper_nose_now(&builds);
    if (nose == NULL) return;
    if (map == NULL || made_for != builds || made_width != width) {
        uint8_t *const grown = realloc(map, (size_t)width * SCREEN_H);
        if (grown == NULL) return;
        map = grown;
        wiper_scene_frame_glass(nose, width, map);   /* a byte a pixel, as the glass's flags: the colours go across as they are */
        box[0] = width, box[1] = SCREEN_H, box[2] = 0, box[3] = 0;
        for (int y = 0; y < SCREEN_H; y++)
            for (int x = 0; x < width; x++)
                if (map[(size_t)y * (size_t)width + (size_t)x] != LAYER_KEY) {
                    if (x < box[0]) box[0] = x;
                    if (y < box[1]) box[1] = y;
                    if (x + 1 > box[2]) box[2] = x + 1;
                    if (y + 1 > box[3]) box[3] = y + 1;
                }
        made_for = builds, made_width = width;
    }
    ported_view_set_wiper_nose(map, box);
}

int drive_wide_wiper_scene(const RaceState *race, WiperBlade *out)
{
    ported_view_set_wiper_glass(NULL, 0, 0.0f, 0.0f, 1.0f);
    ported_view_set_wiper_nose(NULL, NULL);
    const RaceCamera *camera = race != NULL ? race->chase.camera : NULL;
    if (!port_wipers() || camera == NULL || camera->layout != 0 || drive_hud.police.screen != NULL || drive_hud.cockpit.dash == NULL) return 0;
    const uint8_t *const art = dash_art_layer(&drive_hud.cockpit), *const mask = dash_mask_now();
    const float PI = 3.14159265f, deg = PI / 180.0f;
    const uint8_t *const flags = wiper_glass_flags(art, mask);
    const int wide = drive_wide_active();   /* the 640x480 path (the exe's) has its dash at the layer's own coordinates */
    const int width = wide && drive_wide.width > SCREEN_W ? (drive_wide.width < SCREEN_DASH_MOST_WIDTH ? drive_wide.width : SCREEN_DASH_MOST_WIDTH) : SCREEN_W;
    static uint8_t *map;   /* the glass as the frame shows it (wiper_scene_frame_glass), and the pair on it: made once per dash and width */
    static const void *measured_for;
    static int measured_width;
    static WiperBlade frame[WIPER_BLADES];
    static float span;
    if (measured_for != drive_hud.cockpit.dash || measured_width != width) {
        uint8_t *const grown = realloc(map, (size_t)width * SCREEN_H);
        if (grown == NULL) return 0;
        map = grown;
        wiper_scene_frame_glass(flags, width, map);
        const float wheel = wiper_wheel_x(art);   /* the pivots either side of the wheel (wiper_scene.h) */
        wiper_scene_measure(map, width, wheel >= 0.0f ? wiper_scene_frame_x(width, wheel) : -1.0f, frame, &span);
        measured_for = drive_hud.cockpit.dash, measured_width = width;
    }
    const float scale = wide ? (float)drive_wide.scale : 1.0f;
    for (int i = 0; i < WIPER_BLADES; i++) {
        WiperBlade b = frame[i];
        b.top = b.park + span * deg;
        b.x *= scale, b.y *= scale, b.len *= scale, b.xmax *= scale;
        out[i] = b;
    }
    ported_view_set_wiper_glass(map, width, 0.0f, 0.0f, scale);
    wiper_nose_frame(width);   /* port (wipers, dep49): the nose under the blades, on the same grid */
    return WIPER_BLADES;
}

/* port (wipers, dep49): the nose's pixels (wiper_nose_now) listed once per nose; 0 with none */
static size_t nose_listed(const int **at)
{
    static int *list;
    static size_t count;
    static unsigned listed_for;
    unsigned builds = 0;
    const uint8_t *const nose = wiper_nose_now(&builds);
    if (nose == NULL) return 0;
    if (list == NULL || listed_for != builds) {
        free(list);
        list = malloc(sizeof(int) * SCREEN_W * SCREEN_H);
        count = 0;
        if (list != NULL)
            for (size_t i = 0; i < (size_t)SCREEN_W * SCREEN_H; i++)
                if (nose[i] != LAYER_KEY) list[count++] = (int)i;
        listed_for = builds;
    }
    *at = list;
    return count;
}

/* port (wipers): `frame` with the pixels the dash's mask marks as a wiper blade (dash_mask.h BLADE) made nothing, as the windscreen is, and (dep49) the nose's pixels where the
 * frame shows the dash's art (the wheel and the rest drawn over it stay): the nose is the 3D's then, under the blades (wiper_draw); `frame` itself when the dash has neither */
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
    const int *nose_at = NULL;
    const size_t nose_count = nose_listed(&nose_at);
    if (blade_count == 0 && nose_count == 0) return frame;
    memcpy(copy, frame, sizeof copy);
    for (size_t k = 0; k < blade_count; k++) copy[blade_at[k]] = LAYER_KEY;
    const uint8_t *const art = dash_art_layer(&drive_hud.cockpit);
    for (size_t k = 0; k < nose_count; k++)
        if (frame[nose_at[k]] == art[nose_at[k]]) copy[nose_at[k]] = LAYER_KEY;
    return copy;
}

/* port (wipers, dep49): `shown` (blades_hidden's) with the nose's colours back where it made them nothing, for the light's classes alone: the nose keeps the dash's light (it is
 * the dash's art, the half tint) though the 3D's buffer draws it; `shown` itself with no nose */
const uint8_t *nose_classed(const uint8_t *shown)
{
    static uint8_t copy[SCREEN_W * SCREEN_H];
    const int *nose_at = NULL;
    const size_t nose_count = nose_listed(&nose_at);
    const uint8_t *const nose = wiper_nose_now(NULL);
    if (nose_count == 0 || nose == NULL) return shown;
    memcpy(copy, shown, sizeof copy);
    for (size_t k = 0; k < nose_count; k++)
        if (copy[nose_at[k]] == LAYER_KEY) copy[nose_at[k]] = nose[nose_at[k]];
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
    const int *nose_at = NULL;   /* dep49: the nose too, painted in the 3D under the blades (the wheel and the gauges are drawn after) */
    const size_t nose_count = nose_listed(&nose_at);
    for (size_t k = 0; k < nose_count; k++) screen[nose_at[k]] = view[nose_at[k]];
}

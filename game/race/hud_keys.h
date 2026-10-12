/* The race screen's keys (00420710, from 00422598 after 00421c18): the top
 * row's layout (F8), the labels' redraw (F7), the parts mode (the parts key
 * or a pad button) and the in-car camera (F9). A key acts once and waits to
 * be let go: an edge flag (004b9460) is set only while no HUD key is held. */
#ifndef TNFS_HUD_KEYS_H
#define TNFS_HUD_KEYS_H

#include "game/race/hud.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    HUD_KEY_REDRAW = 0x41,  /* 004c697d: F7 */
    HUD_KEY_LAYOUT = 0x42,  /* 004c697e: F8 */
    HUD_KEY_CAMERA = 0x43   /* 004c697f: F9 */
};

typedef struct {
    void *ctx;
    void (*labels)(void *ctx);              /* 00420370 */
    void (*parts_redraw)(void *ctx);        /* 00420918 */
    void (*take)(void *ctx, int32_t kind);  /* 00405b00 on the follow record 004f801c */
} HudKeysOps;

typedef struct {
    const uint8_t *keys;    /* 004c693c: a flag per set-1 key code, up to parts_key */
    int32_t parts_key;      /* 004fb700 */
    int32_t pad;            /* 004fcdcc */
    int32_t pad_button[2];  /* 004fb728 / 004fb72c + 12 x 004fbb08: 3 for one is not the parts button */
    uint8_t pad_flags;      /* 004fce68: bit 1 the pad's parts button */
    uint8_t screen_kind;    /* 004fb789 */
} HudKeysInputs;

typedef struct {
    int32_t armed;          /* 004b9460: no HUD key held since the last one acted */
    int32_t use_parts;      /* 004f9b3c */
    int32_t dash_camera;    /* 004f9b7c */
    int32_t view_changed;   /* 00511fec: 999 after F9 */
    uint8_t clear_view;     /* 004fb78a */
} HudKeysState;

/* 00420710. Ten ticks after the start lights were last drawn (lights_until
 * below race->tick) the labels and waiting 0. Then the first key held of
 * F8, F7, parts: when armed it acts and disarms; F8 cycles the layout
 * (mode + 1) % 3 on track 3 in race type 0, else 1 - mode, then redraw on
 * and the labels; F7 flips redraw, then the labels; parts flips use_parts
 * and on its way off sets hud 004f9a24, redraws the parts when the chase
 * kind or dash_camera is set and asks for clear_view on screen kind 3.
 * Else F9, outside a replay (race->replay, unknown_513e6c) and in an in-car
 * kind (0, 1): when armed, dash_camera flips into kind 0's camera, the
 * chase takes its kind again, view_changed 999 and disarmed; with none of
 * them held: armed. */
void hud_keys(const HudKeysOps *ops, HudState *hud, HudKeysState *st, RaceState *race, const HudKeysInputs *in);

#endif

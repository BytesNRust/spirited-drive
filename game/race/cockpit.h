/* The in-car dashboard (the DASH / GDASH files, 0041f9a0 loads them): its
 * bank 004f9740 and 6-bit palette 004f973c. */
#ifndef TNFS_COCKPIT_H
#define TNFS_COCKPIT_H

#include "game/race/hud.h"
#include "game/race/hud_mask.h"
#include "game/race/race_palette.h"
#include "game/race/race_state.h"

#include <stddef.h>
#include <stdint.h>

enum {
    COCKPIT_COLOUR_FIRST = 0, COCKPIT_COLOUR_COUNT = 0x30,
    COCKPIT_FRAMES = 7,         /* the steering frames, 7 in low resolution, 6 in high */
    COCKPIT_DIGITS = 11,        /* 'num0'..'num9', 'numb' */
    COCKPIT_LIGHTS = 4,         /* 'nob1', 'lth3', 'lth2', 'lth1' */
    COCKPIT_DASH_POINTS = 10, COCKPIT_GAUGE_POINTS = 30, COCKPIT_GATE_POINTS = 9,
    COCKPIT_FMM_WORDS = 4,
    COCKPIT_NAME = 0x64,        /* 004f9ad8..004f9b3b */
    COCKPIT_PATH = 80
};

typedef struct {
    int32_t x, y;
} CockpitPoint;

/* 004f9628..004f9a07 and 004f9ad0..004f9b3b; the shapes point into the bank. */
typedef struct {
    uint8_t *dash, *tach, *sped;                /* 004f9628 'dash', 'tach', 'sped' */
    uint8_t *frames[6][COCKPIT_FRAMES];         /* 004f9634, 50, 6c, 88, a4, c0: 'w'l, 'w'r, 't'l, 's'l, 't'r, 's'r */
    uint8_t *digits[COCKPIT_DIGITS];            /* 004f96dc */
    int32_t unknown_4f9708;
    int32_t digits_on;                          /* 004f970c: the followed car's model is 3 or 0xb */
    uint8_t *gate, *gate_back;                  /* 004f9710 'gate', 'gbak' */
    uint8_t *lights[COCKPIT_LIGHTS];            /* 004f9718 */
    uint8_t *small_lit, *small_dark, *big_lit, *big_dark;  /* 004f9728 'smlt', 'smdk', 'bglt', 'bgdk' */
    int32_t light_count;                        /* 004f9738: the lights found, plus one past a missing one */
    uint8_t *palette;                           /* 004f973c: 0x300 bytes, 6-bit */
    uint8_t *bank;                              /* 004f9740 */
    int32_t unknown_4f9744;
    int32_t tach_count, sped_count;             /* 004f9748, 004f974c */
    int32_t unknown_4f9750;
    CockpitPoint dash_points[COCKPIT_DASH_POINTS];  /* 004f9754; 8 and 9 made relative to 2 and 3 */
    int32_t unknown_4f97a4[2];
    CockpitPoint tach_points[COCKPIT_GAUGE_POINTS]; /* 004f97ac */
    CockpitPoint sped_points[COCKPIT_GAUGE_POINTS]; /* 004f989c; 12 the middle of 11 and 13 */
    CockpitPoint gate_points[COCKPIT_GATE_POINTS];  /* 004f998c */
    int32_t tach_x, tach_y, tach_w, tach_h;     /* 004f99d4: its place and size */
    int32_t sped_x, sped_y, sped_w, sped_h;     /* 004f99e4 */
    int32_t fmm_words[COCKPIT_FMM_WORDS];       /* 004f99f4: the .fmm's words from +8 (004512f8) */
    const uint8_t *fmm;                         /* 004f9a04 */
    int32_t last_left, last_right;              /* 004f9ad0, 004f9ad4: the last steering frames found (high) */
    char name[COCKPIT_NAME];                    /* 004f9ad8 */
} Cockpit;

/* The words 0041f9a0 also sets outside the cockpit's. */
typedef struct {
    int32_t width, height;                      /* 0051201c, 00512020: 640 x 480 high, 320 x 200 low */
    int32_t unknown_4f9b40;                     /* 004b9154[track]: 480, 256 on track 3, 512 on 7 */
    int32_t unknown_4b91c4;
    int32_t unknown_4b92c4[4];                  /* 004b92c4..004b92d0 */
    int32_t unknown_4f9b48;
    int32_t back_saved;                         /* 004f9b64: 00421160 saved the box under 004c2d98 */
} CockpitWorld;

typedef struct {
    char bank[COCKPIT_PATH];    /* dir + name + DH.fsh / DL.fsh (00483e60) */
    char fmm[COCKPIT_PATH];     /* dir + name + DH.fmm / DL.fmm (00473d43, 00451278) */
} CockpitFiles;

/* 0041f9a0's names, `dir` paths slot 16 (00524698). Returns 0, or -1 when
 * one does not fit. */
int cockpit_paths(CockpitFiles *files, const char *dir, const char *name, int32_t high);

/* 0041f988: queues the dashboard's colours 0..0x2f (00446f90). */
void cockpit_queue_palette(RacePalette *pal, const uint8_t *palette);

/* 0041f9a0 (watcall name, full) after the bank is read: race->best_lap the
 * track's record (00524ec0 + 0xa6b track), unknown_4f9b40 004b9154[track],
 * best_lap_set 0; the name kept; the screen size; the palette ('!pal' + 0x10,
 * made 6-bit when its type is 0x24; without one, past 'dash'); the shapes
 * (high: each steering frame missing falls back to the last found of its
 * side, the 't' / 's' ones may stay NULL; low: all needed); 'gate', 'gbak';
 * the lights up to the first missing; the digits when the followed car's
 * model is 3 or 0xb. With full: the places and sizes of 'tach' and 'sped',
 * the 0x7c point lists of 'dash' (10), 'tach', 'sped' (their counts) and
 * 'gate' (9) offset by each shape's place and hotspot, and the .fmm's four
 * words. Then the HUD's start (mode 0, lost 0, redraw 1, strip_dirty 1,
 * the splits shown 0, with full the splits 0) and other words cleared; kind
 * 0's camera *dash_camera (004f9b7c) when not full, else both 0. Returns 0;
 * -1 where the original hangs or aborts (a needed shape missing, a point
 * list not found inside the bank) or a count above the port's 30. */
int cockpit_load(Cockpit *c, CockpitWorld *w, HudState *hud, RaceState *race, int32_t *dash_camera, uint8_t *bank,
                 size_t bank_size, const uint8_t *fmm, const char *name, int32_t high, int32_t full);

typedef struct {
    void *ctx;
    QuadTarget *(*select)(void *ctx, HudSurface s);  /* 00473680 / 004735dc: the draw surface, its clip its own */
    void (*labels)(void *ctx);                       /* 00420370 */
} CockpitRedrawOps;

typedef struct {
    int32_t back_buffer;            /* 004b95b4 */
    HudViewClipInputs view_clip;    /* for 0041e3f4; its high_res (004fa0c0) also picks the box */
    uint8_t *back_shape;            /* [004c2d98 + 0x2c] */
    const uint8_t *remap;           /* 004c6604 */
} CockpitRedrawInputs;

/* 00421160: on the screen (004b95b4 ? 004c2da0 : 004735dc's), 0041e3f4's
 * clip, the dash (004f9628) at (0, 0) with 0xff skipped (00473a98);
 * 004f9b64 = 1; the screen under the back shape's box at (0x208, 0x132)
 * high / (0x104, 0x7f) low grabbed into it (00476e28); on 004c2d98 the box
 * (0, 0)..(0x70, 0x8b) high / (0x38, 0x3b) low shaded through 004c6604
 * twice (004827cf); the screen again, 00420370, 004f9a20 = 99, 004f9a24 =
 * 0, 0041e230. */
void cockpit_redraw(const CockpitRedrawOps *ops, HudState *hud, const Cockpit *c, CockpitWorld *w,
                    const CockpitRedrawInputs *in);

typedef struct {
    void *ctx;
    void (*free_saved)(void *ctx);                  /* 00475260(004f9620) */
    void (*flip)(void *ctx);                        /* 00464a30 */
    QuadTarget *(*select)(void *ctx, HudSurface s); /* 00473680 / 004735dc */
    void (*restore)(void *ctx);                     /* 00473614 */
    void (*load)(void *ctx);                        /* 0041f9a0(004f9ad8, 0): the bank again */
    void (*palette_copy)(void *ctx);                /* 00476460(0, 0xfe, 004f9b80) */
    void (*palette_bank)(void *ctx, uint8_t *bank, uint8_t *palette); /* 00446fd0(004f9740, 004f973c) */
    void (*redraw)(void *ctx);                      /* 00421160 */
} CockpitReloadOps;

typedef struct {
    int32_t back_buffer;        /* 004b95b4 */
    int32_t unknown_509d70;     /* set: no palette and no redraw */
    int32_t unknown_4b92dc, unknown_4b92e0, unknown_4b94dc;
    int32_t dash_camera;        /* 004f9b7c */
    int32_t chase_kind;         /* 004f801c */
    int32_t unknown_4c5390;
} CockpitReloadInputs;

/* The words 004217a8 sets. */
typedef struct {
    int32_t unknown_4f9620;     /* freed (00475260) and cleared when set */
    int32_t unknown_513898;     /* 1 when 004c5390 != 1 */
    int32_t unknown_52b8f8;     /* 1 when 004c5390 == 1 */
} CockpitReloadState;

/* 004217a8 (on a view change), only while the bank (004f9740) is NULL:
 * 004f9620 freed and cleared when set; the screen (004b95b4: 00464a30 then
 * 004c2da0, else 004735dc's) filled with 0 within its clip (00482980(0));
 * 00473614 when 004b95b4 is 0; 0041f9a0(004f9ad8, 0); then unless 00509d70
 * is set: 00476460(0, 0xfe, 004f9b80), 00446fd0(004f9740, 004f973c), the
 * screen again, 00421160 when 004b92dc, 004b92e0, 004f9b7c, 004f801c and
 * 004b94dc are all 0, and 0052b8f8 = 1 when 004c5390 == 1, else 00513898 = 1. */
void cockpit_reload(const CockpitReloadOps *ops, Cockpit *c, CockpitReloadState *st, const CockpitReloadInputs *in);

#endif

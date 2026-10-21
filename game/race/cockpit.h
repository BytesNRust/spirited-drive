/* The in-car dashboard (the DASH / GDASH files, 0041f9a0 loads them): its
 * bank 004f9740 and 6-bit palette 004f973c. */
#ifndef TNFS_COCKPIT_H
#define TNFS_COCKPIT_H

#include "game/gfx/needle.h"
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
    int32_t unknown_4f9744;                     /* the original's pointer to 00482a36's block: CockpitColours.fade */
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
    void (*port_grab)(void *ctx, uint8_t *back_shape);  /* port: NULL keeps the exe's grab; else redoes it before the
                                                         * shading (the wide screen's map lies over other dash) */
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

typedef struct {
    void *ctx;
    void (*free_bank)(void *ctx, uint8_t *bank);                      /* 00475260(004f9740) */
    uint8_t *(*load)(void *ctx, const char *path);                     /* 00476268(path, 0) */
    void (*palette_read)(void *ctx);                                   /* 004766b8(0, 0x100, 004f9b80) */
    void (*palette_write)(void *ctx, const uint8_t *colours);          /* 00476460(0, 0xfe, colours) */
    void (*palette_bank)(void *ctx, uint8_t *bank, const uint8_t *palette); /* 00446fd0 */
    QuadTarget *(*select)(void *ctx, HudSurface s);                   /* 00473680 / 004735dc */
    void (*movie)(void *ctx);                                          /* 00464240 */
} CockpitPoliceOps;

typedef struct {
    int32_t back_buffer;            /* 004b95b4 */
    int32_t ticket;                 /* 004b92dc: the ticket, else (004b92e0) the warning */
    int16_t german;                 /* 0052b31c: the 'g' names */
    int16_t unknown_524e63;         /* set: 00464240 */
    const char *dir;                /* 005246e8: paths slot 17 */
    HudViewClipInputs view_clip;    /* 0041e3f4's; its unknown_4c5390 == 1 picks the 'D' names, else 'R', its
                                       high_res 'H' / 'L' */
} CockpitPoliceInputs;

/* The words 004214e4 keeps. */
typedef struct {
    uint8_t *screen_bank;           /* 004f9620: the loaded .qfs */
    uint8_t *screen;                /* 004f9624: its 'scrn' */
    int32_t loading;                /* 004fcdac: 1 while the file loads */
} CockpitPoliceState;

/* 004214e4 (from 00422598 / 0042270c while 004b92dc or 004b92e0 is set):
 * when 004f9620 is NULL: 004fcdac = 1; the dash bank freed (00475260) and
 * 004f9740 = 0; dir + "ticket" / "tickeg" (ticket, german) or "warn" /
 * "gwarn", + 'D' (004c5390 == 1) / 'R', + 'H' / 'L', + ".qfs" loaded
 * (00476268); 004f9624 = its 'scrn' (004732dd; missing: a hang);
 * 004766b8(0, 0x100, 004f9b80); its '!pal' + 0x10 (004732e4) to the screen
 * entries 0..0xfd (00476460) when 004c5390 == 1, else the bank mapped onto
 * the palette with it (00446fd0); 004fcdac = 0. Then the screen
 * (004b95b4 ? 004c2da0 : 004735dc's), 00464240 when the word 00524e63 is
 * set, 0041e3f4, 'scrn' at its place opaque (004737ec = 00473780). -1
 * where the original hangs or reads through NULL (no file, no 'scrn', no
 * '!pal', a path past its 80 bytes), 0 otherwise. */
int cockpit_police(const CockpitPoliceOps *ops, Cockpit *c, CockpitPoliceState *st, HudState *hud,
                   const CockpitPoliceInputs *in);

/* 0041f818's results outside the palette: 00482a36's block (004f9744
 * points at it), the 16.16 clip of the anti-aliased line 0048383b
 * (0052d384 left, 0052d380 top, 0052d354 right, 0052d35c bottom) and the
 * screen palette as it stood at the end (004f9b80, 6-bit, 004766b8). */
typedef struct {
    uint8_t fade[RACE_FADE_BYTES];
    int32_t line_left, line_top, line_right, line_bottom;
    uint8_t saved[RACE_PALETTE_COLOURS * 3];
} CockpitColours;

/* 0041f818 (from 00455a5c, before 004574ec): the screen palette read into
 * p->copy (004766b8); race_palette_enable (its first part); the inverse
 * table built (0047463c); the fade toward the followed car's model's colour
 * (004b91d0 row: (255, 0, 0), model 3 (255, 64, 0); 00482a36 over p->copy);
 * the line clip (0, 0, width << 16, height << 16 from 0051201c / 00512020);
 * the dash bank mapped onto the race palette with its own (00446fd0);
 * 0041e230; 004f9a24 = 004f9a28 = 0; the dash palette tinted in place
 * (004471dc(0, 0x100)); the screen palette read into out->saved. A model
 * past the table's 12 rows takes row 0 (the original reads past it). */
void cockpit_colours(RacePalette *p, const RaceState *race, const Cockpit *c, const CockpitWorld *w, HudState *hud,
                     CockpitColours *out);


/* 004212d4 (watcall (), from 0042270c), lights the count 004b92d4 sets:
 * while it is not 0, 'bglt' (004f9b48 set) or 'bgdk' opaque (00473764) at
 * dash point 2; when it differs from *shown (004f9a88): 0 puts 'bgdk' at
 * point 2 and 'smdk' at points 3..7, else 'smlt' at the first min(count, 5) of them and 'smdk'
 * at the rest; *shown = count. A count below -3 starts before the points
 * the port keeps: those are skipped (port guard). */
void cockpit_lights(const QuadTarget *t, const Cockpit *c, int32_t count, int32_t big_lit, int32_t *shown);


typedef struct {
    void *ctx;
    QuadTarget *(*select)(void *ctx, HudSurface s); /* 00473680 / 004735dc */
} CockpitSelectOps;

typedef struct {
    int32_t back_buffer;            /* 004b95b4 */
    int32_t use_parts;              /* 004f9b3c */
    HudViewClipInputs view_clip;    /* for 0041e3f4 */
    int32_t gate_always;            /* 004b92c0: the gate drawn also before a gear change */
} CockpitGateInputs;

/* 0041f5bc (watcall car, from 0042270c): the screen selected (004b95b4 ?
 * 004c2da0 : 004735dc's); with 004f9b3c set nothing more. Else 0041e3f4,
 * then with 004b92c0 0 and no gear change ticking (+0x421 <= 0): state 0,
 * 'gbak' at its place (004737ec) when *state (004b945c) was not 0. Else the
 * state and its point among the gate points, a = gear before last (+0x41d)
 * + 2, b = gear (+0x3ed) + 2: ticks below 0xb with no shift (+0x3bd < 1)
 * 1 at b; ticks from 0xe 2 at a; 0xd 3 at (a's x, point 1's y); 0xc 4 at
 * (b's x, point 1's y); 0xb with no shift 5 at b; else 0 at (0, 0). When
 * the state changed: 'gate' at its place and, with 4 lights found
 * (004f9738), lights 3..0 with 0xff skipped (00473a98) centred (their width
 * and height / 2, toward 0) from point 1 on in steps of a quarter of the
 * way (/ 4, toward 0); else light 0 centred at the point. *state = the
 * state. A gate point past the 9 reads (0, 0) and a NULL shape draws
 * nothing (port guards: the original reads on). */
void cockpit_gate(const CockpitSelectOps *ops, HudState *hud, const Cockpit *c, const Car *car,
                  const CockpitGateInputs *in, int32_t *state);

/* 00420d0c's inputs: the car's revs (+0x3c1), its specs' +0xac and its
 * speed (+0x68); the followed car's model (cars[004fbb08] +0x48a); the
 * resolution (004fa0c0), the language word (0052b31c: km/h) and 00482a36's
 * block (004f9744, CockpitColours.fade). */
typedef struct {
    int32_t rpm, redline, speed;
    int32_t model;
    int32_t high, german;
    const uint8_t *fade;
} CockpitGaugeInputs;

/* 00420d0c (watcall (car), from 0042270c): the tachometer's needle (0048383b)
 * from the point tach_count - 1 to the mark of min(rpm, redline + 0x3d4):
 * tach point rpm >> 10 moved toward the next by (rpm & 0x3ff) / 1024
 * (rounded down after the product); then the speed, |speed| x 0x23d70
 * (mph) or x 0x39999 (km/h) in 16.16 rounded at bit 15, then >> 16. Without the digits
 * (004f970c) the speedometer's needle from the point after the tachometer's
 * sped_count - 1 (sped_count 0 reads the last tach point) to speed point
 * speed / 10 (20 in km/h) moved toward the next by the rest (the product
 * divided, toward zero); 0 and below the first point. With them: three
 * digits, hundreds first, from (0xb0, 0xeb) 0xe apart high / (0x58, 0x62) 7
 * low, model 3 (0x131, 0x16d) 9 / (0x99, 0x98) 4; a 0 in the first two
 * places before any other digit is 'numb'; model 0xb skips colour 0xff
 * (004739b0), the others opaque (00473764). The needles are 0x20000 wide
 * at both ends for every model (004b91dc / e0), 0x10000 at low resolution.
 * Port guards: a point past the gate's reads (0, 0), a digit outside 0..9
 * (a negative speed) or a missing shape draws nothing. */
void cockpit_gauges(const QuadTarget *t, NeedleLine *line, const Cockpit *c, const CockpitGaugeInputs *in);

/* 0042270c's callees; the car (esi) is the context's. */
typedef struct {
    void *ctx;
    int32_t (*rand_below)(void *ctx, int32_t n);    /* 00443730 */
    void (*police)(void *ctx);                      /* 004214e4 */
    void (*reload)(void *ctx);                      /* 004217a8 */
    void (*redraw)(void *ctx);                      /* 00421160 */
    void (*select)(void *ctx, HudSurface s);        /* 00473680 / 004735dc */
    /* 0047383c: the clip rectangle (left, right, top, bottom) */
    void (*clip)(void *ctx, int32_t left, int32_t right, int32_t top, int32_t bottom);
    void (*fill)(void *ctx, uint32_t colour);       /* 00482980: the clip rectangle filled */
    /* 00483fa0([004c2d90 + 0x2c], runs): the view surface through a run list, a .fmm word (004f99f4) */
    void (*runs)(void *ctx, int32_t runs);
    void (*restore)(void *ctx);                     /* 00473614 */
    void (*mirror)(void *ctx, int32_t x, int32_t y);    /* 0044c060: the rear-view mirror at dash point 0 */
    void (*clip_box)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h);    /* 0044987c */
    void (*draw)(void *ctx, const uint8_t *shape);  /* 00473ab4: at its place, 0xff skipped */
    void (*place)(void *ctx, const uint8_t *shape); /* 00473780: at its place, opaque */
    void (*place_skip)(void *ctx, const uint8_t *shape);    /* 00473a50: at its place, 0xff skipped */
    void (*present)(void *ctx);                     /* 004737ec([004c2d90 + 0x2c]): the view surface at (0, 0) */
    void (*gauges)(void *ctx);                      /* 00420d0c(car) */
    void (*gate)(void *ctx);                        /* 0041f5bc(car) */
    void (*lights)(void *ctx);                      /* 004212d4 */
    void (*keys)(void *ctx);                        /* 00420710 */
    void (*top_row)(void *ctx);                     /* 00421e40(car) */
    void (*map)(void *ctx, int32_t x, int32_t y);   /* 004737d0([004c2d9c + 0x2c], x, y) */
    void (*replay)(void *ctx);                      /* 0044d640 */
} CockpitFrameOps;

typedef struct {
    int32_t unknown_509d70;         /* set: nothing at all */
    int32_t back_buffer;            /* 004b95b4 */
    int32_t high;                   /* 004fa0c0 */
    uint8_t screen_kind;            /* 004fb789: 3 copies the two last .fmm words */
    int32_t window_size;            /* 00511ff0 */
    int32_t unknown_4c5390;         /* 1: the taller box */
    int32_t model;                  /* the followed car's (cars[004fbb08] +0x48a) */
    int32_t use_parts;              /* 004f9b3c: the map drawn */
    uint32_t strip_colour;          /* 0050b7b4 */
    int32_t width, height;          /* 0051201c, 00512020 */
    int32_t unknown_4b9570;
} CockpitFrameInputs;

/* The words 0042270c keeps that live in other functions' states. */
typedef struct {
    int32_t *loading;               /* 004fcdac (CockpitPoliceState's) */
    uint8_t *clear_view;            /* 004fb78a (HudKeysState's) */
    int32_t *unknown_4b956c;        /* = 004b9570 */
} CockpitFrameWords;

/* 0042270c (watcall car, from 00422598 every frame in the car): nothing
 * while 00509d70 is set. The view's shake race->camera_pitch (004f9b70):
 * -0x8000 while landing_shake (004f9b74) is above 1, else 0, or off road
 * (+0x4a6) above 1.0 of speed (|+0x68|) rand_below(0x80) << 8 - 0x40.
 * With a stop notice (004b92dc / 004b92e0): 004214e4, hud 004f9a24 = 1,
 * 0044d640, done. Else 004fcdac = 1 around 004217a8, stop_notice_tick
 * (004b92e4) = -1000, 00421160 when hud 004f9a24 or 004f9a28 is set and
 * both 0. The steering frame from |+0x18|: high 0x21999, 0x70000, 0xc8000,
 * 0x133333, 0x1b0000 give 0..4, beyond 5; low 0x26666, 0x66666 give 0, 1,
 * up to 0x74ccc 2, then 0x154ccc, 0x1c6666 give 4, 5, beyond 6 (no 3: its
 * branch tests 0xa9999 under 0x74ccc). On the screen (004b95b4 ? 004c2da0
 * : 004735dc's) the view through the .fmm runs: screen kind 3 takes word 3,
 * first with clear_view the view surface filled with the strip colour
 * (clip the screen) and copied through word 2, clear_view 0; else word
 * window_size. 004b956c = 004b9570; without the back buffer 00473614 and
 * 004735dc around the mirror (0044c060 at dash point 0); 0044987c(0, 0,
 * 0x206 / 0x102, 0x1e0 / 0xc8 when 004c5390 is 1 else 0x1c0 / 0xb4). The
 * signed frame (negative steering negates it) against hud 004f9a20: on a
 * change the 'w' frame drawn, the left ones when steering left on a plain
 * track or right on a mirrored one (004fbad8); 004f9a20 = it. On the view
 * surface (clip the screen) 'tach', 'sped' (model 0xb with 0xff skipped),
 * 00420d0c, the 't' and 's' frames; on the screen the view surface clipped
 * to the tach's box and the sped's; 0041f5bc, 004212d4, 00420710,
 * 00421e40; with 004f9b3c the map at (0x206, 0x130) / (0x103, 0x7e);
 * 0044d640. A window size outside the four words skips its runs (port
 * guard: the original reads the words after them). */
void cockpit_frame(const CockpitFrameOps *ops, HudState *hud, RaceState *race, const Cockpit *c, const Car *car,
                   const CockpitFrameWords *w, const CockpitFrameInputs *in);

/* 00422c20's frees (00475260 each). */
typedef struct {
    void *ctx;
    void (*free_bank)(void *ctx, uint8_t *bank);        /* 004f9740 */
    void (*free_screen)(void *ctx, uint8_t *bank);      /* 004f9620 */
    void (*free_fade)(void *ctx);                       /* 004f9744 (the port's block is CockpitColours.fade) */
    void (*free_inverse)(void *ctx);                    /* 00474558: 004c5ff4 */
    void (*free_fmm)(void *ctx, const uint8_t *fmm);    /* 004f9a04 */
} CockpitTeardownOps;

typedef struct {
    int32_t *unknown_513e60;    /* 1 cleared (00455a5c sets it; 0044cc74 reads it) */
    int32_t *unknown_4b91c8;    /* cleared (0044cc74 reads it) */
} CockpitTeardownWords;

/* 00422c20 (watcall (), from 00455a5c's end at 00456044): the dash bank,
 * the police screen's bank, the fade block and the inverse table (00474558)
 * freed when set, each then NULL; the palette entries 0x5e, 0x5f and
 * 0xfa..0xfd enabled again; both stop notices 0; 00513e60 0 when it was 1;
 * 004b91c8 0; the .fmm freed even when NULL, its pointer left as it was. */
void cockpit_teardown(const CockpitTeardownOps *ops, Cockpit *c, CockpitPoliceState *st, RacePalette *p,
                      RaceState *race, const CockpitTeardownWords *w);

#endif

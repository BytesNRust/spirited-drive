/* port: the EXTRAS screen, reached from Game Mode (where the Modem row's art shows the "EXTRAS" label)
 * and from Options (a new last entry). Two pages of switches, painted with the game's text routine
 * (plus_text / plus_panel / plus_text_width) over a darkened panel of the live screen. The net effect
 * of Done is to save extras.cfg and the unlock level into config.dat. Cancel and Escape leave both
 * untouched (the caller copies them back). */
#ifndef TNFS_EXTRAS_MENU_H
#define TNFS_EXTRAS_MENU_H

#include "game/sys/port_extras.h"

#include <stdint.h>

/* the page-1 rows, in order: Mirror, Rally, Warrior, Lost Vegas, Explode, Nitro, Turbo, Unlock Level, Done, Cancel */
enum { ROW_MIRROR = 0, ROW_RALLY, ROW_WARRIOR, ROW_VEGAS, ROW_EXPLODE, ROW_NITRO, ROW_TURBO, ROW_UNLOCK, ROW_DONE,
       ROW_CANCEL_P1, ROWS_PAGE_1 };

/* the page-2 rows: Replay Cameras, Warrior as Rival, No Movies, Start Screen (Game Mode / Network / Control Central),
 * No Assists, Modern 3D, Texture Filter, Fog, Done, Cancel */
enum { ROW_REPLAY = 0, ROW_RIVAL, ROW_MOVIES, ROW_START, ROW_NO_ASSISTS, ROW_MODERN, ROW_FILTER, ROW_FOG, ROW_DONE_P2,
       ROW_CANCEL_P2, ROWS_PAGE_2 };

/* the page indicator's bounds, mirroring graphics_plus's PLUS_NAV_* so the nav-hit answer is identical */
enum { EXTRAS_NAV_X0 = 270, EXTRAS_NAV_X1 = 370, EXTRAS_NAV_Y0 = 432, EXTRAS_NAV_Y1 = 476 };

typedef struct ExtrasMenuOps {
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);
    int32_t (*load_art)(void *ctx, const char *name);
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);
    void (*palette_set)(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode);
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);
    void (*select)(void *ctx, int32_t picture);
    void (*blit_own)(void *ctx, const uint8_t *shape);
    int32_t (*picture_shape)(void *ctx, int32_t picture);
    void (*select_default)(void *ctx);
    void (*pointer_hide)(void *ctx);
    void (*flip)(void *ctx);
    void (*pointer_show)(void *ctx);
    int32_t (*pointer_buttons)(void *ctx);
    void (*text_styles)(void *ctx);
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);
    void (*deadline_set)(void *ctx, int32_t time);
    int32_t (*deadline_passed)(void *ctx);
    void (*sound)(void *ctx, int32_t sound);
    int32_t (*ticks)(void *ctx);
    void (*pointer_activate_if_moved)(void *ctx);
    int32_t (*key)(void *ctx);
    void (*pointer_click)(void *ctx);
    void (*pointer_off)(void *ctx);
    void (*music)(void *ctx);
    /* port: the game's text routine (0046376c's styles, the name box's shade and frame). The same ones
     * graphics_plus.c borrows from the Options glue. */
    void (*plus_text)(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y);
    int32_t (*plus_text_width)(void *ctx, const char *text);
    void (*plus_panel)(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed);
    /* port: persist the on-screen state — extras.cfg from `e` and the unlock level. The caller decides the files. */
    void (*extras_save)(void *ctx, const PortExtras *e, int16_t unlock);
    /* port: 1 lets Mirror and Rally both stay on (the race runs the mirror's geometry on the rally's track),
     * where the exe's screen switches one off as the other is turned on. 0 is the exe. */
    int port_combine_variants;
    void *ctx;
} ExtrasMenuOps;

/* The two pages, run sequentially over the screen's live picture. 1 on Done (extras_save was called),
 * 0 on Cancel/Escape (the input PortExtras and unlock level are unchanged). */
int extras_menu_run(PortExtras *e, int16_t *unlock, const ExtrasMenuOps *o);

#endif

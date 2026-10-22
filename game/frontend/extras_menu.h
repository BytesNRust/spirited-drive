/* The CUSTOMIZE GAME screen, reached from Game Mode (where the Modem row's art shows the "EXTRAS" label)
 * and from Options (a new last entry). Two pages of switches grouped under the four headers the owner named,
 * painted with the game's text routine (plus_text / plus_panel / plus_text_width) over a darkened panel of the
 * live screen. The net effect of Done is to save extras.cfg and the unlock level into config.dat. Cancel and
 * Escape leave both untouched (the caller copies them back). */
#ifndef TNFS_EXTRAS_MENU_H
#define TNFS_EXTRAS_MENU_H

#include "game/sys/port_extras.h"

#include <stdint.h>

/* the clean background's size, as config_bgnd.h's CONFIG_BGND_W and _H */
enum { EXTRAS_BGND_W = 640, EXTRAS_BGND_H = 480 };

/* the page indicator's bounds, mirroring graphics_plus's PLUS_NAV_* so the nav-hit answer is identical */
enum { EXTRAS_NAV_X0 = 270, EXTRAS_NAV_X1 = 370, EXTRAS_NAV_Y0 = 432, EXTRAS_NAV_Y1 = 476 };

/* port: the two pages, and the four groups the owner named, verbatim:
 *
 *   page 1  Game modes         Mirror track mode, Rally track mode, Explode mode, Nitro mode, Turbo mode
 *            Driving & cameras  No driving assists, Bumper view mirror, Bumper view police radar,
 *                                Replay cameras in race mode
 *   page 2  Unlocks            Enable Warrior Player Car, Enable Warrior Rival Car, Enable Lost Vegas Track,
 *                                Unlock Level, Time with variants
 *            Startup            Start Screen, Disable intro video
 *
 * The rows below are that list in order, so a group is a range of them. The list is the switches only: Done and
 * Cancel are the clean background's own words, and the title and the page indicator are the screen's, so none of
 * those is a row to walk. Explode, Nitro and Turbo are Game modes (they change how a race runs) and No Movies is
 * Startup's "Disable intro video". Modern 3D, Texture Filter and Fog are not here at all: they are on the port's
 * own Graphics screen (graphics_own.c) and edited in port.cfg from there. */
enum { EXTRAS_PAGES = 2, EXTRAS_GROUPS = 2 };
enum { GROUP_MODES = 0, GROUP_CAMERAS, GROUP_UNLOCKS, GROUP_START };   /* the two groups of each page, in order */
enum {
    ROW_MIRROR = 0, ROW_RALLY, ROW_EXPLODE, ROW_NITRO, ROW_TURBO,
    ROW_ASSISTS, ROW_BUMPER_MIRROR, ROW_BUMPER_RADAR, ROW_REPLAY,
    ROW_FIRST_P2, ROW_RIVAL, ROW_VEGAS, ROW_UNLOCK, ROW_VARIANT_TIME,
    ROW_START, ROW_MOVIES,
    EXTRAS_ROWS                                    /* every switch on both pages, in the order above */
};
/* where page 2 starts, and how many rows each page shows: the second page is the rest of the list */
enum { ROWS_P1 = ROW_FIRST_P2, ROWS_P2 = EXTRAS_ROWS - ROW_FIRST_P2 };

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
    /* port: build the text styles for the caller's palette. The screen asks for it once as it opens (for its own
     * palette) and once as it closes, so whatever is drawn next is drawn the way its caller means. */
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
     * graphics_plus.c borrows from the Options glue. style 0 cream, 1 yellow, 2 red. */
    void (*plus_text)(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y);
    int32_t (*plus_text_width)(void *ctx, const char *text);
    void (*plus_panel)(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed);
    /* port: the configuration screens' clean background, so the screen is not drawn over the caller's picture.
     * `plus_bgnd` composes it into the screen's own buffers, asked for once as it opens (config_bgnd_load takes
     * about half a second), and says 0 when there is none; `plus_bgnd_shape` then puts it on with its palette.
     * Both may be NULL, which leaves the caller's picture behind the panel. */
    int32_t (*plus_bgnd)(void *ctx, uint8_t pixels[EXTRAS_BGND_W * EXTRAS_BGND_H], uint8_t palette[768]);
    int32_t (*plus_bgnd_shape)(void *ctx, const uint8_t pixels[EXTRAS_BGND_W * EXTRAS_BGND_H],
                               const uint8_t palette[768]);
    /* port: the smooth text, the same draw Options' rows and the Graphics screen's use: the background put back
     * under the letters before they are written, then the letters themselves at 1/1. This screen repaints its
     * whole page on each redraw -- it blits the clean background whole and then writes over it -- so every word it
     * draws needs this, or a row the pointer moved past leaves its old edges behind. NULL falls back to
     * plus_text. */
    void (*plus_text_smooth)(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y);
    /* port: the screen's title in the original screens' look (their red, at their items' size); NULL draws it as a
     * row */
    void (*plus_title)(void *ctx, const char *text, int32_t x, int32_t y);
    /* port: a group's header as the credits draw theirs (tan capitals, underlined `width` pixels long); NULL draws it
     * as a red row */
    void (*plus_header)(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);
    /* port: persist the on-screen state — extras.cfg from `e` and the unlock level. The caller decides the files. */
    void (*extras_save)(void *ctx, const PortExtras *e, int16_t unlock);
    /* port: 1 lets Mirror and Rally both stay on (the race runs the mirror's geometry on the rally's track),
     * where the exe's screen switches one off as the other is turned on. 0 is the exe. */
    int port_combine_variants;
    void *ctx;
} ExtrasMenuOps;

/* The two pages, run sequentially over the screen's live picture. 1 on Done (extras_save was called),
 * 0 on Cancel/Escape (the input PortExtras and unlock level are unchanged). `bank` is the Options' option/options.qfs
 * bank id, used for the Done/Cancel art (donl/canl/dond/cand) so the bottom row matches the exe's Options look. */
int extras_menu_run(PortExtras *e, int16_t *unlock, int32_t bank, const ExtrasMenuOps *o);

#endif
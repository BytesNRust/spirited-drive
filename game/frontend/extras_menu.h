/* The CUSTOMIZE GAME screen, reached from Game Mode (where the Modem row's art shows the "EXTRAS" label)
 * and from Options (a new last entry). Two pages of switches grouped under the four headers the owner named,
 * painted with the game's text routine (plus_text / plus_panel / plus_text_width) over a darkened panel of the
 * live screen. The net effect of Done is to save the mode's file and the unlock level into config.dat. Cancel and
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
 *   page 1  Cheats             Explode mode, Nitro mode, Turbo mode, Warrior Player Car
 *            Unlocks & mods     Warrior Rival Car, Mirror track mode, Rally track mode, Lost Vegas Track,
 *                               Lunar Springs Track, Lunar Gravity, Unlock Level, Car Colours
 *   page 2  In-race options    Speed units, No driving assists, Bumper view mirror, Exterior views police radar,
 *                               Replay cameras in race mode, Rival car markers (F6), 3D render stats (F12),
 *                               Dash box (Map / Gear lever / Hidden), 360 Camera,
 *                               Difficulty (Easy / Original / Hard)
 *  page 3  Miscellaneous      Start Screen, Intro video, Updates and anonymous stats, Open the log folder and Restore profile defaults (actions, no value)
 *
 * Pages 2 and 3 have one group each, so one header; page 1 has two.
 *
 * (Rival car markers and 3D render stats are the race's switches: the mode's file's car_markers and stats_overlay. Page 1
 * is thirteen rows and two headers, fifteen lines, which is what the 20 pitch fits.)
 *
 * The rows below are that list in order, page 1's first, so a page is the first ROWS_P1 of them and page 2 the rest,
 * and a group is a range of them. The list is the switches only: Done and Cancel are the clean background's own words,
 * and the title and the page indicator are the screen's, so none of those is a row to walk. Explode, Nitro and Turbo
 * are Cheats (they change how a race runs) and No Movies is Miscellaneous' "Intro video" (On while no_movies is 0).
 * Modern 3D, Texture Filter and Fog are not here at all: they are on the port's own Graphics screen (graphics_own.c)
 * and edited in port.cfg from there. */
enum { EXTRAS_PAGES = 3, EXTRAS_GROUPS = 2 };
enum { GROUP_CHEATS = 0, GROUP_UNLOCKS, GROUP_IN_RACE, GROUP_MISC };   /* the groups, in the pages' order */
enum {
    ROW_EXPLODE = 0, ROW_NITRO, ROW_TURBO, ROW_WARRIOR,                                      /* Cheats */
    ROW_RIVAL, ROW_MIRROR, ROW_RALLY, ROW_VEGAS, ROW_LUNAR, ROW_LUNAR_GRAVITY, ROW_UNLOCK,   /* Unlocks & mods */
    ROW_EXTRA_LIVERY,                                /* port: Unlocks & mods' last, the car colours: the console version liveries and the catalogue paints (port_extras.h) */
    ROW_UNITS, ROW_ASSISTS, ROW_BUMPER_MIRROR, ROW_BUMPER_RADAR, ROW_REPLAY, ROW_MARKERS,   /* In-race options */
    ROW_STATS,                                     /* port: In-race options' last, the race's F12 switch */
    ROW_DASH_BOX,                                  /* port: In-race options' the dash's map box (port_extras.h) */
    ROW_FOLLOW_CAMERA,                             /* port: In-race options' 360 camera (port_extras.h's follow_camera) */
    ROW_DIFFICULTY,                                /* port: In-race options' last, Easy / Original / Hard (port_extras.h's difficulty, difficulty.h) */
    ROW_START, ROW_MOVIES,                                                                   /* Miscellaneous */
    ROW_UPDATE,                                    /* port: Miscellaneous' last, Check for updates (port_extras.h's update_check, platform/update.h) */
    ROW_LOG_FOLDER,                                /* port: Miscellaneous' last, Open the log folder: an action row (ExtrasMenuOps.open_log_folder), for bug reports */
    ROW_RESET,                                     /* port (modes): Miscellaneous' last, Restore profile defaults: an action row (ExtrasMenuOps.reset_profile) */
    EXTRAS_ROWS                                    /* every switch on both pages, in the order above */
};
/* where page 2 starts, and how many rows each page shows: the second page is the rest of the list */
enum { ROW_FIRST_P2 = ROW_UNITS, ROWS_P1 = ROW_FIRST_P2, ROW_FIRST_P3 = ROW_START, ROWS_P2 = ROW_FIRST_P3 - ROW_FIRST_P2, ROWS_P3 = EXTRAS_ROWS - ROW_FIRST_P3 };

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
    /* port: the display faded out as the screen closes, the way 00423d28's close_screen does, so the menu that opened
     * it comes back up in its own palette instead of reading this screen's. May be NULL for a screen whose caller
     * fades out before opening it. */
    void (*fade_out)(void *ctx, int32_t first, int32_t count);
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
    /* port: persist the on-screen state — the mode's file from `e` and the unlock level. The caller decides the files. */
    void (*extras_save)(void *ctx, const PortExtras *e, int16_t unlock);
    /* port: 1 lets Mirror and Rally both stay on (the race runs the mirror's geometry on the rally's track),
     * where the exe's screen switches one off as the other is turned on. 0 is the exe. */
    int port_combine_variants;
    void *ctx;
    /* port: the page indicator's triangles (page_arrows.h): a mask's size, 0 when the CD gives none, and one drawn in
     * a style's colours (lit yellow, else cream). Both NULL keeps the text "1/2 >". */
    int32_t (*plus_arrow_size)(void *ctx, int32_t right, int32_t *w, int32_t *h);
    void (*plus_arrow)(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y);
    /* port: the Open the log folder row's action, run on Return, Space or a click on it (not on the arrows); the platform's file browser on the folder that holds tnfs.log.
     * NULL: the row does nothing. */
    void (*open_log_folder)(void *ctx);
    /* port (modes): the Profile in use: a row its mode does not leave free is dimmed, shows the mode's value and does not change */
    int profile;
    /* port (modes): the Restore row's action, on the second Return: the mode's file emptied and `e` set to what it now reads. NULL: the row does nothing */
    void (*reset_profile)(void *ctx, PortExtras *e);
} ExtrasMenuOps;

/* The two pages, run sequentially over the screen's live picture. 1 on Done (extras_save was called),
 * 0 on Cancel/Escape (the input PortExtras and unlock level are unchanged). `bank` is the Options' option/options.qfs
 * bank id, used for the Done/Cancel art (donl/canl/dond/cand) so the bottom row matches the exe's Options look. */
int extras_menu_run(PortExtras *e, int16_t *unlock, int32_t bank, const ExtrasMenuOps *o);

#endif

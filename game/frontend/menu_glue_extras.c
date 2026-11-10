/* The EXTRAS screen's glue (the Game Mode MODEM slot's) and Game Mode's text over the live art (menu_glue.h; split from menu_glue.c by responsibility). */
#include "game/frontend/menu_glue_private.h"

/* port: the EXTRAS dispatcher the Game Mode's MODEM slot calls when the original modem flow is gone. It opens the
 * EXTRAS screen over the live Game Mode picture and saves the mode's file and the unlock level on Done. */

/* the Game Mode screen's current picture (set before the menu is entered) */
static int32_t mode_extras_screen;

/* port: the Open the log folder row of the EXTRAS screen: the file browser on the user folder, where tnfs.log is */
static void open_log_folder_glue(void *ctx)
{
    (void)ctx;
    if (!platform_open_user_dir()) port_log("extras: the log folder could not be opened\n");
}

static void extras_save_glue(void *ctx, const PortExtras *e, int16_t unlock)
{
    (void)ctx;
    port_extras_save(e);
    resource_set_units(e->units);   /* the speeds and distances on the screens and in the race, as saved */
    config_dat_set_unlock_level(config_current(), unlock);
    config_save();   /* port: Done stores the level now, not whenever the menus next save config.dat */
}

/* each frame starts with select(0): the back buffer with the calling menu's picture blitted on it, so the EXTRAS
 * text goes over that menu instead of a black field. */
static void extras_select_glue(void *ctx, int32_t picture)
{
    (void)ctx, (void)picture;
    ea_select_default();
    ea_blit_own((const uint8_t *)(intptr_t)ea_picture_shape(mode_extras_screen));
}
static void extras_blit_own_glue(void *ctx, const uint8_t *shape) { (void)ctx; ea_blit_own(shape); }
/* the panel shaded with the palette on screen (the caller's; extras_menu has none of its own) */
static void extras_panel_glue(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    uint8_t shown[768];
    if (rgb == NULL) video_get_palette(shown), rgb = shown;
    glue_plus_panel(ctx, rgb, x0, y0, x1, y1, framed);
}
/* port: the same clean background the Graphics screen uses, out of the one copy central_glue.c composed */
static int32_t extras_bgnd_glue(void *ctx, uint8_t pixels[EXTRAS_BGND_W * EXTRAS_BGND_H], uint8_t palette[768])
{
    (void)ctx;
    return glue_own_bgnd(pixels, palette);
}
static int32_t extras_bgnd_shape_glue(void *ctx, const uint8_t pixels[EXTRAS_BGND_W * EXTRAS_BGND_H],
                                      const uint8_t palette[768])
{
    return glue_own_bgnd_shape(ctx, pixels, palette);
}

/* port: the EXTRAS screen's letters, at the game's own text size (1/1) rather than Game Mode's art size. Its draw_page
 * blits the clean background and shades the panel over it before every write, so nothing is put back under the
 * letters: the clean background there would cover the panel with a light box behind each word.
 *
 * The screen asks for its styles (its text_styles slot) once the fade has put its own palette on, so its letters come
 * from the palette that goes with the background it blits; and once as it closes, after the fade out, so the caller's
 * next is drawn with a ramp built from the caller's own palette. */
static void extras_text_smooth_glue(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    own_panel_text(ctx, style, text, x, y);
}
/* Game Mode's palette as the clean cover read it (mode_cover): the label is first drawn before the fade puts it on
 * screen, when the screen's palette is still black */
uint8_t menu_glue_mode_palette[768];
int menu_glue_mode_palette_known;

/* Game Mode's items are art drawn larger than the game's text: "Single player" is 263 wide there, 158 in graver18 */
enum { MODE_TEXT_NUM = 33, MODE_TEXT_DEN = 20 };

/* the styles for the palette on screen: the clean background's once it is up, and the caller's own before that */
static void extras_text_styles_glue(void *ctx)
{
    (void)ctx;
    if (menu_glue_mode_palette_known) text_build_styles_from(menu_glue_mode_palette);
    else text_build_styles();
}

/* port: a label the items' own letters give (item_art.h: "Customize game"), cut from the CD the first time and kept;
 * a label the CD cannot give is drawn in the font */
static const uint8_t *item_mask(const char *text, int *w)
{
    static uint8_t masks[ITEM_ART_COUNT][ITEM_ART_MAX_W * ITEM_ART_H];
    static int widths[ITEM_ART_COUNT], tried[ITEM_ART_COUNT], ok[ITEM_ART_COUNT];
    const ItemArtId id = item_art_for(text);
    if (id == ITEM_ART_COUNT) return NULL;
    if (!tried[id]) {
        tried[id] = 1;
        ok[id] = item_art_load(resource_load_art, id, masks[id], &widths[id]);
        if (!ok[id]) port_log("item: \"%s\" not on the CD, drawn in the font\n", text);
    }
    *w = widths[id];
    return ok[id] ? masks[id] : NULL;
}

/* port: the Modem item's capitals in Game Mode: its shape stands at (189, 207) and its cap row is the shape's row 1 */
enum { MODEM_CAP_TOP = 208 };

/* The Game Mode label, at the items' size, and its width at that size. "Customize game" is the items' own letters
 * (item_art.h), its mask standing where the Modem item's capitals do; the width is the mask's, so the label stays
 * centred where the caller puts it. */
void menu_glue_extras_text_glue(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    extras_text_styles_glue(ctx);
    text_select((TextStyle)style);
    int w = 0;
    const uint8_t *const art = item_mask(text, &w);
    if (art != NULL) text_draw_item_mask(art, w, ITEM_ART_H, x, MODEM_CAP_TOP - ITEM_ART_PAD);
    else text_draw_smooth(text, x, y, MODE_TEXT_NUM, MODE_TEXT_DEN);
}
int32_t menu_glue_extras_text_width_glue(void *ctx, const char *text)
{
    (void)ctx;
    int w = 0;
    if (item_mask(text, &w) != NULL) return w;
    return text_width_scaled(text, MODE_TEXT_NUM, MODE_TEXT_DEN);
}
static int32_t extras_deadline_passed_glue(void *ctx) { (void)ctx; return 0; }

/* port (modes): a change of the window's shape, scaling, full screen or rate is taken by the window at once */
void menu_glue_window_follows(const PortSettings *was)
{
    const PortSettings now = port_settings_load();
    if (was->format != now.format || was->refresh_hz != now.refresh_hz || was->scale_integer != now.scale_integer || was->fullscreen != now.fullscreen) {
        const PlatformConfig config = port_platform_config(NULL);
        if (!platform_reconfigure(&config)) port_log("profile: the window keeps its old settings\n");
    }
}

/* port (modes): the Restore row: the mode in use back to its own values, what is on screen with it */
static void extras_reset_glue(void *ctx, PortExtras *e)
{
    (void)ctx;
    const PortSettings was = port_settings_load();
    if (!port_profile_reset(was.profile)) port_log("profile: %s could not be emptied\n", port_profile_file((Profile)was.profile));
    *e = port_extras_load();
    resource_set_units(e->units);
    menu_glue_window_follows(&was);
}

/* port: the EXTRAS screen over whichever menu's live picture called it (Game Mode's Modem slot, Options' Extras
 * row). `unlock` is the caller's level: the one config.dat holds. Returns 1 on Done, 0 on Cancel / Escape. */
/* the ops table: most slots are statics from this file's glue; the EXTRAS-specific ones (text/panel/save and
 * the select that restores the caller's picture to the back buffer) live next to glue_extras_screen */
static ExtrasMenuOps extras_menu_ops(void)
{
    return (ExtrasMenuOps){
        move_to,            /* pointer_move_to */
        load_art,           /* load_art */
        shape,              /* shape */
        palette_set,        /* palette_set */
        in_rect,            /* pointer_in_rect */
        extras_select_glue, /* select */
        extras_blit_own_glue, /* blit_own */
        picture_shape,      /* picture_shape */
        select_default,     /* select_default */
        nothing,            /* pointer_hide */
        flip,               /* flip */
        show,               /* pointer_show */
        buttons,            /* pointer_buttons */
        extras_text_styles_glue, /* text_styles */
        fade_in,            /* fade_in */
        fade_out,           /* fade_out: the display faded as this screen closes (00423d28's close_screen) */
        deadline_set,       /* deadline_set */
        extras_deadline_passed_glue, /* deadline_passed */
        sound,              /* sound */
        ticks,              /* ticks */
        activate,           /* pointer_activate_if_moved */
        key,                /* key */
        click,              /* pointer_click */
        off,                /* pointer_off */
        music,              /* music */
        own_panel_text,     /* plus_text: at 1/1, as plus_text_smooth */
        glue_plus_text_width, /* plus_text_width: 1/1, the size the words are drawn at, so the values line up */
        extras_panel_glue,  /* plus_panel */
        extras_bgnd_glue,   /* plus_bgnd */
        extras_bgnd_shape_glue, /* plus_bgnd_shape */
        extras_text_smooth_glue, /* plus_text_smooth */
        own_title_text,     /* plus_title */
        own_header_text,    /* plus_header */
        extras_save_glue,   /* extras_save */
        1,                  /* port_combine_variants: turning Mirror or Rally on leaves the other on (0 the exe) */
        NULL,               /* ctx */
        glue_plus_arrow_size, /* plus_arrow_size: the page indicator's triangles */
        own_panel_arrow,    /* plus_arrow */
        open_log_folder_glue, /* open_log_folder: the Open the log folder row */
        port_settings_profile(), /* profile: the mode in use, which says what can be changed */
        extras_reset_glue,  /* reset_profile: the Restore row's action */
    };
}

int32_t glue_extras_screen(int16_t unlock)
{
    PortExtras e = port_extras_load();
    /* the Done/Cancel art (donl/canl/dond/cand) lives in the Options bank; load it before draw_page blits and free it
     * afterwards. The Options menu loaded its bank before calling here too, but a separate load keeps each side
     * independent. */
    const int32_t options_bank = load_art(NULL, "option/options.qfs");
    const ExtrasMenuOps ops = extras_menu_ops();
    mode_extras_screen = ea_screen();
    /* port: the page keys, so PageDown turns the page as the screen's own words promise (extras_menu.c's header).
     * They were read for the Graphics screen alone (central_glue.c's graphics()); without this the PageDown branch
     * there is dead code, and a scripted walk cannot turn the page. */
    input_page_keys(1);
    const int32_t result = extras_menu_run(&e, &unlock, options_bank, &ops);
    input_page_keys(0);
    ea_bank_free(options_bank);
    return result;
}

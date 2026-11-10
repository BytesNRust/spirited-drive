/* The checked menus over game/sys and ea_lib (menu_glue.h): Game Mode, the profile row and the start screen; the boxes, screens and EXTRAS glue are in menu_glue_dialogs.c, menu_glue_screens.c and menu_glue_extras.c */
#include "game/frontend/menu_glue_private.h"

MenuWords menu_words;
MenuCheats menu_cheats;

uint8_t menu_glue_screen_palette[PALETTE_PUT_BYTES];  /* 0052ced8: what 00463e08 leaves */

/* 004376ac's words while it runs: hosting or joining from its network ops changes them */
static GameModeState *mode_state;

static MenuWords words_of(const GameModeState *st)
{
    return (MenuWords){ st->lost, st->lost_shown, st->mode_menu, st->redraw, st->intro_seen, st->single,
                        st->network_raced };
}

static void words_into(GameModeState *st)
{
    st->mode_menu = menu_words.mode_menu, st->intro_seen = menu_words.intro_seen, st->single = menu_words.single;
    st->network_raced = menu_words.network_raced, st->lost = menu_words.link_game;
    st->lost_shown = menu_words.link_lost, st->redraw = menu_words.redraw;
}

static int32_t mode_network_found(void *ctx)
{
    menu_words = words_of(mode_state);
    const int32_t r = menu_glue_network_found(ctx);
    words_into(mode_state);
    return r;
}

static int32_t mode_network_game(void *ctx)
{
    menu_words = words_of(mode_state);
    const int32_t r = menu_glue_network_game(ctx);
    words_into(mode_state);
    return r;
}

/* port: the Modem item's words covered with the configuration screens' clean background in Game Mode's palette,
 * built once (it takes a while) and kept */
static void mode_cover(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h)
{
    static uint8_t *pixels;   /* the clean background, kept: it takes a while to compose, and each rectangle asked (the Modem words, the profile strip) is cut from it */
    static int tried;
    (void)ctx;
    if (!tried) {
        tried = 1;
        pixels = malloc(CONFIG_BGND_W * CONFIG_BGND_H);
        menu_glue_mode_palette_known = pixels != NULL && config_bgnd_load(resource_load_art, CONFIG_BGND_GAME_MODE, 0, pixels,
                                                                menu_glue_mode_palette);
        if (!menu_glue_mode_palette_known) {
            port_log("game mode: no clean background, the Modem words stay under the label\n");
            free(pixels), pixels = NULL;
        }
    }
    if (pixels == NULL) return;
    uint8_t *cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * (size_t)h);
    if (cover != NULL && config_bgnd_shape(pixels, x, y, w, h, cover) != 0) ea_blit_own(cover);
    free(cover);
}

/* port (modes): the mode `steps` along the cycle from `profile` (Stock, Restored, Restomod, round) */
static int profile_cycled(int profile, int steps)
{
    const int along = (profile + steps) % PROFILE_COUNT;
    return along < 0 ? along + PROFILE_COUNT : along;
}

/* port (modes): `profile` made the one in use (only its name is written: each mode keeps its own) */
static void profile_apply(int profile, const PortSettings *was)
{
    if (!port_profile_select(profile)) port_log("profile: the file could not be saved\n");
    menu_glue_window_follows(was);
    resource_set_units(port_extras_load().units);   /* each mode has its own */
}

/* port (profiles): the profile rows of Game Mode and Options read, peek at or step the profile (game_mode.h and options_menu.h say which step is which) */
int32_t glue_profile(void *ctx, int32_t step, const char *text[3])
{
    enum { REACH = 3 };   /* PEEK / APPLY take n from -REACH to REACH */
    (void)ctx;
    const PortSettings was = port_settings_load();
    int profile = was.profile;
    int32_t shown = 1;
    if (step >= GAME_MODE_PROFILE_PEEK - REACH && step <= GAME_MODE_PROFILE_PEEK + REACH) {   /* Game Mode: shown, not applied */
        profile = profile_cycled(profile, step - GAME_MODE_PROFILE_PEEK);
        shown = profile != was.profile ? 2 : 1;
    } else if (step >= GAME_MODE_PROFILE_APPLY - REACH && step <= GAME_MODE_PROFILE_APPLY + REACH) {   /* Game Mode: applied after its Yes */
        profile = profile_cycled(profile, step - GAME_MODE_PROFILE_APPLY);
        if (profile != was.profile) profile_apply(profile, &was);
    } else if (step >= GAME_MODE_PROFILE_LOOK - REACH && step <= GAME_MODE_PROFILE_LOOK + REACH) {   /* Game Mode: does applying it change the picture? */
        profile = profile_cycled(profile, step - GAME_MODE_PROFILE_LOOK);
        const PortSettings next = port_settings_of_profile(profile);
        shown = next.format != was.format || next.render_scale != was.render_scale;
        profile = was.profile;   /* nothing is changed */
    } else if (step == 1 || step == -1) {   /* Options: a step applies at once (its Cancel puts the files back) */
        profile = profile_cycled(profile, step);
        profile_apply(profile, &was);
    }
    text[0] = port_profile_title((Profile)profile);
    text[1] = port_profile_line1((Profile)profile);
    text[2] = port_profile_line2((Profile)profile);
    return shown;
}

/* port: the foot line of Game Mode: the version, and the update notice once the check has answered */
static int32_t mode_footer(void *ctx, int32_t line, char *text, size_t cap)
{
    char latest[32] = "";
    (void)ctx;
    UpdateState state = platform_update_check_poll(latest, sizeof latest);
#ifdef TNFS_SOURCE_DIR   /* development builds only (TNFS_RELEASE leaves it undefined): TNFS_UPDATE_FAKE=9.9.9 shows the notice */
    const char *const fake = getenv("TNFS_UPDATE_FAKE");
    if (fake != NULL && fake[0] != 0) {
        snprintf(latest, sizeof latest, "%s", fake[0] == 'v' ? fake + 1 : fake);
        state = UPDATE_NEWER;
    }
#endif
    return version_footer_line(text, cap, line, TNFS_VERSION, state, latest);
}

static uint32_t mode_clock_ms(void *ctx) { (void)ctx; return platform_ticks_ms(); }
static void mode_fade_out_quick(void *ctx) { (void)ctx; video_fade_out_quick(0, 0x100); }
static void mode_fade_in_quick(void *ctx, const uint8_t *rgb) { (void)ctx; video_fade_in_quick(0, 0x100, rgb); }

static void mode_open_notice(void *ctx)
{
    (void)ctx;
    if (!platform_open_releases_page()) port_log("update: the releases page could not be opened\n");
}

/* 004376ac (game_mode.c) */
int32_t glue_game_mode(void)
{
    const GameModeOps ops = { palette_clear, move_to, sound_load, load_art, music_feed, shape, palette_set, nothing,
                              in_rect, select_picture, blit_own, select_default, nothing, nothing, picture_shape,
                              nothing, flip, show, buttons, fade_in, nothing, menu_glue_banner_wait, deadline_set, sound, key,
                              ticks, activate, click, menu_glue_name_box, fade_out, pointer_away, menu_glue_back_to_menu, mode_network_found,
                              mode_network_game, menu_glue_yes_no, drain, off, music, deadline_passed, sound_free, bank_free, NULL,
                              menu_glue_extras_text_glue, menu_glue_extras_text_width_glue, mode_cover, glue_profile,
                              glue_plus_text_scaled, glue_plus_text_width_scaled, mode_footer, mode_open_notice,
                              mode_clock_ms, mode_fade_out_quick, mode_fade_in_quick };
    GameModeState st = { ea_screen(), (int16_t)pointer_active(), 0, menu_words.mode_menu, menu_words.intro_seen,
                         menu_words.single, menu_words.network_raced, menu_words.link_game, menu_words.link_lost,
                         menu_words.redraw, frontend_player_name };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    mode_state = &st;
    const int32_t result = game_mode_menu(resource_german(), &st, &ops);
    mode_state = NULL;
    pointer_bind_active(was);
    menu_words = words_of(&st);
    return result;
}

/* port: the name box over Game Mode's picture (004376ac's draw, the first option lit), as the player sees it when
 * choosing there; the screen is faded out again after it, whatever came back */
static int32_t name_asked_over_game_mode(void)
{
    uint8_t rgb[PALETTE_PUT_BYTES];
    palette_clear(NULL, rgb);
    const int32_t bank = load_art(NULL, "control/gamemode.qfs");
    const uint8_t *pal = shape(NULL, bank, "!pal");
    palette_set(NULL, rgb, 0, 0x100, pal + 0x10, (int16_t)pal[0]);
    select_picture(NULL, ea_screen());
    blit_own(NULL, shape(NULL, bank, "bgnd"));
    blit_own(NULL, shape(NULL, bank, "sinl"));
    select_default(NULL);
    blit_own(NULL, (const uint8_t *)(intptr_t)picture_shape(NULL, ea_screen()));
    flip(NULL);
    fade_in(NULL, 0, 0x100, rgb);
    const int32_t taken = menu_glue_name_box(NULL, frontend_player_name);
    fade_out(NULL, 0, 0x100);
    bank_free(NULL, bank);
    return taken;
}

/* port: the mode's file's `start`: the screen the front end opens on instead of Game Mode, what choosing it there does
 * (004376ac's choose) without showing Game Mode first. Network and Control Central go straight on when the
 * player's own config.dat was read (its name, 00524df0), else the name box comes first. Returns the front end's state (0 to race)
 * or -1 when Game Mode is to be shown (the default, or the choice was taken back: no network service, the name box
 * cancelled, the network screen's Back). */
int32_t glue_start_screen(int start)
{
    enum { SINGLE_RACE = 0 };
    if (start != START_NETWORK && start != START_CENTRAL) return -1;
    if (start == START_NETWORK && !menu_glue_network_found(NULL)) return -1;
    /* the CD's and the defaults' name is "Player", not one the player saved: with no saved config.dat, the box comes */
    if ((!config_name_saved() || frontend_player_name[0] == '\0') && !name_asked_over_game_mode()) return -1;
    menu_words.mode_menu = 1;
    menu_words.intro_seen = 1;
    if (start == START_CENTRAL) {
        menu_words.single = 0;
        menu_words.network_raced = 0;
        return SINGLE_RACE;
    }
    if (menu_glue_network_game(NULL) == 1) return -1;
    menu_words.network_raced = 1;
    return SINGLE_RACE;
}

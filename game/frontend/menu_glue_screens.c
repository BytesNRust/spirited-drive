/* The checked menus' showcase and network screens and the title (menu_glue.h; split from menu_glue.c by responsibility). */
#include "game/frontend/menu_glue_private.h"

/* 00408724's calls past the boxes': ctx the ShowcaseGlue */
static void clear_both(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}
static void sync(void *ctx) { (void)ctx; pointer_sync_from_os(); }
static void mark(void *ctx) { (void)ctx; pointer_mark(); }
static void blit_masked(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_masked(s); }
static void palette_write(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_set_palette(first, count, rgb); }
/* 00524738: paths.dat's showcase directory, from the data root as the original's (its names fit 0x28 bytes) */
static const char *show_dir(void) { return resource_german() ? "frontend/gshow/" : "frontend/show/"; }
static int32_t show_bank(void *ctx, const char *path, int32_t flags)
{
    (void)ctx, (void)flags;
    char full[1024];
    snprintf(full, sizeof full, "%s/%s", resource_root(), path);
    return ea_bank_load_path(full);
}
static void music_fade_out(void *ctx) { (void)ctx; menu_audio_music_fade_out(); }
static void music_end(void *ctx) { (void)ctx; menu_audio_music_end(); }
static void music_start(void *ctx, const char *name) { (void)ctx; menu_audio_music_start(name); }
static void voice_start(void *ctx, const char *name) { (void)ctx; menu_audio_track_music(name); }
static void voice_play(void *ctx) { (void)ctx; menu_audio_track_play(); }
static void voice_stop(void *ctx) { (void)ctx; menu_audio_track_stop(); }
static void voice_free(void *ctx) { (void)ctx; menu_audio_track_free(); }
static void movie(void *ctx, const char *name) { (void)ctx; glue_intro_movie(name); }  /* 004419c0 */
static void car_flags(void *ctx) { const ShowcaseGlue *in = ctx; in->car_flags(in->ctx); }

/* 00407290 / 00406008 (showcase_pages.c / showcase_figures.c) */
static void pages(void *ctx, const char *which)
{
    const ShowcaseGlue *const in = ctx;
    ShowcasePagesState st = { in->player_car, in->race_type, in->track, show_dir(), in->pointer_active,
                              in->buttons, ea_screen_slot() };
    const ShowcasePagesOps ops = {
        move_to, show_bank, music_feed, shape, palette_set, sync, mark, in_rect, select_picture, blit_own,
        blit_masked, select_default, picture_shape, blit_picture, flip, show, buttons, voice_start, fade_in, voice_play,
        deadline_set, sound, pointer_away, nothing, ticks, activate, key, click, car_flags, voice_stop, voice_free,
        fade_out, bank_free, off, music, deadline_passed, ctx };
    showcase_pages(&st, &ops, which);
}
static void figures(void *ctx, const char *which)
{
    const ShowcaseGlue *const in = ctx;
    const int16_t german = (int16_t)resource_german();
    ShowcaseFiguresState st = { in->player_car, in->race_type, in->track, &german, show_dir(),
                                in->pointer_active, in->buttons, ea_screen_slot() };
    const ShowcaseFiguresOps ops = {
        move_to, show_bank, music_feed, shape, palette_set, sync, mark, in_rect, select_picture, blit_own,
        blit_masked, select_default, picture_shape, blit_picture, flip, show, buttons, voice_start, fade_in, voice_play,
        deadline_set, sound, pointer_away, nothing, ticks, activate, key, click, car_flags, voice_stop, voice_free,
        fade_out, bank_free, off, music, deadline_passed, ctx };
    showcase_figures(&st, &ops, which);
}

/* 00408724 (showcase.c) */
void glue_showcase(const ShowcaseGlue *in)
{
    showcase_variants_reset();
    const int16_t movies = glue_movies();
    ShowcaseState st = { in->player_car, in->race_type, in->track, &movies, show_dir(),
                         in->pointer_active, in->buttons, ea_screen_slot(), glue_setup_plus(), in->leave_step, (int16_t)in->no_setup };
    const ShowcaseOps ops = {
        move_to, show_bank, music_feed, clear_both, shape, palette_set, palette_write, nothing, styles, sync, mark,
        in_rect, select_picture, blit_own, blit_masked, palette_write, select_default, picture_shape, blit_picture,
        flip, show, buttons, fade_in, deadline_set, sound, pointer_away, nothing, ticks, activate, key, click,
        bank_free, fade_out, pages, figures, music_fade_out, music_end, movie, music_start, car_flags, off, music,
        deadline_passed, (void *)in };
    showcase(&st, &ops);
}

/* the modem is out of scope (UDP only): the menu again */
int32_t menu_glue_back_to_menu(void *ctx) { (void)ctx; return 1; }

/* --- the network screens (00454b78, 00453ef4) on the link game (link_game.c) --- */

/* 00464a30: both pages black */
static void flush(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}
static const char *service_first(void *ctx) { (void)ctx; return link_service_first(); }
static const char *service_next(void *ctx) { (void)ctx; return link_service_next(); }
static int32_t fit_width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static void fit_ramp(void *ctx, int shadow) { (void)ctx; text_ramp(shadow); }
static void fit_draw(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }
static void fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t w)
{
    (void)ctx;
    static const TextFitOps ops = { fit_width, fit_ramp, fit_draw, NULL };
    text_fit_shadowed(text, x, y, w, &ops);
}
static void clip(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1) { (void)ctx; ea_clip(x0, y0, x1, y1); }
static RaceNetGames *games(void *ctx, int32_t timeout) { (void)ctx; return link_games(timeout); }
static RaceNetPlayers *players(void *ctx, const uint8_t *address) { (void)ctx; return link_players(address); }
static void players_draw(void *ctx, const uint8_t *address)
{
    (void)ctx;
    static const NetPlayersDrawOps ops = { players, style, glue_boxed, NULL };
    net_players_draw((int16_t)resource_german(), address, &ops);
}
static int32_t host(void *ctx, const char *name) { (void)ctx; return link_host(name); }
static int32_t connect(void *ctx, const uint8_t *address) { (void)ctx; return link_connect(address); }

static NetGameState *net_screen;  /* 00454b78's words while it runs (its bank 004c5570 for 00453ef4) */

/* 00453ef4 (net_host.c) */
static int32_t host_join(void *ctx)
{
    (void)ctx;
    static const NetHostOps ops = {
        picture_new, select_picture, picture_shape, grab, games, clip, move_to, ticks, in_rect, sound, deadline_set,
        shape, blit_own, style, width, fit, blit_masked, blit_at, players_draw, select_default, nothing, nothing, flip,
        show, deadline_passed, buttons, activate, music, key, click, off, picture_free, host, connect, NULL };
    link_game.view = net_screen->view;  /* 0052b316 is the menu's View word, the games listed and joined with it */
    NetHostState st = { ea_screen(), net_screen->pointer_active, net_screen->buttons, link_game.view, 0, 0,
                        net_screen->bank, frontend_player_name };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t r = net_host_join(&st, &ops);
    pointer_bind_active(was);
    net_screen->pointer_active = st.pointer_active, net_screen->buttons = st.buttons, link_game.view = st.view;
    net_screen->view = st.view;
    return r;
}

int32_t menu_glue_network_found(void *ctx) { (void)ctx; return link_services_found(); }
int32_t glue_network_found(void *ctx) { return menu_glue_network_found(ctx); }

/* 00454b78 (net_game.c): 0 to race, 1 the menu again, 2 the time ran out */
int32_t menu_glue_network_game(void *ctx)
{
    (void)ctx;
    static const NetGameOps ops = {
        move_to, flush, palette_clear, load_art, music_feed, shape, palette_set, palette_write, nothing, styles, sync,
        service_first, service_next, mark, in_rect, style, select_picture, blit_own, fit, palette_write,
        select_default, nothing, nothing, picture_shape, nothing, flip, show, fade_in, deadline_set, sound, ticks,
        buttons, activate, key, click, host_join, off, music, deadline_passed, fade_out, bank_free, nothing, NULL };
    NetGameState st = { ea_screen(), (int16_t)pointer_active(), 0, link_game.view, 0 };
    NetGameState *const outer = net_screen;
    net_screen = &st;
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t r = net_game_menu(&st, &ops);
    pointer_bind_active(was);
    net_screen = outer;
    link_game.view = st.view;
    return r;
}
int32_t glue_network_game(void *ctx) { return menu_glue_network_game(ctx); }

/* 004372e4 (title.c) */
void glue_title_screen(int32_t state)
{
    TitleState s = { 0, resource_german(), menu_words.link_game, menu_words.link_lost };
    DialogPointer p = { pointer_active(), 0, 0, 0, 0, 0 };
    const TitleOps ops = { deadline_set, screen_show, menu_glue_banner_wait, buttons, music, deadline_passed, key, fade_out,
                           NULL };
    port_log("mark:screen=title\n");   /* port: a headless walk measures it */
    const uint32_t start = timer_ticks();
    title_screen_show(state, &s, &p, &ops);
    port_log("mark:hold screen=title ticks=%u\n", (unsigned)(timer_ticks() - start));
    menu_words.link_game = (int16_t)s.link_game, menu_words.link_lost = (int16_t)s.link_lost;
}

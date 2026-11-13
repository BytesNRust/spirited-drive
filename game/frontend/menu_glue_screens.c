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

/* port: the variant box (showcase.h ShowcasePlus) over the car's race model. The screen's palette is the art's 256
 * colours, all in use, and has none of a livery's. So a preview paints with a few of the colours the art uses least:
 * they are the box's alone (the art's pixels in them go to the nearest other colour, `variant_settle`), each colour a
 * preview asks for takes one and is written into the screen's palette, and keeps it while the art stays (a change of a
 * slot in use would show on the picture still on screen). A colour past the slots falls back to the palette's nearest.
 * The box's own frame and arrows use the palette's nearest. */
enum { SHADOW = 4, BORDER = 2, LIT_COLOUR = 0xe8b800, DARK_COLOUR = 0x141414, SHADOW_COLOUR = 0x181c10, SLOTS = 4,
       TRIANGLE_HALF = 8, TRIANGLE_COLOUR = 0xbfac98, TRIANGLE_OUTLINE = 0x141414, TRIANGLE_INSET = 6, LABEL_INSET = 5,
       PALETTE_COLOURS = 0x100, TRANSPARENT = 0xff };

static struct {
    uint8_t slot[SLOTS];        /* the palette indices the box owns */
    int slots;
    int32_t taken_rgb[SLOTS];
    int taken;                  /* of those, the ones given colours since the art was drawn */
    int added;                  /* a colour was given a slot in this draw */
    uint8_t remap[PALETTE_COLOURS];   /* the art's pixel in index i is painted as remap[i] */
    int remaps;                 /* some index of the slots is in the art */
    uint8_t *palette;           /* the screen's palette (8 bits a channel) while the box draws */
} box;

static int variant_model(int car) { return race_head_to_head_model((int16_t)car); }

static int32_t colour_gap(const uint8_t *palette, int a, int b)
{
    int32_t gap = 0;
    for (int k = 0; k < 3; k++) {
        const int32_t d = palette[a * 3 + k] - palette[b * 3 + k];
        gap += d * d;
    }
    return gap;
}

/* the SLOTS palette indices the screen's pixels use least (the pointer's and the mask's never), and for each in use the
 * index of the nearest colour that is not one of them */
static void variant_free_colours(void *ctx, const uint8_t *palette)
{
    (void)ctx;
    int32_t w = 0, h = 0;
    const uint8_t *pixels = ea_picture_pixels(ea_screen(), &w, &h);
    uint32_t count[PALETTE_COLOURS] = { 0 };
    uint8_t pointer[PALETTE_COLOURS] = { 0 }, owned[PALETTE_COLOURS] = { 0 };
    box.slots = 0, box.taken = 0, box.remaps = 0;
    for (int i = 0; i < PALETTE_COLOURS; i++) box.remap[i] = (uint8_t)i;
    if (pixels == NULL) return;
    for (int32_t i = 0; i < w * h; i++) count[pixels[i]]++;
    pointer_colours_used(pointer);   /* the cursor is an overlay, not in the picture */
    pointer[TRANSPARENT] = 1;
    while (box.slots < SLOTS) {
        int best = -1;
        for (int i = 0; i < PALETTE_COLOURS; i++)
            if (!pointer[i] && !owned[i] && (best < 0 || count[i] < count[best])) best = i;
        if (best < 0) break;
        owned[best] = 1;
        box.slot[box.slots++] = (uint8_t)best;
    }
    for (int s = 0; s < box.slots; s++) {
        const int from = box.slot[s];
        if (count[from] == 0) continue;
        int nearest = -1;
        for (int i = 0; i < PALETTE_COLOURS; i++)
            if (!owned[i] && i != TRANSPARENT && (nearest < 0 || colour_gap(palette, from, i) < colour_gap(palette, from, nearest)))
                nearest = i;
        box.remap[from] = (uint8_t)nearest, box.remaps = 1;
    }
}

/* the art's pixels out of the box's colours; the box draws after this */
static void variant_settle(void *ctx)
{
    (void)ctx;
    int32_t w = 0, h = 0;
    uint8_t *pixels = ea_picture_pixels(ea_screen(), &w, &h);
    if (pixels == NULL || !box.remaps) return;
    for (int32_t i = 0; i < w * h; i++) pixels[i] = box.remap[pixels[i]];
}

/* the index to paint `rgb` with: one of the box's colours set to it, else the palette's nearest */
static int32_t variant_colour(int32_t rgb)
{
    for (int i = 0; i < box.taken; i++)
        if (box.taken_rgb[i] == rgb) return box.slot[i];
    if (box.taken >= box.slots || box.palette == NULL) return ea_colour(rgb);
    const int slot = box.slot[box.taken];
    box.taken_rgb[box.taken++] = rgb, box.added = 1;
    box.palette[slot * 3] = (uint8_t)(rgb >> 16), box.palette[slot * 3 + 1] = (uint8_t)(rgb >> 8);
    box.palette[slot * 3 + 2] = (uint8_t)rgb;
    return slot;
}

static void fill_rows(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t colour)
{
    for (int32_t y = y0; y <= y1; y++) ea_frame(x0, y, x1, y, colour);
}

/* the box's frame and arrows, in the palette's own colours: the photos' border, the gold, the arrows' beige */
static void fill_chrome(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t rgb)
{
    fill_rows(x0, y0, x1, y1, ea_colour(rgb));
}

/* a preview's colour: exact, in a free palette slot */
static void variant_fill(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t rgb)
{
    (void)ctx;
    fill_rows(x0, y0, x1, y1, variant_colour(rgb));
}

static int variant_offered(void *ctx, int car)
{
    (void)ctx;
    return car_variant_choices(showcase_variants, showcase_variant_count, variant_model(car)) != 0;
}

static void variant_step(void *ctx, int car, int step, int wrap)
{
    (void)ctx;
    car_variant_step(showcase_variants, showcase_variant_count, variant_model(car), step, wrap);
}

static int variant_can_step(void *ctx, int car, int step)
{
    (void)ctx;
    return car_variant_can_step(showcase_variants, showcase_variant_count, variant_model(car), step);
}

/* a paging triangle as the screen's "Next car" ones: 9 wide and 17 tall, its point to the side it pages to */
static void variant_triangle(int32_t x, int32_t y, int left)
{
    /* a dark outline one pixel round it first, so the beige shows on the light swatches too */
    for (int32_t dy = -TRIANGLE_HALF - 1; dy <= TRIANGLE_HALF + 1; dy++) {
        const int32_t cut = dy < 0 ? -dy : dy;
        if (left) fill_chrome(x - 1 + cut, y + dy, x + TRIANGLE_HALF + 1, y + dy, TRIANGLE_OUTLINE);
        else fill_chrome(x - TRIANGLE_HALF - 1, y + dy, x + 1 - cut, y + dy, TRIANGLE_OUTLINE);
    }
    for (int32_t dy = -TRIANGLE_HALF; dy <= TRIANGLE_HALF; dy++) {
        const int32_t cut = dy < 0 ? -dy : dy;
        if (left) fill_chrome(x + cut, y + dy, x + TRIANGLE_HALF, y + dy, TRIANGLE_COLOUR);
        else fill_chrome(x - TRIANGLE_HALF, y + dy, x - cut, y + dy, TRIANGLE_COLOUR);
    }
}

/* the photos' look: a dark border and a drop shadow (none below: the box stands on the General photo), the border gold
 * when the box is the selection */
static int variant_draw(void *ctx, int car, int lit, uint8_t *palette)
{
    (void)ctx;
    box.palette = palette, box.added = 0;
    fill_chrome(VARIANT_RIGHT + 1, VARIANT_TOP + SHADOW, VARIANT_RIGHT + SHADOW, VARIANT_BOTTOM, SHADOW_COLOUR);
    fill_chrome(VARIANT_LEFT, VARIANT_TOP, VARIANT_RIGHT, VARIANT_BOTTOM, lit ? LIT_COLOUR : DARK_COLOUR);
    const CarVariantCanvas canvas = { NULL, variant_fill, VARIANT_LEFT + BORDER, VARIANT_TOP + BORDER,
                                      VARIANT_RIGHT - BORDER, VARIANT_BOTTOM - BORDER };
    const int model = variant_model(car);
    car_variant_draw(showcase_variants, showcase_variant_count, model, &canvas);
    const int32_t middle = (VARIANT_TOP + VARIANT_BOTTOM) / 2;
    if (car_variant_can_step(showcase_variants, showcase_variant_count, model, -1))
        variant_triangle(VARIANT_LEFT + BORDER + TRIANGLE_INSET, middle, 1);
    if (car_variant_can_step(showcase_variants, showcase_variant_count, model, 1))
        variant_triangle(VARIANT_RIGHT - BORDER - TRIANGLE_INSET, middle, 0);
    text_select(lit ? TEXT_YELLOW : TEXT_CREAM);   /* the photos' captions: bottom left inside the frame */
    text_draw_shadowed("Color", VARIANT_LEFT + BORDER + LABEL_INSET, VARIANT_BOTTOM - BORDER - text_line_height() - 2);
    box.palette = NULL;
    return box.added;
}

/* 00408724 (showcase.c) */
void glue_showcase(const ShowcaseGlue *in)
{
    static const ShowcasePlus plus = { variant_offered, variant_step, variant_can_step, variant_free_colours, variant_settle, variant_draw,
                                       NULL };
    showcase_variants_reset();
    const int16_t movies = glue_movies();
    ShowcaseState st = { in->player_car, in->race_type, in->track, &movies, show_dir(),
                         in->pointer_active, in->buttons, ea_screen_slot(), &plus };
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

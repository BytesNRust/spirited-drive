#include "game/frontend/showcase.h"

#include "game/frontend/palette_put.h"
#include "game/sys/port_log.h"

#include <stdio.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { HISTORY, VIDEO, GENERAL, PERFORMANCE, ENGINE, NEXT, PREVIOUS, DONE, NONE, ITEMS = NONE,
       VARIANT = NONE + 1 };   /* port: the variant box, an item with no art of the bank's */
enum { PEEK_AT = 70, PEEK_LENGTH = 100, PEEK_PIXELS = 26, SLIDE_TICKS = 15,   /* port: 100 ticks a second */
       MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, ALL = 0x100, BOXES = 8, TOURNAMENT = 3 };

/* 00405f4c..00405fb7, copied to the stack: the arrows' next item and the pointer's place, by item (NONE too) */
static const int16_t UP_OF[VARIANT + 1] = { 3, 3, 3, 5, 2, 1, 4, 4, 7, VARIANT };
static const int16_t DOWN_OF[VARIANT + 1] = { 5, 4, 4, 1, 6, 3, 2, 2, 7, GENERAL };
static const int16_t LEFT_OF[VARIANT + 1] = { 1, 2, 0, 2, 0, 6, 7, 5, 7, VARIANT };
static const int16_t RIGHT_OF[VARIANT + 1] = { 4, 0, 1, 0, 1, 7, 5, 6, 7, PERFORMANCE };
static const int16_t POINTER_X[VARIANT + 1] = { 533, 376, 217, 419, 208, 382, 230, 35, 320, 134 };
static const int16_t POINTER_Y[VARIANT + 1] = { 306, 265, 222, 137, 369, 450, 450, 445, 240, 131 };
/* 00405fb8: the cars' file names, by 00524e8b */
static const char *const CARS[SHOWCASE_CARS] = { "rx7", "nsx", "supra", "911", "vette", "viper", "512tr", "diablo" };
/* the boxes 00463f80 is asked about, in turn, and the item each picks */
static const int16_t BOX_LEFT[BOXES] = { 0x134, 0x1c3, 0x86, 0x134, 0x52, SHOWCASE_PREVIOUS_LEFT, SHOWCASE_NEXT_LEFT, SHOWCASE_DONE_LEFT };
static const int16_t BOX_RIGHT[BOXES] = { 0x1bc, 0x267, 300, 0x212, 0x14f, SHOWCASE_PREVIOUS_RIGHT, SHOWCASE_NEXT_RIGHT, SHOWCASE_DONE_RIGHT };
static const int16_t BOX_TOP[BOXES] = { 0xd1, 0xf6, 0xa4, 0x4e, 0x146, SHOWCASE_BOTTOM_TOP, SHOWCASE_BOTTOM_TOP, SHOWCASE_BOTTOM_TOP };
static const int16_t BOX_BOTTOM[BOXES] = { 0x141, 0x16e, 0x119, 0xcb, 0x19c, SHOWCASE_BOTTOM_BOTTOM, SHOWCASE_BOTTOM_BOTTOM,
                                           SHOWCASE_BOTTOM_BOTTOM };
static const int16_t BOX_ITEM[BOXES] = { VIDEO, HISTORY, GENERAL, PERFORMANCE, ENGINE, PREVIOUS, NEXT, DONE };
static const char *const LIT[ITEMS] = { "hisl", "vidl", "genl", "perl", "engl", "nexl", "prel", "donl" };
static const char *const DARK[ITEMS] = { "hisd", "vidd", "gend", "perd", "engd", "nexd", "pred", "dond" };

typedef struct Show {
    int32_t bank;           /* esi */
    int16_t selected;       /* esp + 0x6cc */
    int16_t previous;       /* esp + 0x6e0 */
    int changed;            /* esp + 0x6c8 */
    int redraw;             /* esp + 0x6d8 */
    int fade;               /* esp + 0x6dc: the palette faded in on the next draw */
    int armed;              /* esp + 0x6c4: the keys taken */
    int32_t rest;           /* esp + 0x6d4: the ticks summed while a key is held */
    int32_t pending;        /* port: the ticks since the last time they were taken (rest's sum, or reset by a key) */
    int32_t clock;          /* port: the ticks since the page opened (the Video tile's second face: its slide and its peek) */
    int face;               /* port: the Video tile's face: 0 the video, 1 RECORDS */
    int slide;              /* port: the strip's place of the face, 0 .. SHOWCASE_TILE_W, moving to face * SHOWCASE_TILE_W */
    int peek;               /* port: 0 not yet, 1 under way, 2 done (once a page) */
    int32_t peek_from;      /* port: the clock when the peek began */
    int released;           /* esp + 0x6d0: the buttons seen up */
    int16_t quit;           /* esp + 0x6c0 */
    char name[SHOWCASE_NAME];          /* esp + 0x600 */
    uint8_t black[SHOWCASE_PALETTE];   /* esp + 0x300 */
    uint8_t rgb[SHOWCASE_PALETTE];     /* esp + 0 */
} Show;

static void move_to(int16_t item, const ShowcaseOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

/* the car's art and its palette */
static void load(Show *s, const ShowcaseState *st, const ShowcaseOps *o)
{
    snprintf(s->name, sizeof s->name, "%s%s.qfs", st->car_dir, CARS[*st->player_car]);
    s->bank = o->load_bank(o->ctx, s->name, 0);
    o->music_feed(o->ctx);
}

static void palette(Show *s, const ShowcaseOps *o)
{
    const uint8_t *pal = o->shape(o->ctx, s->bank, "!pal");
    o->palette_set(o->ctx, s->rgb, 0, ALL, pal + 0x10, pal[0]);
}

/* port: the SETUP tile is there when the screen has one */
static int has_box(const ShowcaseState *st)
{
    return st->plus != NULL;
}

/* port: the Video tile's second face is there when the screen has one */
static int has_records(const ShowcaseState *st)
{
    return st->plus != NULL && st->plus->tile != NULL && st->plus->records_open != NULL;
}

/* port: the ticks since the last call, added to the page's clock and to what is waiting to be taken */
static void tick_poll(Show *s, const ShowcaseOps *o)
{
    const int32_t d = o->ticks(o->ctx);
    s->clock += d;
    s->pending += d;
}

/* port: the strip's place now: the face's slide, and the peek's bump (a parabola out and back, once a page) */
static int tile_shift(const Show *s)
{
    int shift = s->slide;
    if (s->peek == 1) {
        const int32_t t = s->clock - s->peek_from;
        shift += (int)(4 * PEEK_PIXELS * t * (PEEK_LENGTH - t) / ((int32_t)PEEK_LENGTH * PEEK_LENGTH));
    }
    return shift;
}

/* port: the arrows in the tile: a left one on the records, a right one on the video from the first time it is lit (owner 2026-11-19: not only once the peek has shown there is more) */
static int tile_arrows(const Show *s)
{
    return s->face ? 1 : 2;
}

/* the first box the pointer is in becomes the selection, else none */
static void follow(Show *s, const ShowcaseState *st, const ShowcaseOps *o)
{
    int16_t item = NONE;
    if (has_box(st) && o->pointer_in_rect(o->ctx, VARIANT_LEFT, VARIANT_RIGHT, VARIANT_TOP, VARIANT_BOTTOM)) item = VARIANT;
    for (int i = 0; item == NONE && i < BOXES; i++) {
        if (o->pointer_in_rect(o->ctx, BOX_LEFT[i], BOX_RIGHT[i], BOX_TOP[i], BOX_BOTTOM[i])) {
            item = BOX_ITEM[i];
            break;
        }
    }
    if (s->selected != item) {
        s->previous = s->selected;
        s->selected = item;
        s->changed = 1;
    }
}

/* an item's art: the first five masked, the rest at their own place; the video's lit only when offered */
static void item_art(Show *s, int16_t item, const char *const *tags, const ShowcaseState *st,
                     const ShowcaseOps *o)
{
    if (item == VARIANT) {
        if (!has_box(st)) return;
        st->plus->draw(st->plus->ctx, *st->player_car, tags == LIT, s->rgb);
        return;
    }
    if (item < 0 || item > DONE) return;
    if (item == VIDEO && has_records(st)) {   /* port: the original's art, then the two faces over it when there is something to show */
        const int lit = tags == LIT, shift = tile_shift(s), arrows = lit ? tile_arrows(s) : 0;
        const int changed = s->face || s->slide != 0 || shift != 0 || arrows != 0;
        if (!lit) o->blit_masked(o->ctx, o->shape(o->ctx, s->bank, tags[item]));
        if (lit && *st->intro_movies != 0) o->blit_masked(o->ctx, o->shape(o->ctx, s->bank, tags[item]));
        if (changed) st->plus->tile(st->plus->ctx, *st->player_car, lit, shift, arrows, s->rgb);
        return;
    }
    if (tags == LIT && item == VIDEO && *st->intro_movies == 0) return;
    const uint8_t *shape = o->shape(o->ctx, s->bank, tags[item]);
    if (item <= ENGINE) o->blit_masked(o->ctx, shape);
    else o->blit_own(o->ctx, shape);
}

static void draw(Show *s, const ShowcaseState *st, const ShowcaseOps *o)
{
    o->select(o->ctx, *st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, s->bank, "bgnd"));
    if (s->selected != VARIANT) item_art(s, s->selected, LIT, st, o);
    if (has_records(st) && s->selected != VIDEO && (s->face || s->slide != 0 || tile_shift(s) != 0)) item_art(s, VIDEO, DARK, st, o);   /* else the page's own */
    item_art(s, VARIANT, s->selected == VARIANT ? LIT : DARK, st, o);
    if (s->fade) o->palette_show(o->ctx, 0, ALL, s->black);
    o->select_default(o->ctx);
    o->blit_word(o->ctx, o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    o->select(o->ctx, *st->screen);
    o->pointer_show(o->ctx);
    if (s->fade) {
        o->pointer_mark(o->ctx);
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        o->fade_in(o->ctx, 0, ALL, s->rgb);
        o->inverse_build(o->ctx);
        o->text_styles(o->ctx);
        s->fade = 0;
    }
    o->deadline_set(o->ctx, WAIT);
    s->redraw = 0;
    s->changed = 0;
}

static void mark_change(Show *s, const ShowcaseState *st, const ShowcaseOps *o)
{
    s->redraw = 1;
    o->select(o->ctx, *st->screen);
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    if (s->previous != VARIANT) item_art(s, s->previous, DARK, st, o);   /* the box is the redraw's: it sets a palette */
    if (s->selected != VARIANT) item_art(s, s->selected, LIT, st, o);
    s->changed = 0;
}

/* Next and Previous: a tournament keeps to its track's cars (0..2, 3..5 or 6..7), else all eight in a ring */
static void step_car(int up, const ShowcaseState *st)
{
    int16_t car = (int16_t)(*st->player_car + (up ? 1 : -1));
    const int16_t track = *st->track;
    *st->player_car = car;
    if (*st->race_type != TOURNAMENT) {
        if (up) *st->player_car = (int16_t)(car % SHOWCASE_CARS);
        else if (car < 0) *st->player_car = 7;
    } else if (track == 0 || track == 3) {
        if (up && car > 2) *st->player_car = 0;
        else if (!up && car < 0) *st->player_car = 2;
    } else if (track == 1 || track == 4 || track == 5) {
        if (up && car > 5) *st->player_car = 3;
        else if (!up && car < 3) *st->player_car = 5;
    } else {
        if (up && car > 7) *st->player_car = 6;
        else if (!up && car < 6) *st->player_car = 7;
    }
}

/* back from a page: the car's art again */
static void page(Show *s, const ShowcaseState *st, const ShowcaseOps *o,
                 void (*screen)(void *ctx, const char *which), const char *which)
{
    o->free(o->ctx, s->bank);
    o->fade_out(o->ctx, 0, ALL);
    o->music_feed(o->ctx);
    screen(o->ctx, which);
    o->music_feed(o->ctx);
    load(s, st, o);
    palette(s, o);
    move_to(s->selected, o);
    s->redraw = 1;
    s->fade = 1;
}

/* port: the Video tile's other face (once the page has done or been spared its peek) */
static void flip_face(Show *s, const ShowcaseOps *o)
{
    s->face ^= 1;
    if (s->peek != 2) s->peek = 2;
    o->sound(o->ctx, MOVED);
    s->redraw = 1;
}

/* port: a click on the arrow inside the tile (30 pixels at its edge) takes the other face; 1 when it did */
static int tile_click(Show *s, const ShowcaseOps *o)
{
    enum { ZONE = 30 };
    const int left = s->face && o->pointer_in_rect(o->ctx, SHOWCASE_TILE_X, SHOWCASE_TILE_X + ZONE, SHOWCASE_TILE_Y, SHOWCASE_TILE_Y + SHOWCASE_TILE_H);
    const int right = !s->face &&
                      o->pointer_in_rect(o->ctx, SHOWCASE_TILE_X + SHOWCASE_TILE_W - ZONE, SHOWCASE_TILE_X + SHOWCASE_TILE_W, SHOWCASE_TILE_Y, SHOWCASE_TILE_Y + SHOWCASE_TILE_H);
    if (left || right) flip_face(s, o);
    return left || right;
}

/* port: Enter, Space or a click on the tile open the panel; the screen is drawn again once it is closed */
static void take_variant(Show *s, const ShowcaseState *st)
{
    st->plus->open(st->plus->ctx, *st->player_car, s->rgb);
    s->redraw = 1;
}

static void take(Show *s, const ShowcaseState *st, const ShowcaseOps *o, int by_click)
{
    switch (s->selected) {
    case HISTORY: page(s, st, o, o->pages, "his"); break;
    case GENERAL: page(s, st, o, o->pages, "gen"); break;
    case PERFORMANCE: page(s, st, o, o->figures, "per"); break;
    case ENGINE: page(s, st, o, o->figures, "mec"); break;
    case VIDEO:
        if (has_records(st)) {
            if (by_click && tile_click(s, o)) break;
            if (s->face) {   /* port: RECORDS' screen, a screen of its own: out to black, in again on the page as with the history's */
                o->free(o->ctx, s->bank);
                o->fade_out(o->ctx, 0, ALL);
                o->music_feed(o->ctx);
                st->plus->records_open(st->plus->ctx, *st->player_car, s->rgb);
                o->music_feed(o->ctx);
                load(s, st, o);
                palette(s, o);
                move_to(s->selected, o);
                s->redraw = 1;
                s->fade = 1;
                break;
            }
        }
        if (*st->intro_movies == 0) break;
        o->music_fade_out(o->ctx);
        o->fade_out(o->ctx, 0, ALL);
        o->music_end(o->ctx);
        snprintf(s->name, sizeof s->name, "%s.tgv", CARS[*st->player_car]);
        o->movie(o->ctx, s->name);
        o->music_start(o->ctx, "showcase.asf");
        o->music_feed(o->ctx);
        s->redraw = 1;
        s->fade = 1;
        move_to(s->selected, o);
        break;
    case NEXT:
    case PREVIOUS:
        if (st->leave_step != NULL) {   /* port (dep59 section 4): the carousel takes the next car's page */
            *st->leave_step = s->selected == NEXT ? 1 : -1;
            s->quit = 1;
            break;
        }
        step_car(s->selected == NEXT, st);
        o->car_flags(o->ctx);
        o->fade_out(o->ctx, 0, ALL);
        o->music_feed(o->ctx);
        o->free(o->ctx, s->bank);
        load(s, st, o);
        palette(s, o);
        s->redraw = 1;
        s->fade = 1;
        break;
    case DONE: s->quit = 1; break;
    case VARIANT: take_variant(s, st); break;
    default: break;
    }
}


/* the arrows: the pointer off, the item the table names, the pointer onto it */
static void arrow(Show *s, int16_t to, const ShowcaseOps *o)
{
    o->pointer_off(o->ctx);
    s->previous = s->selected;
    s->selected = to;
    s->changed = 1;
    move_to(s->selected, o);
}

static void keys(Show *s, const ShowcaseState *st, const ShowcaseOps *o)
{
    if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
    if (*st->buttons == 0) s->released = 1;
    o->pointer_activate_if_moved(o->ctx);
    const uint16_t key = (uint16_t)o->key(o->ctx);
    if (key == SPACE || key == ENTER || (*st->buttons == LEFT_BUTTON && s->released)) {
        const int armed = s->armed;
        o->pointer_click(o->ctx);
        if (armed) {
            o->sound(o->ctx, CHOSEN);
            take(s, st, o, key != SPACE && key != ENTER);
        }
        tick_poll(s, o);
        s->pending = 0;
        s->released = 0;
        s->armed = 0;
        s->rest = 0;
    } else if (key != 0 && s->armed) {
        switch (key) {
        case ESCAPE: o->sound(o->ctx, CHOSEN), s->quit = 1; break;
        case UP: arrow(s, s->selected == GENERAL && has_box(st) ? VARIANT : UP_OF[s->selected], o); break;
        case DOWN: arrow(s, DOWN_OF[s->selected], o); break;
        case LEFT:
        case RIGHT:
            if (has_records(st) && s->selected == VIDEO && (key == RIGHT) != (s->face != 0)) flip_face(s, o);   /* port: the tile's faces */
            else arrow(s, (key == LEFT ? LEFT_OF : RIGHT_OF)[s->selected], o);
            break;
        default: break;
        }
        tick_poll(s, o);
        s->pending = 0;
        s->armed = 0;
        s->rest = 0;
    } else if (key == 0) {
        s->armed = 1;
    }
}

/* port: the Video tile's slide to its face and its peek, by the page's clock; a frame of either is a redraw */
static void animate(Show *s, const ShowcaseState *st, int32_t delta)
{
    if (!has_records(st) || s->fade) return;
    const int goal = s->face * SHOWCASE_TILE_W;
    if (s->slide != goal) {
        int step = (int)((int32_t)SHOWCASE_TILE_W * delta / SLIDE_TICKS);
        if (step < 1) step = 1;
        s->slide = s->slide < goal ? (s->slide + step > goal ? goal : s->slide + step) : (s->slide - step < goal ? goal : s->slide - step);
        s->redraw = 1;
    }
    if (s->peek == 0 && !s->face && s->clock >= PEEK_AT) s->peek = 1, s->peek_from = s->clock, port_log("mark:tile_peek=start\n");
    if (s->peek == 1) {
        if (s->clock - s->peek_from >= PEEK_LENGTH) s->peek = 2;
        s->redraw = 1;
    }
}

/* 00408724 */
int32_t showcase(ShowcaseState *st, const ShowcaseOps *o)
{
    port_log("mark:screen=showcase\n");   /* port: the headless walk (tools/menu_walk.sh) waits for it */
    /* port (dep59 section 4, owner 2026-11-17): a carousel page entered by a step starts on that step's item, so the player keeps paging
     * without moving the pointer; the first entry (0) on the video as the game's. Done and Escape hand back 0 */
    const int16_t entry = st->leave_step != NULL ? *st->leave_step : 0;
    const int16_t first = entry > 0 ? NEXT : entry < 0 ? PREVIOUS : VIDEO;
    if (st->leave_step != NULL) *st->leave_step = 0;
    Show s = { .selected = first, .previous = first, .redraw = 1, .fade = 1, .armed = 1, .released = 1 };
    if (*st->pointer_active == 0) move_to(first, o);
    load(&s, st, o);
    o->flush(o->ctx);
    palette_put_clear(s.black);
    palette(&s, o);
    o->palette_write(o->ctx, 0, ALL, s.rgb);
    o->inverse_build(o->ctx);
    o->text_styles(o->ctx);
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->pointer_mark(o->ctx);
    tick_poll(&s, o);   /* port: the page's clock starts here */
    s.clock = 0, s.pending = 0;
    while (!s.quit) {
        follow(&s, st, o);
        const int32_t before = s.clock;
        tick_poll(&s, o);
        animate(&s, st, s.clock - before);
        if (s.redraw) draw(&s, st, o);
        if (s.changed) mark_change(&s, st, o);
        if (!s.armed) {
            s.rest += s.pending;
            s.pending = 0;
            if ((int16_t)s.rest > REST) s.armed = 1;
        }
        keys(&s, st, o);
        const int fade = s.fade;
        o->music(o->ctx);
        if (!fade && (o->deadline_passed(o->ctx) || (*st->buttons == RIGHT_BUTTON && s.released))) {
            o->sound(o->ctx, CHOSEN);
            s.quit = 1;
        }
    }
    const int own_music = st->leave_step == NULL;   /* port (dep59 section 4): in the carousel the music is the carousel's, ended once by it */
    if (own_music) o->music_fade_out(o->ctx);
    o->fade_out(o->ctx, 0, ALL);
    if (own_music) o->music_end(o->ctx);
    o->free(o->ctx, s.bank);
    o->pointer_mark(o->ctx);
    return s.quit;
}

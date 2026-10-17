#include "game/frontend/showcase.h"

#include "game/frontend/palette_put.h"

#include <stdio.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { HISTORY, VIDEO, GENERAL, PERFORMANCE, ENGINE, NEXT, PREVIOUS, DONE, NONE, ITEMS = NONE };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, ALL = 0x100, BOXES = 8, TOURNAMENT = 3 };

/* 00405f4c..00405fb7, copied to the stack: the arrows' next item and the pointer's place, by item (NONE too) */
static const int16_t UP_OF[NONE + 1] = { 3, 3, 3, 5, 2, 1, 4, 4, 7 };
static const int16_t DOWN_OF[NONE + 1] = { 5, 4, 4, 1, 6, 3, 2, 2, 7 };
static const int16_t LEFT_OF[NONE + 1] = { 1, 2, 0, 2, 0, 6, 7, 5, 7 };
static const int16_t RIGHT_OF[NONE + 1] = { 4, 0, 1, 0, 1, 7, 5, 6, 7 };
static const int16_t POINTER_X[NONE + 1] = { 533, 376, 217, 419, 208, 382, 230, 35, 320 };
static const int16_t POINTER_Y[NONE + 1] = { 306, 265, 222, 137, 369, 450, 450, 445, 240 };
/* 00405fb8: the cars' file names, by 00524e8b */
static const char *const CARS[SHOWCASE_CARS] = { "rx7", "nsx", "supra", "911", "vette", "viper", "512tr", "diablo" };
/* the boxes 00463f80 is asked about, in turn, and the item each picks */
static const int16_t BOX_LEFT[BOXES] = { 0x134, 0x1c3, 0x86, 0x134, 0x52, 200, 0x168, 0 };
static const int16_t BOX_RIGHT[BOXES] = { 0x1bc, 0x267, 300, 0x212, 0x14f, 0x104, 0x1a4, 0x82 };
static const int16_t BOX_TOP[BOXES] = { 0xd1, 0xf6, 0xa4, 0x4e, 0x146, 0x1ae, 0x1ae, 0x1ae };
static const int16_t BOX_BOTTOM[BOXES] = { 0x141, 0x16e, 0x119, 0xcb, 0x19c, 0x1e0, 0x1e0, 0x1e0 };
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

/* the first box the pointer is in becomes the selection, else none */
static void follow(Show *s, const ShowcaseOps *o)
{
    int16_t item = NONE;
    for (int i = 0; i < BOXES; i++) {
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
static void item_art(const Show *s, int16_t item, const char *const *tags, const ShowcaseState *st,
                     const ShowcaseOps *o)
{
    if (item < 0 || item > DONE) return;
    if (tags == LIT && item == VIDEO && *st->intro_movies == 0) return;
    const uint8_t *shape = o->shape(o->ctx, s->bank, tags[item]);
    if (item <= ENGINE) o->blit_masked(o->ctx, shape);
    else o->blit_own(o->ctx, shape);
}

static void draw(Show *s, const ShowcaseState *st, const ShowcaseOps *o)
{
    o->select(o->ctx, *st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, s->bank, "bgnd"));
    item_art(s, s->selected, LIT, st, o);
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
    item_art(s, s->previous, DARK, st, o);
    item_art(s, s->selected, LIT, st, o);
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

static void take(Show *s, const ShowcaseState *st, const ShowcaseOps *o)
{
    switch (s->selected) {
    case HISTORY: page(s, st, o, o->pages, "his"); break;
    case GENERAL: page(s, st, o, o->pages, "gen"); break;
    case PERFORMANCE: page(s, st, o, o->figures, "per"); break;
    case ENGINE: page(s, st, o, o->figures, "mec"); break;
    case VIDEO:
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
    default: break;
    }
}

/* the arrows: the pointer off, the item the table names, the pointer onto it */
static void arrow(Show *s, const int16_t *next, const ShowcaseOps *o)
{
    o->pointer_off(o->ctx);
    s->previous = s->selected;
    s->selected = next[s->selected];
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
            take(s, st, o);
        }
        o->ticks(o->ctx);
        s->released = 0;
        s->armed = 0;
        s->rest = 0;
    } else if (key != 0 && s->armed) {
        switch (key) {
        case ESCAPE: o->sound(o->ctx, CHOSEN), s->quit = 1; break;
        case UP: arrow(s, UP_OF, o); break;
        case LEFT: arrow(s, LEFT_OF, o); break;
        case RIGHT: arrow(s, RIGHT_OF, o); break;
        case DOWN: arrow(s, DOWN_OF, o); break;
        default: break;
        }
        o->ticks(o->ctx);
        s->armed = 0;
        s->rest = 0;
    } else if (key == 0) {
        s->armed = 1;
    }
}

/* 00408724 */
int32_t showcase(ShowcaseState *st, const ShowcaseOps *o)
{
    Show s = { .selected = VIDEO, .previous = VIDEO, .redraw = 1, .fade = 1, .armed = 1, .released = 1 };
    if (*st->pointer_active == 0) move_to(VIDEO, o);
    load(&s, st, o);
    o->flush(o->ctx);
    palette_put_clear(s.black);
    palette(&s, o);
    o->palette_write(o->ctx, 0, ALL, s.rgb);
    o->inverse_build(o->ctx);
    o->text_styles(o->ctx);
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->pointer_mark(o->ctx);
    while (!s.quit) {
        follow(&s, o);
        if (s.redraw) draw(&s, st, o);
        if (s.changed) mark_change(&s, st, o);
        if (!s.armed) {
            s.rest += o->ticks(o->ctx);
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
    o->music_fade_out(o->ctx);
    o->fade_out(o->ctx, 0, ALL);
    o->music_end(o->ctx);
    o->free(o->ctx, s.bank);
    o->pointer_mark(o->ctx);
    return s.quit;
}

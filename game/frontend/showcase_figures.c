#include "game/frontend/showcase_figures.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { BLOCK, DONE, PREVIOUS, NEXT, NONE };
enum { NO_ARROW, BACK_ARROW, ON_ARROW };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, ALL = 0x100, PAGES = 3, CARS = 8,
       TOURNAMENT = 3 };

/* 00405e90..00405efb, copied to the stack: the pointer's place by item, Up / Down's next item (the same) */
static const int16_t POINTER_X[NONE + 1] = { 315, 35, 230, 382, 320 };
static const int16_t POINTER_Y[NONE + 1] = { 218, 445, 450, 450, 240 };
static const int16_t UP_OF[NONE + 1] = { 2, 0, 0, 0, 1 };
static const int16_t DOWN_OF[NONE + 1] = { 2, 0, 0, 0, 1 };
/* 00405eb8: the cars' short names, by 00524e8b */
static const char *const CAR[CARS] = { "rx7", "nsx", "sup", "911", "zr1", "vip", "512", "dia" };
/* 00405ee4: "mec"'s pages; 00405ed8 / 00405ef0: "per"'s, two shapes each (the first alone when it goes dark) */
static const char *const MEC_PAGE[PAGES] = { "engn", "driv", "chas" };
static const char *const PER_PAGE[PAGES] = { "acc1", "bra1", "han1" };
static const char *const PER_PAGE_2[PAGES] = { "accl", "brak", "hand" };
/* per kind ("per", "mec"): the block's area, its arrows' boxes (back, on) and Done's right edge */
static const int16_t AREA[2][4] = { { 0x22, 0x254, 0x68, 0x14d }, { 0xc, 0x268, 0x61, 0x191 } };
static const int16_t BACK_BOX[2][4] = { { 0x23, 0x3e, 0xc8, 0xeb }, { 0xe, 0x2d, 0xe1, 0x10e } };
static const int16_t ON_BOX[2][4] = { { 0x235, 0x253, 0xc8, 0xeb }, { 0x244, 0x266, 0xe1, 0x10e } };
static const int16_t DONE_RIGHT[2] = { 0x69, 0x82 };
static const int16_t BOTTOM_LEFT[2] = { 200, 0x168 };
static const int16_t BOTTOM_RIGHT[2] = { 0x104, 0x1a4 };
static const char *const ARROW_LIT[3] = { "frml", "left", "righ" };
static const char *const BUTTON_LIT[3] = { "donl", "prel", "nexl" };
static const char *const BUTTON_DARK[3] = { "dond", "pred", "nexd" };

typedef struct Figures {
    int32_t bank;           /* esi */
    int mec;                /* which is "mec" (strcmp each time in the original) */
    int per;                /* which is "per" */
    int16_t selected;       /* esp + 0x3b8 */
    int16_t previous;       /* esp + 0x3b4 */
    int16_t arrow;          /* esp + 0x3b0 */
    int16_t page;           /* esp + 0x3c8 */
    int changed;            /* esp + 0x3c4 */
    int redraw;             /* esp + 0x3c0 */
    int fade;               /* esp + 0x3a8 */
    int armed;              /* esp + 0x3ac */
    int32_t rest;           /* esp + 0x3a4 */
    int released;           /* esp + 0x3bc */
    int16_t quit;           /* esp + 0x3a0 */
    char name[SHOWCASE_FIGURES_NAME];          /* esp + 0x300 */
    uint8_t rgb[SHOWCASE_FIGURES_PALETTE];     /* esp + 0 */
} Figures;

static int in_box(const int16_t box[4], const ShowcaseFiguresOps *o)
{
    return o->pointer_in_rect(o->ctx, box[0], box[1], box[2], box[3]) != 0;
}

static void move_to(int16_t item, const ShowcaseFiguresOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

static void blit(const Figures *f, const char *tag, int masked, const ShowcaseFiguresOps *o)
{
    const uint8_t *shape = o->shape(o->ctx, f->bank, tag);
    if (masked) o->blit_masked(o->ctx, shape);
    else o->blit_own(o->ctx, shape);
}

/* the car's art and its palette */
static void load(Figures *f, const ShowcaseFiguresState *st, const ShowcaseFiguresOps *o, const char *which)
{
    snprintf(f->name, sizeof f->name, "%s%s%s.qfs", st->car_dir, CAR[*st->player_car], which);
    f->bank = o->load_bank(o->ctx, f->name, 0);
    o->music_feed(o->ctx);
    const uint8_t *pal = o->shape(o->ctx, f->bank, "!pal");
    o->palette_set(o->ctx, f->rgb, 0, ALL, pal + 0x10, pal[0]);
}

static void pick(Figures *f, int16_t item)
{
    if (f->selected == item) return;
    f->previous = f->selected;
    f->selected = item;
    f->changed = 1;
}

static void follow(Figures *f, const ShowcaseFiguresOps *o)
{
    const int kind = f->mec;
    if (in_box(AREA[kind], o)) {
        pick(f, BLOCK);
        int16_t arrow = NO_ARROW;
        if (in_box(BACK_BOX[kind], o)) arrow = BACK_ARROW;
        else if (in_box(ON_BOX[kind], o)) arrow = ON_ARROW;
        if (f->arrow != arrow) {
            f->arrow = arrow;
            f->changed = 1;
        }
    } else if (o->pointer_in_rect(o->ctx, 0, DONE_RIGHT[kind], 0x1ae, 0x1e0)) {
        pick(f, DONE);
    } else if (o->pointer_in_rect(o->ctx, BOTTOM_LEFT[0], BOTTOM_RIGHT[0], 0x1ae, 0x1e0)) {
        pick(f, PREVIOUS);
    } else if (o->pointer_in_rect(o->ctx, BOTTOM_LEFT[1], BOTTOM_RIGHT[1], 0x1ae, 0x1e0)) {
        pick(f, NEXT);
    } else {
        pick(f, NONE);
    }
}

static void draw(Figures *f, const ShowcaseFiguresState *st, const ShowcaseFiguresOps *o, const char *which)
{
    o->select(o->ctx, *st->screen);
    blit(f, "bgnd", 0, o);
    if (f->mec) {
        blit(f, MEC_PAGE[f->page], 0, o);
    } else {
        blit(f, PER_PAGE[f->page], 0, o);
        blit(f, PER_PAGE_2[f->page], 0, o);
    }
    if (f->selected == BLOCK) {
        blit(f, ARROW_LIT[0], 1, o);
        if (f->arrow == BACK_ARROW || f->arrow == ON_ARROW) blit(f, ARROW_LIT[f->arrow], 1, o);
    } else if (f->selected >= DONE && f->selected <= NEXT) {
        blit(f, BUTTON_LIT[f->selected - DONE], 0, o);
    }
    o->select_default(o->ctx);
    o->blit_word(o->ctx, o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    o->select(o->ctx, *st->screen);
    o->pointer_show(o->ctx);
    if (f->fade) {
        o->pointer_mark(o->ctx);
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        o->music_feed(o->ctx);
        snprintf(f->name, sizeof f->name, "%s%s16.eas", CAR[*st->player_car], which);
        o->voice_start(o->ctx, f->name);
        o->music_feed(o->ctx);
        o->fade_in(o->ctx, 0, ALL, f->rgb);
        f->fade = 0;
        o->voice_play(o->ctx);
    }
    o->deadline_set(o->ctx, WAIT);
    f->redraw = 0;
}

/* the item left dark (the block with its page's first shape again), the new one lit */
static void mark_change(Figures *f, const ShowcaseFiguresState *st, const ShowcaseFiguresOps *o)
{
    f->redraw = 1;
    o->select(o->ctx, *st->screen);
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    if (f->previous == BLOCK) {
        blit(f, "frmd", 1, o);
        blit(f, f->mec ? MEC_PAGE[f->page] : PER_PAGE[f->page], 0, o);
    } else if (f->previous >= DONE && f->previous <= NEXT) {
        blit(f, BUTTON_DARK[f->previous - DONE], 0, o);
    }
    if (f->selected == BLOCK) blit(f, ARROW_LIT[0], 1, o);
    else if (f->selected >= DONE && f->selected <= NEXT) blit(f, BUTTON_LIT[f->selected - DONE], 0, o);
    f->changed = 0;
}

/* German keeps to the first two pages: on toggles the byte's low bit, back skips the third. Taken with the
 * pointer that holds for anything not "mec", by the keys only for "per" (the same for the two callers). */
static int two_pages(const Figures *f, const ShowcaseFiguresState *st, int keyed)
{
    return *st->german && (keyed ? f->per : !f->mec);
}

static void page_on(Figures *f, int two)
{
    if (two) f->page = (int16_t)(f->page ^ 1);
    else f->page = (int16_t)((f->page + 1) % PAGES);
}

static void page_back(Figures *f, int two)
{
    f->page--;
    if (f->page < 0) f->page = PAGES - 1;
    if (two && f->page == 2) f->page = 1;
}

/* Previous and Next: a tournament keeps to its track's cars, else all eight in a ring */
static void step_car(int up, const ShowcaseFiguresState *st)
{
    int16_t car = (int16_t)(*st->player_car + (up ? 1 : -1));
    const int16_t track = *st->track;
    *st->player_car = car;
    if (*st->race_type != TOURNAMENT) {
        if (up) *st->player_car = (int16_t)(car % CARS);
        else if (car < 0) *st->player_car = 7;
    } else if (track == 0 || track == 3) {
        if (up && car > 2) *st->player_car = 0;
        else if (!up && car < 0) *st->player_car = 2;
    } else if (track == 1 || track == 4) {
        if (up && car > 5) *st->player_car = 3;
        else if (!up && car < 3) *st->player_car = 5;
    } else {
        if (up && car > 7) *st->player_car = 6;
        else if (!up && car < 6) *st->player_car = 7;
    }
}

/* the block's arrows as taken */
static void take_block(Figures *f, const ShowcaseFiguresState *st, const ShowcaseFiguresOps *o)
{
    const int kind = f->mec;
    if (in_box(ON_BOX[kind], o)) {
        page_on(f, two_pages(f, st, 0));
        f->redraw = 1;
    } else if (in_box(BACK_BOX[kind], o)) {
        page_back(f, two_pages(f, st, 0));
        f->redraw = 1;
    }
}

static void take(Figures *f, const ShowcaseFiguresState *st, const ShowcaseFiguresOps *o, const char *which)
{
    switch (f->selected) {
    case BLOCK: take_block(f, st, o); break;
    case DONE: f->quit = 1; break;
    case PREVIOUS:
    case NEXT:
        step_car(f->selected == NEXT, st);
        o->car_flags(o->ctx);
        o->voice_stop(o->ctx);
        o->voice_free(o->ctx);
        o->fade_out(o->ctx, 0, ALL);
        o->music_feed(o->ctx);
        o->free(o->ctx, f->bank);
        load(f, st, o, which);
        f->redraw = 1;
        f->fade = 1;
        break;
    default: break;
    }
}

/* Left / Right: from no item to Done (Done as the previous one too); on the block its page, with its arrow lit;
 * else along the bottom (Done, Previous, Next, round) */
static void sideways(Figures *f, int on, const ShowcaseFiguresState *st, const ShowcaseFiguresOps *o)
{
    o->pointer_off(o->ctx);
    if (f->selected == NONE) {
        f->selected = DONE;
        f->previous = DONE;
        move_to(DONE, o);
        f->changed = 1;
    } else if (f->selected == BLOCK) {
        blit(f, ARROW_LIT[on ? ON_ARROW : BACK_ARROW], 1, o);
        o->sound(o->ctx, CHOSEN);
        if (on) page_on(f, two_pages(f, st, 1));
        else page_back(f, two_pages(f, st, 1));
        move_to(f->selected, o);
        f->redraw = 1;
    } else {
        f->previous = f->selected;
        f->selected = (int16_t)(f->selected + (on ? 1 : -1));
        if (on && f->selected > NEXT) f->selected = DONE;
        else if (!on && f->selected < DONE) f->selected = NEXT;
        move_to(f->selected, o);
        f->changed = 1;
    }
}

static void upright(Figures *f, const int16_t *next, const ShowcaseFiguresOps *o)
{
    o->pointer_off(o->ctx);
    f->previous = f->selected;
    f->selected = next[f->selected];
    move_to(f->selected, o);
    f->changed = 1;
}

static void keys(Figures *f, const ShowcaseFiguresState *st, const ShowcaseFiguresOps *o, const char *which)
{
    if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
    if (*st->buttons == 0) f->released = 1;
    o->pointer_activate_if_moved(o->ctx);
    o->select(o->ctx, *st->screen);
    const uint16_t key = (uint16_t)o->key(o->ctx);
    if (key == SPACE || key == ENTER || (*st->buttons == LEFT_BUTTON && f->released)) {
        const int armed = f->armed;
        o->pointer_click(o->ctx);
        if (armed) {
            o->sound(o->ctx, CHOSEN);
            take(f, st, o, which);
        }
        o->ticks(o->ctx);
        f->released = 0;
        f->armed = 0;
        f->rest = 0;
    } else if (key != 0 && f->armed) {
        switch (key) {
        case ESCAPE: o->sound(o->ctx, CHOSEN), f->quit = 1; break;
        case LEFT: sideways(f, 0, st, o); break;
        case RIGHT: sideways(f, 1, st, o); break;
        case UP: upright(f, UP_OF, o); break;
        case DOWN: upright(f, DOWN_OF, o); break;
        default: break;
        }
        o->ticks(o->ctx);
        f->armed = 0;
        f->rest = 0;
    } else if (key == 0) {
        f->armed = 1;
    }
}

/* 00406008; its esp + 0x39c redraw of the arrows alone is never asked (only ever set 0), so it is left out */
int32_t showcase_figures(ShowcaseFiguresState *st, const ShowcaseFiguresOps *o, const char *which)
{
    Figures f = { .mec = strcmp(which, "mec") == 0, .per = strcmp(which, "per") == 0, .redraw = 1, .fade = 1,
                  .armed = 1, .released = 1 };
    if (*st->pointer_active == 0) {
        f.selected = f.previous = DONE;
        move_to(DONE, o);
    }
    load(&f, st, o, which);
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->pointer_mark(o->ctx);
    while (!f.quit) {
        follow(&f, o);
        if (f.redraw) draw(&f, st, o, which);
        if (f.changed) mark_change(&f, st, o);
        if (!f.armed) {
            f.rest += o->ticks(o->ctx);
            if ((int16_t)f.rest > REST) f.armed = 1;
        }
        keys(&f, st, o, which);
        const int fade = f.fade;
        o->music(o->ctx);
        if (!fade && (o->deadline_passed(o->ctx) || (*st->buttons == RIGHT_BUTTON && f.released))) {
            o->sound(o->ctx, CHOSEN);
            f.quit = 1;
        }
    }
    o->voice_stop(o->ctx);
    o->voice_free(o->ctx);
    o->fade_out(o->ctx, 0, ALL);
    o->free(o->ctx, f.bank);
    o->pointer_mark(o->ctx);
    return f.quit;
}

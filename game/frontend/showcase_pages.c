#include "game/frontend/showcase_pages.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
/* the two text blocks (their art "fr2" / "fr1", pages "f2s%d" / "f1s%d"), Done, Previous, Next */
enum { UPPER, LOWER, DONE, PREVIOUS, NEXT, NONE };
enum { NO_ARROW, BACK_ARROW, ON_ARROW };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, ALL = 0x100, PAGES = 5, CARS = 8,
       TOURNAMENT = 3 };

/* 00405efc..00405f4b, copied to the stack: the pointer's place by item, Up / Down's next item ("gen" only) */
static const int16_t POINTER_X[NONE + 1] = { 480, 400, 35, 230, 382, 320 };
static const int16_t POINTER_Y[NONE + 1] = { 170, 325, 445, 450, 450, 240 };
static const int16_t UP_OF[NONE + 1] = { 3, 0, 1, 1, 1, 2 };
static const int16_t DOWN_OF[NONE + 1] = { 1, 3, 0, 0, 0, 2 };
/* 00405f2c: the cars' short names, by 00524e8b */
static const char *const CAR[CARS] = { "rx7", "nsx", "sup", "911", "zr1", "vip", "512", "dia" };
/* the buttons along the bottom, asked in turn after the blocks */
static const int16_t BOTTOM_LEFT[3] = { 0, 200, 0x168 };
static const int16_t BOTTOM_RIGHT[3] = { 0x82, 0x104, 0x1a4 };
/* per block: its area and its arrows' boxes (back, on; the upper's on box one row shorter when taken) */
static const int16_t AREA[2][4] = { { 0x174, 0x254, 0x6f, 0xe7 }, { 0xc1, 0x25f, 0xef, 0x1a6 } };
static const int16_t HOVER_BACK[2][4] = { { 0x174, 0x18d, 0x9e, 0xb9 }, { 0xc3, 0xdc, 0x140, 0x155 } };
static const int16_t HOVER_ON[2][4] = { { 0x23a, 0x253, 0x9e, 0xba }, { 0x244, 0x25d, 0x140, 0x155 } };
static const int16_t TAKE_ON[2][4] = { { 0x23a, 0x253, 0x9e, 0xb9 }, { 0x244, 0x25d, 0x140, 0x155 } };
/* per block: lit, dark, its arrows lit (back, on) and its page's tag */
static const char *const BLOCK_LIT[2] = { "fr2l", "fr1l" };
static const char *const BLOCK_DARK[2] = { "fr2d", "fr1d" };
static const char *const ARROW_LIT[2][3] = { { "fr2l", "f2lf", "f2rt" }, { "fr1l", "f1lf", "f1rt" } };
static const char *const PAGE_TAG[2] = { "f2s%d", "f1s%d" };
static const char *const BUTTON_LIT[3] = { "donl", "prel", "nexl" };
static const char *const BUTTON_DARK[3] = { "dond", "pred", "nexd" };

typedef struct Pages {
    int32_t bank;           /* esi */
    int general;            /* which is "gen" (strcmp each time in the original) */
    int16_t selected;       /* esp + 0x398 */
    int16_t previous;       /* esp + 0x388 */
    int16_t arrow;          /* esp + 0x390: the block's arrow the pointer is on */
    int16_t page[2];        /* esp + 0x3a4, 0x3a8 */
    int changed;            /* esp + 0x394 */
    int redraw;             /* esp + 0x3a0 */
    int fade;               /* esp + 0x38c */
    int armed;              /* esp + 0x39c */
    int32_t rest;           /* esp + 0x384 */
    int released;           /* esp + 0x378 */
    int16_t quit;           /* esp + 0x37c */
    char name[SHOWCASE_PAGES_NAME];           /* esp + 0x300 */
    uint8_t rgb[SHOWCASE_PAGES_PALETTE];      /* esp + 0 */
} Pages;

static int in_box(const int16_t box[4], const ShowcasePagesOps *o)
{
    return o->pointer_in_rect(o->ctx, box[0], box[1], box[2], box[3]) != 0;
}

static void move_to(int16_t item, const ShowcasePagesOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[item], POINTER_Y[item]);
}

static void blit(const Pages *p, const char *tag, int masked, const ShowcasePagesOps *o)
{
    const uint8_t *shape = o->shape(o->ctx, p->bank, tag);
    if (masked) o->blit_masked(o->ctx, shape);
    else o->blit_own(o->ctx, shape);
}

static void blit_page(Pages *p, int block, const ShowcasePagesOps *o)
{
    snprintf(p->name, sizeof p->name, PAGE_TAG[block], p->page[block]);
    blit(p, p->name, 0, o);
}

/* the car's art and its palette */
static void load(Pages *p, const ShowcasePagesState *st, const ShowcasePagesOps *o, const char *which)
{
    snprintf(p->name, sizeof p->name, "%s%s%s.qfs", st->car_dir, CAR[*st->player_car], which);
    p->bank = o->load_bank(o->ctx, p->name, 0);
    o->music_feed(o->ctx);
    const uint8_t *pal = o->shape(o->ctx, p->bank, "!pal");
    o->palette_set(o->ctx, p->rgb, 0, ALL, pal + 0x10, pal[0]);
}

static void pick(Pages *p, int16_t item)
{
    if (p->selected == item) return;
    p->previous = p->selected;
    p->selected = item;
    p->changed = 1;
}

/* a block the pointer is in ("gen" only): the block and the arrow it is on */
static int follow_block(Pages *p, int block, const ShowcasePagesOps *o)
{
    if (!in_box(AREA[block], o) || !p->general) return 0;
    pick(p, (int16_t)block);
    int16_t arrow = NO_ARROW;
    if (in_box(HOVER_BACK[block], o)) arrow = BACK_ARROW;
    else if (in_box(HOVER_ON[block], o)) arrow = ON_ARROW;
    if (p->arrow != arrow) {
        p->arrow = arrow;
        p->changed = 1;
    }
    return 1;
}

static void follow(Pages *p, const ShowcasePagesOps *o)
{
    if (follow_block(p, UPPER, o) || follow_block(p, LOWER, o)) return;
    for (int i = 0; i < 3; i++) {
        if (o->pointer_in_rect(o->ctx, BOTTOM_LEFT[i], BOTTOM_RIGHT[i], 0x1ae, 0x1e0)) {
            pick(p, (int16_t)(DONE + i));
            return;
        }
    }
    pick(p, NONE);
}

static void draw(Pages *p, const ShowcasePagesState *st, const ShowcasePagesOps *o, const char *which)
{
    o->select(o->ctx, *st->screen);
    blit(p, "bgnd", 0, o);
    if (p->general) {
        blit_page(p, UPPER, o);
        blit_page(p, LOWER, o);
    }
    if (p->selected == UPPER || p->selected == LOWER) {
        blit(p, BLOCK_LIT[p->selected], 1, o);
        if (p->arrow == BACK_ARROW || p->arrow == ON_ARROW) blit(p, ARROW_LIT[p->selected][p->arrow], 1, o);
    } else if (p->selected >= DONE && p->selected <= NEXT) {
        blit(p, BUTTON_LIT[p->selected - DONE], 0, o);
    }
    o->select_default(o->ctx);
    o->blit_word(o->ctx, o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    o->select(o->ctx, *st->screen);
    o->pointer_show(o->ctx);
    if (p->fade) {
        o->pointer_mark(o->ctx);
        if (*st->pointer_active) o->pointer_buttons(o->ctx);
        o->music_feed(o->ctx);
        snprintf(p->name, sizeof p->name, "%s%s16.eas", CAR[*st->player_car], which);
        o->voice_start(o->ctx, p->name);
        o->music_feed(o->ctx);
        o->fade_in(o->ctx, 0, ALL, p->rgb);
        p->fade = 0;
        o->voice_play(o->ctx);
    }
    o->deadline_set(o->ctx, WAIT);
    p->redraw = 0;
}

/* the item left dark (a block with its page again), the new one lit */
static void mark_change(Pages *p, const ShowcasePagesState *st, const ShowcasePagesOps *o)
{
    p->redraw = 1;
    o->select(o->ctx, *st->screen);
    o->sound(o->ctx, MOVED);
    o->pointer_hide(o->ctx);
    o->lock(o->ctx);
    if (p->previous == UPPER || p->previous == LOWER) {
        blit(p, BLOCK_DARK[p->previous], 1, o);
        blit_page(p, p->previous, o);
    } else if (p->previous >= DONE && p->previous <= NEXT) {
        blit(p, BUTTON_DARK[p->previous - DONE], 0, o);
    }
    if (p->selected == UPPER || p->selected == LOWER) blit(p, BLOCK_LIT[p->selected], 1, o);
    else if (p->selected >= DONE && p->selected <= NEXT) blit(p, BUTTON_LIT[p->selected - DONE], 0, o);
    p->changed = 0;
}

static void page_on(Pages *p, int block) { p->page[block] = (int16_t)((p->page[block] + 1) % PAGES); }

static void page_back(Pages *p, int block)
{
    p->page[block]--;
    if (p->page[block] < 0) p->page[block] = PAGES - 1;
}

/* Previous and Next: a tournament keeps to its track's cars, else all eight in a ring */
static void step_car(int up, const ShowcasePagesState *st)
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

static void take(Pages *p, const ShowcasePagesState *st, const ShowcasePagesOps *o, const char *which)
{
    switch (p->selected) {
    case UPPER:
    case LOWER:
        if (in_box(TAKE_ON[p->selected], o)) page_on(p, p->selected), p->redraw = 1;
        else if (in_box(HOVER_BACK[p->selected], o)) page_back(p, p->selected), p->redraw = 1;
        break;
    case DONE: p->quit = 1; break;
    case PREVIOUS:
    case NEXT:
        step_car(p->selected == NEXT, st);
        o->car_flags(o->ctx);
        o->voice_stop(o->ctx);
        o->voice_free(o->ctx);
        o->fade_out(o->ctx, 0, ALL);
        o->music_feed(o->ctx);
        o->free(o->ctx, p->bank);
        load(p, st, o, which);
        p->redraw = 1;
        p->fade = 1;
        break;
    default: break;
    }
}

/* Left / Right: from no item to Done; in a block its page, with its arrow lit; else along the bottom (Done,
 * Previous, Next, round) */
static void sideways(Pages *p, int on, const ShowcasePagesOps *o)
{
    o->pointer_off(o->ctx);
    if (p->selected == NONE) {
        p->previous = NONE;
        p->selected = DONE;
        move_to(DONE, o);
        p->changed = 1;
    } else if (p->selected == UPPER || p->selected == LOWER) {
        blit(p, ARROW_LIT[p->selected][on ? ON_ARROW : BACK_ARROW], 1, o);
        o->sound(o->ctx, CHOSEN);
        if (on) page_on(p, p->selected);
        else page_back(p, p->selected);
        move_to(p->selected, o);
        p->redraw = 1;
    } else {
        p->previous = p->selected;
        p->selected = (int16_t)(p->selected + (on ? 1 : -1));
        if (on && p->selected > NEXT) p->selected = DONE;
        else if (!on && p->selected < DONE) p->selected = NEXT;
        move_to(p->selected, o);
        p->changed = 1;
    }
}

/* Up / Down: from no item to Done (Done as the previous one too); "gen" by the tables */
static void upright(Pages *p, const int16_t *next, const ShowcasePagesOps *o)
{
    if (p->selected == NONE) {
        p->selected = DONE;
        o->pointer_off(o->ctx);
        move_to(DONE, o);
        p->previous = DONE;
        p->changed = 1;
    } else if (p->general) {
        o->pointer_off(o->ctx);
        p->previous = p->selected;
        p->selected = next[p->selected];
        move_to(p->selected, o);
        p->changed = 1;
    }
}

static void keys(Pages *p, const ShowcasePagesState *st, const ShowcasePagesOps *o, const char *which)
{
    if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
    if (*st->buttons == 0) p->released = 1;
    o->pointer_activate_if_moved(o->ctx);
    o->select(o->ctx, *st->screen);
    const uint16_t key = (uint16_t)o->key(o->ctx);
    if (key == SPACE || key == ENTER || (*st->buttons == LEFT_BUTTON && p->released)) {
        const int armed = p->armed;
        o->pointer_click(o->ctx);
        if (armed) {
            o->sound(o->ctx, CHOSEN);
            take(p, st, o, which);
        }
        o->ticks(o->ctx);
        p->released = 0;
        p->armed = 0;
        p->rest = 0;
    } else if (key != 0 && p->armed) {
        switch (key) {
        case ESCAPE: o->sound(o->ctx, CHOSEN), p->quit = 1; break;
        case LEFT: sideways(p, 0, o); break;
        case RIGHT: sideways(p, 1, o); break;
        case UP: upright(p, UP_OF, o); break;
        case DOWN: upright(p, DOWN_OF, o); break;
        default: break;
        }
        o->ticks(o->ctx);
        p->armed = 0;
        p->rest = 0;
    } else if (key == 0) {
        p->armed = 1;
    }
}

/* 00407290; its esp + 0x380 redraw of the arrows alone is never asked (only ever set 0), so it is left out */
int32_t showcase_pages(ShowcasePagesState *st, const ShowcasePagesOps *o, const char *which)
{
    Pages p = { .general = strcmp(which, "gen") == 0, .redraw = 1, .fade = 1, .armed = 1, .released = 1 };
    if (strcmp(which, "his") == 0) p.selected = p.previous = DONE;
    if (*st->pointer_active == 0) {
        p.selected = p.previous = DONE;
        move_to(DONE, o);
    }
    load(&p, st, o, which);
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->pointer_mark(o->ctx);
    while (!p.quit) {
        follow(&p, o);
        if (p.redraw) draw(&p, st, o, which);
        if (p.changed) mark_change(&p, st, o);
        if (!p.armed) {
            p.rest += o->ticks(o->ctx);
            if ((int16_t)p.rest > REST) p.armed = 1;
        }
        keys(&p, st, o, which);
        const int fade = p.fade;
        o->music(o->ctx);
        if (!fade && (o->deadline_passed(o->ctx) || (*st->buttons == RIGHT_BUTTON && p.released))) {
            o->sound(o->ctx, CHOSEN);
            p.quit = 1;
        }
    }
    o->voice_stop(o->ctx);
    o->voice_free(o->ctx);
    o->fade_out(o->ctx, 0, ALL);
    o->free(o->ctx, p.bank);
    o->pointer_mark(o->ctx);
    return p.quit;
}

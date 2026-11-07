#include "game/frontend/net_host.h"

#include "game/frontend/text_fit.h"
#include "game/sys/port_log.h"

#include <stddef.h>
#include <string.h>

enum { NAME_X = 0x3a, TITLE_Y = 0xf8, FIRST_Y = 0x113, LINE = 0x12 };

/* 00453e40 */
void net_players_draw(int16_t german, const uint8_t *address, const NetPlayersDrawOps *ops)
{
    const RaceNetPlayers *const p = ops->players(ops->ctx, address);
    ops->text_select(ops->ctx, 2);
    ops->text_boxed(ops->ctx, german != 0 ? "Rennfahrer" : "Racers:", NAME_X, TITLE_Y);  /* 004b0e68 / 004b0e74 */
    ops->text_select(ops->ctx, 0);
    if (p == NULL) return;
    int32_t y = FIRST_Y;
    for (int32_t i = 0; i < p->count; i++, y += LINE)
        ops->text_boxed(ops->ctx, (const char *)p->list + i * RACE_NET_PLAYER_SIZE, NAME_X, y);
}

/* 00453ef4's items: the arrows, the rows 1..8 between them, New, Cancel */
enum { UP_ARROW = 0, ROWS = 8, DOWN_ARROW = 9, NEW = 10, CANCEL = 11 };
enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000 };
enum { OPENED = 9, SHOWN = 8, STEP_WAIT = 10, WAIT = 12000, ASK_AGAIN = 200, REST = 0xf, CHIME = 0x49, TIMEOUT = 100 };
enum { ROW_X = 0x18b, ROW_Y = 0xb3, ROW_LINE = 0x15, ROW_WIDTH = 0xe6, WIDE_NAME = 0x9b };

/* 00453e00 / 00453e18: the items' spots */
static const int16_t SPOT_X[12] = { 460, 460, 460, 460, 460, 460, 460, 460, 460, 460, 372, 551 };
static const int16_t SPOT_Y[12] = { 176, 196, 217, 238, 259, 280, 301, 322, 343, 358, 270, 270 };
/* 004c5574..: the box's frames by step */
static const char *const FRAME[8] = { NULL, "Box2", "Box3", "Box4", "Box5", "Box6", "Box1", "Box1" };

typedef struct Box {
    RaceNetGames *games;  /* ebp */
    int32_t under;        /* esp + 0x44: 0x104 x 0xf0, what lies under the players */
    int32_t behind;       /* esp + 0x48: 0xd2 x 0x10a, what lies behind the box */
    int16_t selected;     /* esp + 0x68 */
    int16_t previous;     /* esp + 0x50 */
    int16_t scroll;       /* esp + 0x60: the first game shown */
    int16_t step;         /* esp + 0x64: the box opening, 9 once open */
    int changed;          /* esp + 0x54 */
    int armed;            /* esp + 0x4c: the keys taken */
    int released;         /* esp + 0x78: the buttons seen up */
    int done;             /* esp + 0x74 */
    uint32_t rest;        /* esp + 0x58: the ticks summed (a dword, compared by its low half) */
    uint16_t k;           /* bx: the last key */
} Box;

static void move_to(const Box *b, const NetHostOps *o)
{
    o->pointer_move_to(o->ctx, SPOT_X[b->selected], SPOT_Y[b->selected]);
}

static void pick(Box *b, int16_t item)
{
    b->previous = b->selected;
    b->selected = item;
    b->changed = 1;
}

static int no_new(const NetHostState *st) { return st->view != 0 || st->no_music != 0; }

/* the pointer's spots, the first one it is on taken (the rows all asked) */
static void follow(Box *b, const NetHostState *st, const NetHostOps *o)
{
    if (o->pointer_in_rect(o->ctx, 0x1c1, 0x1d9, 0xa9, 0xb7)) {
        if (b->selected != UP_ARROW) pick(b, UP_ARROW);
    } else if (o->pointer_in_rect(o->ctx, 0x1c1, 0x1d9, 0x15e, 0x16e)) {
        if (b->selected != DOWN_ARROW) pick(b, DOWN_ARROW);
    } else if (o->pointer_in_rect(o->ctx, 0x16c, 0x17d, 0xd3, 0x147) && !no_new(st)) {
        if (b->selected != NEW) pick(b, NEW);
    } else if (o->pointer_in_rect(o->ctx, 0x21f, 0x230, 0xde, 0x13b)) {
        if (b->selected != CANCEL) pick(b, CANCEL);
    } else {
        for (int16_t i = 1; i <= ROWS; i++) {
            const int16_t top = (int16_t)(0xba + ROW_LINE * (i - 1)), bottom = (int16_t)(0xcd + ROW_LINE * (i - 1));
            if (o->pointer_in_rect(o->ctx, 0x17f, 0x21e, top, bottom) && i != b->selected
                && i <= b->games->count - b->scroll)
                pick(b, i);
        }
    }
}

static void blit(const NetHostState *st, const NetHostOps *o, const char *tag)
{
    o->blit_own(o->ctx, o->shape(o->ctx, st->bank, tag));
}

/* the lit tag of the arrows, New and Cancel */
static void item_lit(const Box *b, const NetHostState *st, const NetHostOps *o)
{
    if (b->selected == UP_ARROW) blit(st, o, "Arl1");
    else if (b->selected == DOWN_ARROW) blit(st, o, "Arl2");
    else if (b->selected == NEW) blit(st, o, "Newl");
    else if (b->selected == CANCEL) blit(st, o, "Cacl");
}

/* the eight rows from the scroll on, the picked one lit once the box is nearly open */
static void rows(const Box *b, const NetHostOps *o)
{
    char name[0x60];  /* the original's is 20 bytes on its stack */
    int16_t y = ROW_Y;
    for (int16_t row = 0; row < ROWS; row++) {
        const int32_t at = row + b->scroll;
        if (at >= b->games->count) continue;
        strcpy(name, (const char *)b->games->list + at * RACE_NET_GAME_SIZE);
        if (o->text_width(o->ctx, name) > WIDE_NAME) text_lower(name);
        if (row + 1 == b->selected) {
            if (b->step > 6) o->text_select(o->ctx, 1);
            o->text_fit(o->ctx, name, ROW_X, y, ROW_WIDTH);
            o->text_select(o->ctx, 0);
        } else {
            o->text_fit(o->ctx, name, ROW_X, y, ROW_WIDTH);
        }
        y = (int16_t)(y + ROW_LINE);
    }
}

static void present(const NetHostState *st, const NetHostOps *o)
{
    o->select_default(o->ctx);
    o->lock(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
}

/* one pass of the box (all its opening steps while it opens) */
static void draw(Box *b, NetHostState *st, const NetHostOps *o)
{
    do {
        if (b->step == 1) o->sound(o->ctx, CHIME);
        if (b->step < SHOWN) o->deadline_set(o->ctx, STEP_WAIT);
        o->select(o->ctx, st->screen);
        blit(st, o, "Box1");
        item_lit(b, st, o);
        o->text_select(o->ctx, 0);
        rows(b, o);
        if (b->step < SHOWN) {
            o->blit_masked(o->ctx, o->shape(o->ctx, st->bank, FRAME[b->step]));
            if (b->step > 5) item_lit(b, st, o);
        }
        if (b->step < OPENED) b->step++;
        if ((int16_t)b->rest > ASK_AGAIN) {
            if (b->selected >= 1 && b->selected <= ROWS) {
                o->restore(o->ctx, o->picture_shape(o->ctx, b->under), 0, 0xb9);
                o->players_draw(o->ctx, b->games->list + (b->selected - 1) * RACE_NET_GAME_SIZE + RACE_NET_GAME_ADDRESS);
            }
            b->rest = 0;
            o->ticks(o->ctx);
        } else {
            o->restore(o->ctx, o->picture_shape(o->ctx, b->under), 0, 0xb9);
        }
        present(st, o);
        if (b->step > SHOWN) o->pointer_show(o->ctx);
        if (b->step < SHOWN) while (!o->deadline_passed(o->ctx)) {}
        if (b->step == SHOWN) {
            b->armed = 0;
            st->key = 0;
        }
    } while (b->step < OPENED);
    b->changed = 0;
}

/* every 200 ticks the games asked again; a row past them goes to the last, or Cancel */
static void ask_again(Box *b, const NetHostOps *o)
{
    const int16_t sel = b->selected;
    b->games = o->games(o->ctx, TIMEOUT);
    if (b->games->count != 0) port_log("mark:game_listed\n");   /* port: the test harness's guest waits for it before picking the game */
    if (sel >= 1 && sel <= ROWS && sel > b->games->count) b->selected = b->games->count != 0 ? b->games->count : CANCEL;
    b->changed = 1;
}

static void choose(Box *b, const NetHostState *st, const NetHostOps *o)
{
    if (!b->armed) return;
    o->pointer_click(o->ctx);
    if (st->buttons == 2) {
        b->done = 1;
    } else if (b->selected == UP_ARROW) {
        if (b->scroll > 0) b->changed = 1, b->scroll--;
    } else if (b->selected == DOWN_ARROW) {
        if (b->scroll < b->games->count - ROWS) b->changed = 1, b->scroll++;
    } else if (b->selected >= 1 && b->selected <= ROWS) {
        if (b->scroll + b->selected <= b->games->count) b->done = 1;
    } else if (b->selected == NEW || b->selected == CANCEL) {
        b->done = 1;
    }
    b->armed = 0;
    b->released = 0;
    b->rest = 0;
    o->ticks(o->ctx);
}

/* Left: to New, or from Cancel back */
static void left(Box *b, const NetHostState *st, const NetHostOps *o)
{
    if (b->selected == CANCEL) {
        o->pointer_off(o->ctx);
        if (b->selected != b->previous) b->selected = b->previous;
        else if (b->games->count != 0) b->selected = 1;
        else if (!no_new(st)) b->selected = NEW;
    } else if (b->selected != NEW && !no_new(st)) {
        o->pointer_off(o->ctx);
        b->previous = b->selected;
        b->selected = NEW;
    }
}

/* Right: to Cancel, or from New back */
static void right(Box *b, const NetHostOps *o)
{
    if (b->selected == NEW) {
        o->pointer_off(o->ctx);
        if (b->selected != b->previous) b->selected = b->previous;
        else b->selected = b->games->count != 0 ? 1 : CANCEL;
    } else {
        o->pointer_off(o->ctx);
        if (b->selected != CANCEL) b->previous = b->selected, b->selected = CANCEL;
    }
}

/* Up / Down in the list (the arrows scroll it) */
static void up_down(Box *b, uint16_t k, const NetHostOps *o)
{
    if (b->selected == NEW || b->selected == CANCEL) return;
    o->pointer_off(o->ctx);
    if (k == UP) {
        if (b->selected > 0) {
            if (b->selected > 1 || b->scroll != 0) b->selected--;
        } else if (b->scroll > 0) {
            b->scroll--;
        }
    } else if (b->selected < DOWN_ARROW) {
        if (b->selected < b->games->count) b->selected++;
    } else if (b->scroll < b->games->count - ROWS) {
        b->scroll++;
    }
}

static void step_keys(Box *b, const NetHostState *st, const NetHostOps *o)
{
    const uint16_t k = b->k;
    if (k == ESCAPE) {
        b->done = 1;
    } else if (k == LEFT || k == RIGHT || k == UP || k == DOWN) {
        if (k == LEFT) left(b, st, o);
        else if (k == RIGHT) right(b, o);
        else up_down(b, k, o);
        move_to(b, o);
        b->changed = 1;
    }
    b->armed = 0;
    b->rest = 0;
    o->ticks(o->ctx);
}

static void input(Box *b, NetHostState *st, const NetHostOps *o)
{
    if (!b->armed) {
        b->rest += (uint32_t)o->ticks(o->ctx);
        if ((int16_t)b->rest > REST) b->armed = 1;
    }
    if (st->pointer_active) st->buttons = (int16_t)o->pointer_buttons(o->ctx);
    if (st->buttons == 0) b->released = 1;
    if (!b->released) {
        b->rest += (uint32_t)o->ticks(o->ctx);
        if ((int16_t)b->rest > REST) b->released = 1;
    }
    o->pointer_activate_if_moved(o->ctx);
    o->music(o->ctx);
    b->k = (uint16_t)o->key(o->ctx);
    if (b->k == 0) {
        if (st->key != 0) b->armed = 1, b->k = (uint16_t)st->key;
        st->key = 0;
    }
    if (st->buttons == 2 && b->released) b->selected = CANCEL, b->armed = 1, b->k = ENTER;
    if (b->k == SPACE || b->k == ENTER || (st->buttons != 0 && b->released)) choose(b, st, o);
    else if (b->k != 0 && b->armed) step_keys(b, st, o);
    else if (b->k == 0) b->armed = 1;
}

/* the box closed in seven steps, the screen put back */
static void close_box(const Box *b, const NetHostState *st, const NetHostOps *o)
{
    o->sound(o->ctx, CHIME);
    for (int16_t s = 6; s >= 0; s--) {
        o->deadline_set(o->ctx, STEP_WAIT);
        o->select(o->ctx, st->screen);
        o->blit_masked(o->ctx, o->shape(o->ctx, st->bank, FRAME[s + 1]));
        present(st, o);
        while (!o->deadline_passed(o->ctx)) {}
    }
    o->deadline_set(o->ctx, WAIT);
    o->picture_free(o->ctx, b->under);
    o->select(o->ctx, st->screen);
    o->restore(o->ctx, o->picture_shape(o->ctx, b->behind), 0x166, 0x6c);
    o->select_default(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, st->screen));
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    o->picture_free(o->ctx, b->behind);
    o->clip(o->ctx, 0, 0, 0x280, 0x1e0);
}

/* 00453ef4 */
int32_t net_host_join(NetHostState *st, const NetHostOps *ops)
{
    Box b = { .selected = NEW, .previous = NEW, .step = 1, .changed = 1 };
    b.behind = ops->picture_new(ops->ctx, 0xd2, 0x10a, 0);
    b.under = ops->picture_new(ops->ctx, 0x104, 0xf0, 0);
    ops->select(ops->ctx, st->screen);
    ops->save(ops->ctx, ops->picture_shape(ops->ctx, b.under), 0, 0xb9);
    ops->save(ops->ctx, ops->picture_shape(ops->ctx, b.behind), 0x166, 0x6c);
    if (no_new(st)) b.selected = b.previous = CANCEL;
    b.games = ops->games(ops->ctx, TIMEOUT);
    if (b.games->count != 0) b.selected = b.previous = 1, port_log("mark:game_listed\n");
    ops->clip(ops->ctx, 0x166, 0xa0, 0x237, 0x176);
    move_to(&b, ops);
    ops->ticks(ops->ctx);
    while (!b.done) {
        follow(&b, st, ops);
        b.rest += (uint32_t)ops->ticks(ops->ctx);
        if ((int16_t)b.rest > ASK_AGAIN) ask_again(&b, ops);
        if (b.changed) draw(&b, st, ops);
        input(&b, st, ops);
    }
    close_box(&b, st, ops);
    if (b.selected == CANCEL || b.k == ESCAPE) return 0;
    if (b.selected == NEW) return ops->host(ops->ctx, st->name) != 0;
    if (b.selected >= 1 && b.selected <= ROWS)
        return ops->connect(ops->ctx, b.games->list + (b.scroll + b.selected - 1) * RACE_NET_GAME_SIZE + RACE_NET_GAME_ADDRESS);
    return 0;
}

/* 00432e90 */
void net_kick(int16_t german, int32_t player, const NetKickOps *ops)
{
    const char *const text = german != 0 ? "Spieler ausschlie\xe1" "en?" : "Kick this player out ?";  /* 004af7f4 with 0xe1 / 004af80c */
    if ((int16_t)ops->yes_no(ops->ctx, text) != 0) ops->drop(ops->ctx, (int16_t)player);
}

#include "game/frontend/game_list.h"

#include "game/frontend/file_list.h"

#include <stdio.h>
#include <string.h>

enum {
    UP_ARROW = 0, ROWS = 10, DOWN_ARROW = 0xb, CANCEL = 0xd, FRAMES = 9, SHOWN = 8, OPENED = 6, NAME = 0xf,
    SOUND_MOVE = 0x47, SOUND_CHOSEN = 0x48, SOUND_OPEN = 0x49, FRAME_TIME = 10, REST = 0xf, TOLD = 0x64,
    KEY_SPACE = 0x20, KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b, KEY_UP = 0x4800, KEY_LEFT = 0x4b00, KEY_RIGHT = 0x4d00,
    KEY_DOWN = 0x5000, RIGHT_BUTTON = 2, TEXT = 0x28, PLAYERS = 8, PLAYER = 0x95, LAPS = 0x14
};

/* where the pointer goes per item (004291c0, 004291dc): the arrows, the ten rows, 12 (none), Cancel */
static const int16_t POINTER_X[14] = { 455, 455, 455, 455, 455, 455, 455, 455, 455, 455, 455, 455, 366, 545 };
static const int16_t POINTER_Y[14] = { 121, 143, 165, 186, 207, 228, 249, 270, 291, 312, 333, 356, 237, 237 };
/* the replays the race writes (004aeff8..) */
static const char *const SYSTEM_REPLAY[4] = { "REPLAY.RPL", "REPLAY1.RPL", "REPLAY2.RPL", "REPLAY3.RPL" };

typedef struct List {
    int32_t bank;     /* ebp */
    int32_t window;   /* esp + 0x104: 0xcd x 0x106, the list drawn in it */
    int32_t under;    /* esp + 0x100: 0x280 x 0xc8, the screen under a banner */
    int16_t mode;     /* esp + 0x110 */
    int16_t count;    /* esp + 0x114 */
    int16_t top;      /* esp + 0x118: the first name shown */
    int16_t frame;    /* esp + 0x11c: 1..8 the window opening, then 9 */
    int16_t item;     /* esp + 0x12c */
    int16_t was;      /* esp + 0x134: the item Right left, for Left */
    int redraw;       /* esp + 0x108 */
    int armed;        /* esp + 0x124 */
    int released;     /* esp + 0x10c */
    int done;         /* esp + 0x120 */
    int32_t result;   /* esp + 0x13c */
    int32_t rest;     /* esp + 0x138: the ticks summed, compared as a short */
    char cwd[GAME_LIST_PATH];  /* esp + 0 */
} List;

static uint8_t *name_at(const GameListState *st, int i)
{
    return st->names + i * FILE_LIST_NAME;
}

/* sprintf(names[i], found): the name as its own format (the oracle's names hold no '%') */
static void put_name(const GameListState *st, int i, const char *found)
{
    strcpy((char *)name_at(st, i), found);
}

/* every name emptied, then the matches of pattern read in, up to 100 */
static void read_names(List *l, GameListState *st, const char *pattern, const GameListOps *ops)
{
    for (int16_t i = 0; i < GAME_LIST_NAMES; i++) name_at(st, i)[0] = 0;
    l->count = 0;
    const char *found = ops->find_first(ops->ctx, pattern);
    if (!found) return;
    put_name(st, 0, found);
    for (int16_t i = 1; i < GAME_LIST_NAMES; i++) {
        l->count = i;
        found = ops->find_next(ops->ctx);
        if (!found) break;
        put_name(st, i, found);
    }
}

/* the path: the working directory, '/', the saves' or replays' directory, '/' made '\', its last character cut */
static void enter_dir(const List *l, const char *dir, const GameListOps *ops)
{
    char path[GAME_LIST_PATH * 2];
    strcpy(path, l->cwd);
    strcat(path, "/");
    strcat(path, dir);
    for (char *c = path; (c = strchr(c, '/')) != NULL;) *c = '\\';
    path[strlen(path) - 1] = 0;
    ops->chdir(ops->ctx, path);
}

static void move_to(const List *l, const GameListOps *ops)
{
    ops->pointer_move_to(ops->ctx, POINTER_X[l->item], POINTER_Y[l->item]);
}

static void show(const GameListState *st, const GameListOps *ops)
{
    ops->select_default(ops->ctx);
    ops->lock(ops->ctx);
    ops->blit_own(ops->ctx, ops->picture_shape(ops->ctx, st->screen));
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
}

/* the item under the pointer: an arrow, Cancel, or a row with a name */
static void follow(List *l, const GameListOps *ops)
{
    int16_t now = -1;
    if (ops->pointer_in_rect(ops->ctx, 0x178, 0x217, 0x70, 0x84)) {
        now = UP_ARROW;
    } else if (ops->pointer_in_rect(ops->ctx, 0x178, 0x217, 0x15c, 0x16e)) {
        now = DOWN_ARROW;
    } else if (ops->pointer_in_rect(ops->ctx, 0x217, 0x22a, 0x8c, 0x154)) {
        now = CANCEL;
    } else {
        int16_t y0 = 0x87, y1 = 0x9a;
        for (int16_t row = 1; row <= ROWS; row++, y0 += 0x15, y1 += 0x15) {
            if (ops->pointer_in_rect(ops->ctx, 0x178, 0x217, y0, y1) && row != l->item && row <= l->count - l->top) {
                l->was = l->item;
                l->item = row;
                l->redraw = 1;
            }
        }
        return;
    }
    if (now == l->item) return;
    l->was = l->item;
    l->item = now;
    l->redraw = 1;
}

/* the window: its frames while it opens (the names lit from frame 7), then once a change */
static void draw(List *l, const GameListState *st, const GameListOps *ops)
{
    char text[TEXT];
    do {
        if (l->frame == 1) ops->sound(ops->ctx, SOUND_OPEN);
        if (l->frame < SHOWN) ops->deadline_set(ops->ctx, FRAME_TIME);
        ops->select(ops->ctx, l->window);
        ops->blit_shape_at(ops->ctx, ops->shape(ops->ctx, l->bank, "opn8"), 0, 0);
        if (l->item == UP_ARROW) {
            ops->blit_shape_at(ops->ctx, ops->shape(ops->ctx, l->bank, "arul"), 0x5b, 7);
        } else if (l->item == DOWN_ARROW) {
            ops->blit_shape_at(ops->ctx, ops->shape(ops->ctx, l->bank, "ardl"), 0x5b, 0xf1);
        } else if (l->item == CANCEL) {
            ops->blit_shape_at(ops->ctx, ops->shape(ops->ctx, l->bank, "canl"), 0xb7,
                         st->german ? 0x4e : 0x55);
        }
        ops->text_select(ops->ctx, 0);
        int16_t y = 0x1c;
        for (int16_t row = 0; row < ROWS; row++) {
            const uint8_t *name = name_at(st, l->top + row);
            if (!name[0]) continue;
            int16_t n = 0;
            for (; name[n] != '.'; n++) text[n] = (char)name[n];
            text[n] = 0;
            if (row + 1 != l->item) {
                ops->text_fit(ops->ctx, text, 0x23, y, 0x7d);
            } else {
                if (l->frame > OPENED) ops->text_select(ops->ctx, 1);
                ops->text_fit(ops->ctx, text, 0x23, y, 0x7d);
                ops->text_select(ops->ctx, 0);
            }
            y += 0x15;
        }
        if (l->frame < SHOWN) {
            snprintf(text, sizeof text, "opn%d", l->frame);
            ops->blit_masked_at(ops->ctx, ops->shape(ops->ctx, l->bank, text), 0, 0);
        }
        if (l->frame > SHOWN) ops->sound(ops->ctx, SOUND_MOVE);
        if (l->frame < FRAMES) l->frame++;
        ops->select(ops->ctx, st->screen);
        ops->blit_at(ops->ctx, ops->picture_shape(ops->ctx, l->window), 0x161, 0x6c);
        show(st, ops);
        const int16_t frame = l->frame;
        ops->pointer_show(ops->ctx);
        if (frame < SHOWN) {
            while (!ops->deadline_passed(ops->ctx)) ops->wait(ops->ctx);
        }
    } while (l->frame < FRAMES);
    l->redraw = 0;
}

/* the screen under a banner kept, the banner, the screen put back */
static void tell(const List *l, const GameListState *st, const char *text, const GameListOps *ops)
{
    ops->select(ops->ctx, st->screen);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, l->under), 0, 0xd2);
    ops->banner_wait(ops->ctx, text, TOLD);
}

static void untell(const List *l, const GameListState *st, const GameListOps *ops)
{
    ops->select(ops->ctx, st->screen);
    ops->blit_at(ops->ctx, ops->picture_shape(ops->ctx, l->under), 0, 0xd2);
    show(st, ops);
    ops->pointer_show(ops->ctx);
}

static int16_t ask(const char *german, const char *english, const GameListState *st, const GameListOps *ops)
{
    char text[GAME_LIST_PATH];
    if (!st->german) return (int16_t)ops->yes_no(ops->ctx, english);
    snprintf(text, sizeof text, german, 0x94);
    return (int16_t)ops->yes_no(ops->ctx, text);
}

/* after a delete: the list read again, the first name lit (Cancel when none), a page scrolled back */
static void listed_again(List *l, GameListState *st, const char *pattern, const GameListOps *ops)
{
    read_names(l, st, pattern, ops);
    if (l->mode == GAME_LIST_DELETE_REPLAY) file_list_drop_replays(st->names, &l->count);
    l->item = l->was = l->count == 0 ? CANCEL : 1;
    if (l->count - l->top < ROWS && l->top > 0) l->top--;
    l->redraw = 1;
}

static void delete_file(List *l, GameListState *st, const uint8_t *name, const char *pattern, const GameListOps *ops)
{
    char path[TEXT];
    snprintf(path, sizeof path, "%s", (const char *)name);
    ops->remove_file(ops->ctx, path);
    listed_again(l, st, pattern, ops);
}

static void load_game(List *l, GameListState *st, const uint8_t *name, const GameListOps *ops)
{
    ops->select(ops->ctx, st->screen);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, l->under), 0, 0xd2);
    memcpy(st->game_kept, st->game, GAME_LIST_STATE);
    ops->game_load(ops->ctx, (const char *)name);
    for (int16_t p = 0; p < PLAYERS; p++) {
        uint8_t *player = st->game + p * PLAYER;
        memset(player + 0x59, 0, 0x18);
        for (int16_t lap = 0; lap < LAPS; lap++) memset(player + 9 + lap * 4, 0, 4);
    }
    *st->tournament = 1;
    memcpy(st->config, st->game + 0x4a8, 0xac);
    memcpy(st->controls, st->game + 0x554, 0x1a);
    *st->lost_vegas = (int16_t)(st->game[0x56e] | st->game[0x56f] << 8);
    st->config[0x64] = 3, st->config[0x65] = 0;
    ops->music_fade_to(ops->ctx, (int16_t)(st->config[0x6e] | st->config[0x6f] << 8));
    ops->banner_wait(ops->ctx, st->german ? "Spielstand erfolgreich geladen" : "Game loaded successfully", TOLD);
    untell(l, st, ops);
    const int16_t track = (int16_t)(st->config[0x8c] | st->config[0x8d] << 8);
    const int16_t other = (int16_t)(st->config[0x8a] | st->config[0x8b] << 8);
    if (track < 3 && other > 0) l->result = 1;
    l->done = 1;
}

/* a row acted on: by mode, load or delete its file */
static void pick(List *l, GameListState *st, const GameListOps *ops)
{
    ops->clip(ops->ctx, 0, 0, 0x280, 0x1e0);
    const uint8_t *name = name_at(st, l->item + l->top - 1);
    if (name[0]) {
        ops->sound(ops->ctx, SOUND_CHOSEN);
        if (l->mode == GAME_LIST_DELETE_GAME) {
            if (ask("Spiel l%cschen ?", "Delete game ?", st, ops)) delete_file(l, st, name, "*.SAV", ops);
        } else if (l->mode == GAME_LIST_DELETE_REPLAY) {
            int system = 0;
            for (int k = 0; k < 4 && !system; k++) system = file_list_same_name(name, SYSTEM_REPLAY[k]);
            if (system) {
                char text[GAME_LIST_PATH];
                if (st->german) snprintf(text, sizeof text, "Kann system-weiderholung nicht l%cschen", 0x94);
                tell(l, st, st->german ? text : "Cannot delete system replay", ops);
                l->redraw = 1;
                untell(l, st, ops);
            } else if (ask("Weiderholung l%cschen ?", "Delete replay ?", st, ops)) {
                delete_file(l, st, name, "*.rpl", ops);
            }
        } else if (l->mode == GAME_LIST_LOAD_GAME) {
            if ((int16_t)ops->yes_no(ops->ctx, st->german ? "Spiel laden ?" : "Load game ?")) load_game(l, st, name, ops);
        } else if (l->mode == GAME_LIST_LOAD_REPLAY) {
            const char *q = st->german ? (st->highlight ? "Highlight laden ?" : "Wiederholung laden ?")
                                       : (st->highlight ? "Load highlight ?" : "Load replay ?");
            if ((int16_t)ops->yes_no(ops->ctx, q)) {
                strcpy(st->replay, (const char *)name);  /* sprintf "%s": the whole name, its NUL too */
                l->result = 1;
                l->done = 1;
            }
        }
    }
    ops->clip(ops->ctx, 0x165, 0x6f, 0x22a, 0x16e);
    ops->pointer_redraw(ops->ctx);
    move_to(l, ops);
    ops->pointer_show(ops->ctx);
}

static void act(List *l, GameListState *st, const GameListOps *ops)
{
    switch ((uint16_t)l->item) {
    case UP_ARROW:
        if (l->top > 0) ops->sound(ops->ctx, SOUND_MOVE), l->redraw = 1, l->top--;
        break;
    case DOWN_ARROW:
        if (l->top < l->count - ROWS) ops->sound(ops->ctx, SOUND_MOVE), l->redraw = 1, l->top++;
        break;
    case CANCEL:
        l->done = 1;
        break;
    case 12:
        break;
    default:
        if ((uint16_t)l->item <= ROWS) pick(l, st, ops);
        break;
    }
}

/* the arrows: Left back from Cancel, Right to it, Up and Down through the names (and the page) */
static void step(List *l, const GameListState *st, uint16_t key, const GameListOps *ops)
{
    if (key == KEY_ESCAPE) {
        l->done = 1;
        return;
    }
    if (key != KEY_LEFT && key != KEY_RIGHT && key != KEY_UP && key != KEY_DOWN) return;
    ops->pointer_off(ops->ctx);
    if (key == KEY_LEFT) {
        if (l->item == CANCEL && l->was != CANCEL) l->item = l->was;
    } else if (key == KEY_RIGHT) {
        if (l->item != CANCEL) l->was = l->item, l->item = CANCEL;
    } else if (key == KEY_UP) {
        if (l->item == CANCEL) {
        } else if (l->item > 0) {
            /* name_at(-1) is 004fa1c8's byte, 0x44 in the exe's data: the up arrow stops it */
            for (l->item--; l->item > 0 && !name_at(st, l->item - 1)[0];) l->item--;
        } else if (l->top > 0) {
            l->top--;
        }
    } else if (l->item != CANCEL) {
        if (l->item < DOWN_ARROW) {
            l->item++;
            if (!name_at(st, l->item - 1)[0]) l->item = DOWN_ARROW;
        } else if (l->top < l->count - ROWS) {
            l->top++;
        }
    }
    move_to(l, ops);
    l->redraw = 1;
}

/* 0042ae9c */
int32_t game_list_screen(int32_t mode, GameListState *st, const GameListOps *ops)
{
    static List zero;
    List l = zero;
    l.mode = (int16_t)mode;
    l.redraw = 1;
    l.frame = 1;
    l.item = l.was = CANCEL;
    ops->text_styles(ops->ctx);
    l.window = ops->picture_new(ops->ctx, 0xcd, 0x106, 0x20);
    l.under = ops->picture_new(ops->ctx, 0x280, 0xc8, 0);
    ops->select(ops->ctx, st->screen);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, l.window), 0, 0);
    l.bank = ops->load_art(ops->ctx, st->check ? "check/loadanim.qfs" : "option/loadanim.qfs");
    ops->music_feed(ops->ctx);
    int16_t dot = 0;
    while (st->replay[dot] != '.' && dot < NAME) dot++;
    st->replay[dot] = 0;
    for (int16_t i = 0; i < GAME_LIST_NAMES; i++) name_at(st, i)[0] = 0;
    ops->getcwd(ops->ctx, l.cwd);
    const int saves = l.mode == GAME_LIST_LOAD_GAME || l.mode == GAME_LIST_DELETE_GAME;
    enter_dir(&l, saves ? st->save_dir : st->replay_dir, ops);
    const char *found = ops->find_first(ops->ctx, saves ? "*.SAV" : "*.RPL");
    if (found) {
        l.item = l.was = 1;
        put_name(st, 0, found);
        for (int16_t i = 1; i < GAME_LIST_NAMES; i++) {
            l.count = i;
            found = ops->find_next(ops->ctx);
            if (!found) break;
            put_name(st, i, found);
        }
    }
    if (l.mode == GAME_LIST_DELETE_REPLAY) file_list_drop_replays(st->names, &l.count);
    /* the saved game's or the replay's name lit, the page scrolled to it */
    for (int16_t i = 0; i < l.count; i++) {
        char want[GAME_LIST_PATH];
        if (l.mode == GAME_LIST_LOAD_GAME) snprintf(want, sizeof want, "%s.SAV", (const char *)st->config);
        else if (l.mode == GAME_LIST_LOAD_REPLAY) snprintf(want, sizeof want, "%s.RPL", st->replay);
        else continue;
        if (strcmp(want, (const char *)name_at(st, i)) != 0) continue;
        if (l.count < ROWS) {
            l.top = 0;
            l.item = l.was = (int16_t)(i + 1);
        } else if (i < l.count - ROWS) {
            l.top = i;
        } else {
            l.top = (int16_t)(l.count - ROWS);
            l.item = l.was = (int16_t)(i - l.top + 1);
        }
    }
    ops->clip(ops->ctx, 0x165, 0x6f, 0x22a, 0x16e);
    move_to(&l, ops);
    ops->ticks(ops->ctx);
    while (!l.done) {
        follow(&l, ops);
        if (l.redraw) draw(&l, st, ops);
        if (!l.armed) {
            l.rest += ops->ticks(ops->ctx);
            if ((int16_t)l.rest > REST) l.armed = 1;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (*st->buttons == 0) l.released = 1;
        if (!l.released) {
            l.rest += ops->ticks(ops->ctx);
            if ((int16_t)l.rest > REST) l.released = 1;
        }
        ops->pointer_activate_if_moved(ops->ctx);
        ops->music(ops->ctx);
        uint16_t key = (uint16_t)ops->key(ops->ctx);
        if (*st->buttons == RIGHT_BUTTON && l.released) {
            l.item = CANCEL;
            l.armed = 1;
            key = KEY_ENTER;
        }
        if (key == KEY_SPACE || key == KEY_ENTER || (*st->buttons != 0 && l.released)) {
            const int armed = l.armed;
            ops->pointer_click(ops->ctx);
            if (armed) {
                act(&l, st, ops);
                ops->ticks(ops->ctx);
                l.armed = 0;
                l.released = 0;
                l.rest = 0;
            }
        } else if (key != 0 && l.armed) {
            step(&l, st, key, ops);
            l.armed = 0;
            l.rest = 0;
            ops->ticks(ops->ctx);
        } else if (key == 0) {
            l.armed = 1;
        }
    }
    ops->chdir(ops->ctx, l.cwd);
    ops->sound(ops->ctx, SOUND_OPEN);
    for (int16_t frame = 7; frame > 0; frame--) {
        char text[TEXT];
        snprintf(text, sizeof text, "opn%d", frame);
        ops->deadline_set(ops->ctx, FRAME_TIME);
        ops->select(ops->ctx, st->screen);
        ops->blit_masked(ops->ctx, ops->shape(ops->ctx, l.bank, text));
        show(st, ops);
        while (!ops->deadline_passed(ops->ctx)) {
        }
    }
    ops->picture_free(ops->ctx, l.window);
    ops->picture_free(ops->ctx, l.under);
    ops->free(ops->ctx, l.bank);
    ops->clip(ops->ctx, 0, 0, 0x280, 0x1e0);
    return l.result;
}

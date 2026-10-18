#include "game/frontend/chat.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, BACKSPACE = 8, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00,
       DOWN = 0x5000, DELETE = 0x5300, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, ALL = 0x100, WIDE = 0x20b, BLINK = 0xf, WRAP_WORD = 7,
       PICTURE_W = 0x24a, PICTURE_H = 0x18, LINE_X = 0x1b, TYPED_Y = 0x76, HISTORY_Y = 0xcd, STEP = 0x18,
       TEXT_X = 0x13 };
/* the typed text, Done */
enum { TEXT, DONE };

/* 0040a3c4 / 0040a3c8, copied to the stack: the pointer's place by item */
static const int16_t POINTER_X[2] = { 320, 60 };
static const int16_t POINTER_Y[2] = { 145, 450 };
/* the German keys taken past ASCII (ü ä ö ß µ Ä Ü Ö ° §) */
static const int32_t GERMAN_KEYS[] = { 0x81, 0x84, 0x94, 0xe1, 0xe6, 0x8e, 0x9a, 0x99, 0xf8, 0x15 };

/* the message with room either side: the original's cursor can step before
 * its typed text (into the name) and past it (into its stack) */
enum { SLACK = 0x80 };

typedef struct Chat {
    int32_t bank;           /* esp + 0x41c */
    int32_t scratch;        /* esp + 0x420: a picture a line is drawn on */
    int32_t typed[2];       /* esp + 0x404: the typed lines' backgrounds */
    int32_t history[CHAT_HISTORY];  /* esp + 0x3e0 */
    int16_t selected;       /* esp + 0x438 */
    int16_t line;           /* esp + 0x440 */
    int16_t column;         /* esp + 0x450 */
    int16_t phase;          /* esp + 0x44c: the cursor hidden */
    int32_t blink;          /* esp + 0x444 */
    int32_t redraw;         /* esp + 0x43c (tested as a short) */
    int16_t quit;           /* esp + 0x448 */
    uint8_t store[SLACK + CHAT_MESSAGE + SLACK];
    uint8_t *message;       /* esp + 0x300 */
    uint8_t rgb[CHAT_PALETTE];  /* esp + 0 */
} Chat;

/* a byte of a typed line, written only within the store */
static void put(Chat *c, int line, int column, uint8_t v)
{
    const long at = SLACK + CHAT_TEXT + (long)line * CHAT_LINE + column;
    if (at >= 0 && at < (long)sizeof c->store) c->store[at] = v;
}

static char *text(Chat *c, int line) { return (char *)c->message + CHAT_TEXT + line * CHAT_LINE; }

/* the original's string copy: forward, a byte at a time, kept within the store */
static void copy(Chat *c, char *to, const char *from)
{
    const char *end = (const char *)c->store + sizeof c->store;
    for (; to < end && from < end; to++, from++) {
        *to = *from;
        if (*from == 0) break;
    }
}

/* a line drawn on the scratch picture over its background, then shown on the screen */
static void draw_line(const Chat *c, int32_t background, const char *s, int32_t x, int32_t y,
                      const ChatState *st, const ChatOps *o)
{
    o->select(o->ctx, c->scratch);
    o->blit(o->ctx, o->picture_shape(o->ctx, background), 0, 0);
    if (s != NULL) o->text_shadowed(o->ctx, s, x, 0);
    o->select(o->ctx, *st->screen);
    o->show(o->ctx, o->picture_shape(o->ctx, c->scratch), LINE_X, y);
}

static void draw_typed(Chat *c, int line, const ChatState *st, const ChatOps *o)
{
    draw_line(c, c->typed[line], text(c, line), TEXT_X, TYPED_Y + line * STEP, st, o);
}

/* the cursor after the text, the line drawn */
static void cursor(Chat *c, const ChatState *st, const ChatOps *o)
{
    put(c, c->line, c->column, '_');
    put(c, c->line, c->column + 1, 0);
    draw_typed(c, c->line, st, o);
    c->redraw++;
}

/* back to the line above: its last character under the cursor */
static void line_up(Chat *c)
{
    c->line--;
    c->column = (int16_t)(strlen(text(c, c->line)) - 1);
}

static void wait_released(const ChatState *st, const ChatOps *o)
{
    if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_poll(o->ctx);
    while (o->keys_pending(o->ctx) != 0 || *st->buttons != 0) {
        o->music(o->ctx);
        if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_poll(o->ctx);
    }
}

static void chat_open(Chat *c, ChatState *st, const ChatOps *o)
{
    *st->up = 1;
    put(c, 0, 0, 0);
    put(c, 1, 0, 0);
    c->message[0] = CHAT_KIND, c->message[1] = 0;
    /* the name copied to its NUL (the original's unbounded copy) */
    for (size_t i = 0; SLACK + CHAT_NAME + i < sizeof c->store; i++) {
        c->message[CHAT_NAME + i] = (uint8_t)st->name[i];
        if (st->name[i] == 0) break;
    }
    c->bank = o->load_bank(o->ctx, "control/chat.qfs");
    const uint8_t *pal = o->shape(o->ctx, c->bank, "!pal");
    o->palette_set(o->ctx, c->rgb, 0, ALL, pal + 0x10, pal[0]);
    o->select(o->ctx, *st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, c->bank, "bgnd"));
    o->blit_masked(o->ctx, o->shape(o->ctx, c->bank, "Senl"));
    o->select_default(o->ctx);
    o->blit_word(o->ctx, o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    if (*st->pointer_active) o->pointer_poll(o->ctx);
    o->fade_in(o->ctx, 0, ALL, c->rgb);
    o->inverse_build(o->ctx);
    o->text_styles(o->ctx);
    c->scratch = o->picture_new(o->ctx, PICTURE_W, PICTURE_H, 0);
    for (int i = 0; i < 2; i++) c->typed[i] = o->picture_new(o->ctx, PICTURE_W, PICTURE_H, 0);
    for (int i = 0; i < CHAT_HISTORY; i++) c->history[i] = o->picture_new(o->ctx, PICTURE_W, PICTURE_H, 0);
    o->select(o->ctx, *st->screen);
    for (int i = 0; i < 2; i++) o->grab(o->ctx, o->picture_shape(o->ctx, c->typed[i]), LINE_X, TYPED_Y + i * STEP);
    for (int i = 0; i < CHAT_HISTORY; i++)
        o->grab(o->ctx, o->picture_shape(o->ctx, c->history[i]), LINE_X, HISTORY_Y + i * STEP);
    o->drain(o->ctx);
    wait_released(st, o);
    if (*st->german) o->layout_on(o->ctx);
    o->pointer_move_to(o->ctx, POINTER_X[c->selected], POINTER_Y[c->selected]);
    o->pointer_mark(o->ctx);
}

/* Done or the text, by the pointer: the buttons lit or dark */
static void follow(Chat *c, const ChatState *st, const ChatOps *o)
{
    const int16_t item = o->pointer_in_rect(o->ctx, 0, 0x7d, 0x1ae, 0x1e0) ? DONE : TEXT;
    if (c->selected == item) return;
    o->sound(o->ctx, MOVED);
    c->selected = item;
    o->select(o->ctx, *st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, c->bank, item == DONE ? "Donl" : "Dond"));
    o->blit_masked(o->ctx, o->shape(o->ctx, c->bank, item == DONE ? "Send" : "Senl"));
    c->redraw++;
}

/* the lines come in, from 004b905f round */
static void history(Chat *c, const ChatState *st, const ChatOps *o)
{
    o->text_select(o->ctx, 0);
    int16_t i = *st->line;
    for (int k = 0; k < CHAT_HISTORY; k++) {
        if (i == CHAT_HISTORY) i = 0;
        const char *s = i >= 0 && i < CHAT_HISTORY ? (const char *)st->lines + i * CHAT_HISTORY_LINE : NULL;
        draw_line(c, c->history[k], s, 0, HISTORY_Y + k * STEP, st, o);
        i++;
        o->yield(o->ctx, 0);
    }
    *st->arrived = 0;
    c->redraw++;
    o->lines_done(o->ctx, *st->lines_lock);
}

/* Backspace / Delete: the character before the cursor; at a line's start, or
 * when the line fits after the one above (it ends in a space then), back up */
static void erase(Chat *c, const ChatState *st, const ChatOps *o)
{
    put(c, c->line, c->column, 0);
    if (c->column == 0) {
        if (c->line == 0) return;
        draw_typed(c, c->line, st, o);
        line_up(c);
    } else {
        c->column--;
        if (c->line != 0 && text(c, c->line)[0] != ' ') {
            put(c, c->line, c->column, 0);
            const int32_t above = o->text_width(o->ctx, text(c, c->line - 1));
            if (above + o->text_width(o->ctx, text(c, c->line)) <= WIDE) {
                put(c, c->line, c->column, ' ');
                char *above_text = text(c, c->line - 1);
                copy(c, above_text + strlen(above_text), text(c, c->line));
                put(c, c->line, 0, 0);
                draw_typed(c, c->line, st, o);
                line_up(c);
            }
        }
    }
    cursor(c, st, o);
}

/* Enter on the text: the message sent (and to this machine's own lines when it hosts), the lines cleared */
static void send(Chat *c, const ChatState *st, const ChatOps *o)
{
    if (c->line == 0 && c->column == 0) return;
    o->sound(o->ctx, CHOSEN);
    put(c, c->line, c->column, 0);
    o->send(o->ctx, 0, c->message, CHAT_MESSAGE);
    if (*st->host) o->on_chat(o->ctx, 0, c->message, CHAT_MESSAGE);
    c->line = 0, c->column = 0;
    for (int i = 0; i < 2; i++) {
        put(c, i, 0, 0);
        o->select(o->ctx, c->scratch);
        o->blit(o->ctx, o->picture_shape(o->ctx, c->typed[i]), 0, 0);
        if (i == 0) {
            put(c, 0, 0, '_'), put(c, 0, 1, 0);
            o->text_shadowed(o->ctx, text(c, 0), TEXT_X, 0);
        }
        o->select(o->ctx, *st->screen);
        o->show(o->ctx, o->picture_shape(o->ctx, c->scratch), LINE_X, TYPED_Y + i * STEP);
    }
    c->redraw++;
}

static int takes(int32_t key, const ChatState *st)
{
    if (key > 0x1f && key < 0x7f) return 1;
    if (*st->german == 0) return 0;
    for (size_t i = 0; i < sizeof GERMAN_KEYS / sizeof GERMAN_KEYS[0]; i++)
        if (key == GERMAN_KEYS[i]) return 1;
    return 0;
}

/* a character typed: past 0x20b pixels the second line takes it (with the
 * last word under 7 characters); the second line full drops it */
static void type(Chat *c, int32_t key, const ChatState *st, const ChatOps *o)
{
    if (!takes(key, st)) return;
    put(c, c->line, c->column, (uint8_t)key);
    c->column++;
    if (o->text_width(o->ctx, text(c, c->line)) <= WIDE) {
        cursor(c, st, o);
        return;
    }
    c->column--;
    put(c, c->line, c->column, 0);
    if (c->line + 1 == 2) return;
    c->line++;
    if (key != SPACE) {
        char *space = strrchr(text(c, 0), ' ');
        if (space != NULL && strlen(space) < WRAP_WORD) {
            copy(c, text(c, c->line), space + 1);
            space[1] = 0;
        }
    }
    draw_typed(c, 0, st, o);
    const size_t n = strlen(text(c, c->line));
    c->column = (int16_t)(n + 1);
    put(c, c->line, (int)n, (uint8_t)key);
    cursor(c, st, o);
}

static void take(Chat *c, uint32_t key, const ChatState *st, const ChatOps *o)
{
    switch (key) {
    case UP: case LEFT: case RIGHT: case DOWN:
        /* the other item: Done from the text, the text from Done */
        o->pointer_move_to(o->ctx, POINTER_X[c->selected == TEXT], POINTER_Y[c->selected == TEXT]);
        break;
    case BACKSPACE: case DELETE:
        if (c->selected != DONE) erase(c, st, o);
        break;
    case ENTER:
        if (c->selected == DONE) {
            o->pointer_click(o->ctx);
            c->quit = 1;
        } else {
            send(c, st, o);
        }
        break;
    case ESCAPE:
        o->pointer_click(o->ctx);
        c->quit = 1;
        break;
    default:
        if (c->selected != DONE) type(c, (int32_t)key, st, o);
        break;
    }
    o->ticks(o->ctx);
}

/* the keys until none is left (the original's last key, esp + 0x414, is only ever 0) */
static int32_t keys(Chat *c, const ChatState *st, const ChatOps *o)
{
    for (;;) {
        int32_t key = o->key_read(o->ctx);
        if (key == 0 && o->joystick_poll(o->ctx) == 0) return key;
        if (c->quit) return key;
        if (key == 0) {
            key = *st->joystick_key != 0 ? *st->joystick_key : *st->joystick_key2;
            if (key == SPACE) key = ESCAPE;
        } else if (*st->german && (key == SPACE || key == BACKSPACE)) {
            o->key_read(o->ctx);
        }
        if (key == 0) return key;
        o->music(o->ctx);
        o->text_select(o->ctx, 1);
        take(c, (uint32_t)key, st, o);
    }
}

/* the cursor blinking */
static void blink(Chat *c, const ChatState *st, const ChatOps *o)
{
    c->blink += o->ticks(o->ctx);
    if ((int16_t)c->blink < BLINK) return;
    o->text_select(o->ctx, 1);
    o->select(o->ctx, c->scratch);
    o->blit(o->ctx, o->picture_shape(o->ctx, c->typed[c->line]), 0, 0);
    c->phase ^= 1;
    if (c->phase) put(c, c->line, c->column, 0);
    o->text_shadowed(o->ctx, text(c, c->line), TEXT_X, 0);
    put(c, c->line, c->column, '_');
    put(c, c->line, c->column + 1, 0);
    o->select(o->ctx, *st->screen);
    o->show(o->ctx, o->picture_shape(o->ctx, c->scratch), LINE_X, TYPED_Y + c->line * STEP);
    c->blink = 0;
    c->redraw++;
}

static void chat_close(Chat *c, ChatState *st, const ChatOps *o)
{
    o->sound(o->ctx, CHOSEN);
    o->picture_free(o->ctx, c->scratch);
    for (int i = 0; i < 2; i++) o->picture_free(o->ctx, c->typed[i]);
    for (int i = 0; i < CHAT_HISTORY; i++) o->picture_free(o->ctx, c->history[i]);
    wait_released(st, o);
    if (*st->german) o->layout_off(o->ctx);
    *st->up = 0;
    o->fade_out(o->ctx, 0, ALL);
    o->art_free(o->ctx);
    o->free(o->ctx, c->bank);
    *st->central_redraw = 1;
}

/* 0040a5d8 */
void chat_screen(ChatState *st, const ChatOps *o)
{
    Chat chat = { 0 };
    Chat *c = &chat;
    c->message = c->store + SLACK;
    chat_open(c, st, o);
    while (!c->quit && *st->link_lost == 0) {
        o->link_service(o->ctx);
        follow(c, st, o);
        o->music(o->ctx);
        if (*st->arrived != 0 && o->lines_ready(o->ctx, *st->lines_lock)) history(c, st, o);
        if (keys(c, st, o) == 0) blink(c, st, o);
        if ((int16_t)c->redraw != 0) {
            o->surface_lock(o->ctx);
            o->lock(o->ctx);
            o->blit_word(o->ctx, o->picture_shape(o->ctx, *st->screen));
            o->pointer_show(o->ctx);
            c->redraw = 0;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
        if (*st->buttons == LEFT_BUTTON) {
            o->pointer_click(o->ctx);
            if (c->selected == DONE && !c->quit) c->quit = 1;
        } else if (*st->buttons == RIGHT_BUTTON) {
            c->quit = 1;
        }
        if (*st->game_over == 1) c->quit = 1;
    }
    chat_close(c, st, o);
}

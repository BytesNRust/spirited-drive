#include "game/frontend/text_entry.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, BACKSPACE = 8, DELETE = 0x5300, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };

/* until no key is pending and no button held: the music served, the buttons polled while the pointer is active */
static void settle(TextEntryState *st, const TextEntryOps *o)
{
    while (o->keys_pending(o->ctx) || st->buttons != 0) {
        o->music(o->ctx);
        if (st->pointer_active) st->buttons = (int16_t)o->pointer_poll(o->ctx);
    }
}

/* The box drawn over the screen under it, the music served between the steps; after an edit (blinking 0) the
 * screen is put before the pointer is hidden, while blinking after, and the cursor taken away (blinking 2) serves
 * the music once more before the flip. */
static void put(const TextEntryOps *o, const TextEntryState *st, int32_t box, int32_t under, const char *text,
                int32_t x, int32_t left, int32_t top, int blinking)
{
    o->select(o->ctx, box);
    o->music(o->ctx);
    o->blit(o->ctx, o->picture_shape(o->ctx, under), 0, 0);
    o->music(o->ctx);
    o->text_shadowed(o->ctx, text, x, 0);
    o->music(o->ctx);
    o->select_default(o->ctx);
    if (blinking) o->pointer_hide(o->ctx);
    o->music(o->ctx);
    o->blit_own(o->ctx, o->picture_shape(o->ctx, st->screen));
    if (blinking) o->music(o->ctx);
    else o->pointer_hide(o->ctx), o->music(o->ctx);
    o->show(o->ctx, o->picture_shape(o->ctx, box), left, top);
    if (blinking == 2) o->music(o->ctx);
    o->flip(o->ctx);
}

static void cursor_of(char *cursor, const char *typed)
{
    strcpy(cursor, typed);
    strcat(cursor, "_");
}

/* 004920ac: the C library's isalnum, ASCII only */
static int alnum(uint8_t c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

static int has_alnum(const char *typed, int16_t n)
{
    for (int16_t i = 0; i < n; i++)
        if (alnum((uint8_t)typed[i])) return 1;
    return 0;
}

/* the characters taken only with free_text, in the order 004626d0 tests them */
static int free_only(int16_t k)
{
    static const int16_t KEYS[] = { 0x20, 0x2e, 0x5c, 0x2f, 0x25, 0x2a, 0x2b, 0x5b, 0x5d, 0x7c, 0x3a, 0x3b, 0x22,
                                    0x3c, 0x3e, 0x2c, 0x3f };
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
        if (k == KEYS[i]) return 1;
    return 0;
}

static int never(int16_t k) { return k == 0x25 || k == 0x5c || k == 0x2f || k == 0x7c; }

/* CP437's ü ä ö ß µ Ä Ü Ö ° §, in the order 004626d0 tests them */
static int umlaut(int16_t k)
{
    return k == 0x81 || k == 0x84 || k == 0x94 || k == 0xe1 || k == 0xe6 || k == 0x8e || k == 0x9a || k == 0x99 ||
           k == 0xf8 || k == 0x15;
}

/* 004626d0 */
int32_t text_entry_edit(char *text, int32_t max, int32_t x, int32_t y, int32_t free_text, int german,
                        TextEntryState *st, const TextEntryOps *ops)
{
    enum { H = 0x23, STEP = 30, MARGIN = 5, FULL = 0x1b8, BLINK = 0xf, STYLE = 1 };
    char typed[TEXT_ENTRY_TYPED] = { 0 };
    char cursor[TEXT_ENTRY_TYPED + 2];
    const int16_t most = (int16_t)max;
    const int free = (int16_t)free_text != 0;
    const int32_t span = most / 2 * STEP;
    const int16_t left = (int16_t)((int16_t)x - span - MARGIN);
    const int32_t top = (int16_t)y;
    ops->drain(ops->ctx);
    if (st->pointer_active) st->buttons = (int16_t)ops->pointer_poll(ops->ctx);
    settle(st, ops);
    if (german) ops->layout_on(ops->ctx);
    const int32_t width = (int16_t)((int16_t)x + span + MARGIN) - left;
    const int16_t half = (int16_t)(width / 2);
    const int32_t box = ops->picture_new(ops->ctx, width, H, 0);
    const int32_t under = ops->picture_new(ops->ctx, width, H, 0);
    int16_t n = (int16_t)strlen(text);
    strncpy(typed, text, (size_t)n + 1);
    cursor_of(cursor, typed);
    ops->text_select(ops->ctx, STYLE);
    ops->select(ops->ctx, st->screen);
    ops->grab(ops->ctx, ops->picture_shape(ops->ctx, under), left, top);
    ops->select(ops->ctx, box);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, under), 0, 0);
    int16_t w = (int16_t)ops->text_width(ops->ctx, typed);
    ops->text_shadowed(ops->ctx, typed, half - w / 2, 0);
    ops->select_default(ops->ctx);
    ops->blit_own(ops->ctx, ops->picture_shape(ops->ctx, st->screen));
    ops->show(ops->ctx, ops->picture_shape(ops->ctx, box), left, top);
    ops->flip(ops->ctx);
    settle(st, ops);
    ops->drain(ops->ctx);
    int32_t sum = 0;
    int blink = 0;
    int16_t k;
    for (;;) {
        st->buttons = (int16_t)ops->pointer_poll(ops->ctx);
        ops->music(ops->ctx);
        ops->joystick_poll(ops->ctx);
        k = (int16_t)ops->key_read(ops->ctx);
        if (k == 0) {
            if (st->buttons == LEFT_BUTTON || st->joystick_key != 0) k = ENTER;
            else if (st->buttons == RIGHT_BUTTON) k = ESCAPE;
        } else if (german && (k == SPACE || k == BACKSPACE)) {
            ops->key_read(ops->ctx);
        }
        if (k == ENTER && !has_alnum(typed, n)) k = 0;
        if (k == 0) {
            sum += ops->ticks(ops->ctx);
            if ((int16_t)sum < BLINK) continue;
            blink ^= 1;
            put(ops, st, box, under, blink ? cursor : typed, half - w / 2, left, top, blink ? 1 : 2);
            sum = 0;
            continue;
        }
        if (k == ENTER || k == ESCAPE) break;
        if ((free_only(k) && !free) || never(k)) {
            /* refused */
        } else if (k == BACKSPACE && n > 0) {
            typed[--n] = 0;
        } else if (k == DELETE && n > 0) {
            n = 0;
            memset(typed, 0, sizeof typed);
        } else if (n < most && ops->text_width(ops->ctx, typed) < FULL && k > 0x1f && k < 0x7f) {
            typed[n++] = (char)(!free && k > 0x60 && k < 0x7b ? k - 0x20 : k);
        } else if (german && n < most && ops->text_width(ops->ctx, typed) < FULL && umlaut(k)) {
            typed[n++] = (char)k;
        }
        w = (int16_t)ops->text_width(ops->ctx, typed);
        cursor_of(cursor, typed);
        put(ops, st, box, under, typed, half - w / 2, left, top, 0);
        ops->ticks(ops->ctx);
    }
    if (k == ENTER) strncpy(text, typed, (size_t)(most + 1));
    ops->select_default(ops->ctx);
    ops->pointer_hide(ops->ctx);
    ops->blit(ops->ctx, ops->picture_shape(ops->ctx, under), left, top);
    ops->flip(ops->ctx);
    ops->picture_free(ops->ctx, box);
    ops->picture_free(ops->ctx, under);
    if (german) ops->layout_off(ops->ctx);
    return k == ENTER;
}

#include "game/frontend/key_capture.h"

#include <string.h>

enum { ESCAPE = 0x1b, SPACE = 0x20, BACKSPACE = 8, RIGHT_BUTTON = 2 };

/* the characters 004657d0 never takes (the ranges tested at 00465862) */
static int refused(uint32_t k)
{
    return k == 9 || (k >= 0x21 && k <= 0x26) || (k >= 0x28 && k <= 0x2b) || k == 0x2d || k == 0x3a || k == 0x3c ||
           (k >= 0x3e && k <= 0x40) || k == 0x4d || k == 0x50 || k == 0x5e || k == 0x5f || k == 0x6d || k == 0x70 ||
           (k >= 0x7b && k <= 0x7e);
}

/* CP437's ü Ü Ö ö Ä ä ß, in the order 004657d0 tests them */
static int umlaut(uint32_t k)
{
    return k == 0x81 || k == 0x9a || k == 0x99 || k == 0x94 || k == 0x8e || k == 0x84 || k == 0xe1;
}

static uint32_t capital(uint32_t k) { return k >= 'a' && k <= 'z' ? k - 0x20 : k; }

/* name written as the original's sprintf(name, "%c", k) */
static uint32_t put_char(char *name, uint32_t k)
{
    name[0] = (char)k;
    name[1] = 0;
    return k;
}

/* The key's name into name; the key as named, or 0 when it names nothing (or, in German, '`'). */
static uint32_t name_key(char *name, uint32_t k, int german)
{
    /* 004b17ac.. */
    static const struct { uint32_t key; const char *name; } NAMED[] = {
        { 0x20, "SPACE" }, { 0xd, "ENTER" }, { 0x5200, "INSERT" }, { 0x5300, "DELETE" }, { 0x4700, "HOME" },
        { 0x4f00, "END" }, { 0x4900, "PGUP" }, { 0x5100, "PGDN" }, { 0x4800, "UP" }, { 0x5000, "DOWN" },
        { 0x4b00, "LEFT" }, { 0x4d00, "RIGHT" }, { 8, "BCKSPC" }, { 9, "TAB" }, { 0x4c00, "CENTER" },
    };
    for (size_t i = 0; i < sizeof NAMED / sizeof NAMED[0]; i++)
        if (k == NAMED[i].key) {
            strcpy(name, NAMED[i].name);
            return k;
        }
    if (!(k >= 0x21 && k <= 0x7e) && !(german && umlaut(k))) return 0;
    if (!german) return put_char(name, capital(k));
    if (k == '<' || k == '>' || k == '^' || k == '`') k = 0;
    k = capital(k);
    if (k == 0x84) return put_char(name, 0x8e);
    if (k == 0x94) return put_char(name, 0x99);
    if (k == 0x81) return put_char(name, 0x9a);
    return put_char(name, k);
}

/* 004657d0 */
int32_t key_capture(char *name, int german, KeyCaptureState *st, const KeyCaptureOps *ops)
{
    while (ops->keys_pending(ops->ctx)) {
        ops->music(ops->ctx);
        if (st->pointer_active) ops->pointer_poll(ops->ctx);
    }
    if (german) ops->layout_on(ops->ctx);
    ops->ticks(ops->ctx);
    ops->drain(ops->ctx);
    uint32_t k;
    do {
        ops->music(ops->ctx);
        if (st->pointer_active) st->buttons = (int16_t)ops->pointer_poll(ops->ctx);
        k = st->shift_left || st->shift_right ? 0 : (uint32_t)ops->key_read(ops->ctx);
        if (german && (k == SPACE || k == BACKSPACE)) ops->key_read(ops->ctx);
        if (refused(k)) k = 0;
        if (k == ESCAPE || st->buttons == RIGHT_BUTTON || st->quit == 1) {
            if (german) ops->layout_off(ops->ctx);
            return 0;
        }
        if (k) k = name_key(name, k, german);
    } while (k == 0);
    if (german) ops->layout_off(ops->ctx);
    return (int32_t)k;
}

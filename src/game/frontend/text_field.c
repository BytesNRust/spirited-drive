/* 004626d0: one-line text entry, used for the player name. The text is drawn
 * centred in yellow with a shadow; a "_" cursor blinks every 15 ticks.
 * Enter or the left button accept (only if something other than spaces was
 * typed), Escape or the right button cancel, Backspace deletes, Delete clears. */
#include "game/frontend/text_field.h"

#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    WORK_SIZE = 42,        /* 00461ec2 template */
    FIELD_HEIGHT = 35,     /* 0x23 */
    MAX_TEXT_WIDTH = 0x1B7,
    BLINK_TICKS = 14
};

typedef struct {
    char work[WORK_SIZE];
    int length;            /* sStack_14 */
    int max_length;        /* param_2 */
    int x0, y, width;
    uint8_t *saved;        /* iStack_34: what was under the field */
    int cursor_on;         /* local_28 */
    int blink_ticks;       /* local_18 */
} Field;

static void draw(const Field *f, int with_cursor)
{
    char shown[WORK_SIZE + 2];
    snprintf(shown, sizeof shown, "%s%s", f->work, with_cursor ? "_" : "");  /* 004b16a0 */
    video_write_rect(f->x0, f->y, f->width, FIELD_HEIGHT, f->saved);
    text_select(TEXT_YELLOW);
    /* Centred on the text without the cursor, so the cursor does not shift it. */
    text_draw_shadowed(shown, f->x0 + f->width / 2 - (short)text_width(f->work) / 2, f->y);
    video_flip();
}

static int only_spaces(const Field *f)
{
    for (int i = 0; i < f->length; i++) {
        if (f->work[i] != ' ') return 0;
    }
    return 1;
}

static int rejected(int c, int free_text)
{
    /* % \ / | are never allowed; without free_text a wider set is refused. */
    if (c == '%' || c == '\\' || c == '/' || c == '|') return 1;
    return !free_text && strchr(" .*+[]:;\"<>,?", c) != NULL;
}

static int is_german_extra(int c)
{
    static const int EXTRA[] = { 0x81, 0x84, 0x94, 0xE1, 0xE6, 0x8E, 0x9A, 0x99, 0xF8, 0x15 };
    for (size_t i = 0; i < sizeof EXTRA / sizeof EXTRA[0]; i++) {
        if (EXTRA[i] == c) return 1;
    }
    return 0;
}

static void edit(Field *f, int c, int free_text)
{
    if (rejected(c, free_text)) return;
    if (c == 0x08 && f->length > 0) {
        f->work[--f->length] = '\0';
    } else if (c == KEY_DELETE && f->length > 0) {
        memset(f->work, 0, sizeof f->work);
        f->length = 0;
    } else if (f->length < f->max_length && text_width(f->work) <= MAX_TEXT_WIDTH) {
        const int printable = c >= 0x20 && c <= 0x7E;
        if (!printable && !(resource_german() && is_german_extra(c))) return;
        if (!free_text && c >= 'a' && c <= 'z') c -= 0x20;
        f->work[f->length++] = (char)c;
    }
}

static int next_char(void)
{
    const int c = input_next_char();
    if (c != 0) return c;
    const int buttons = pointer_read_buttons();
    if (buttons == POINTER_BUTTON_SELECT) return 0x0D;
    if (buttons == POINTER_BUTTON_BACK) return 0x1B;
    return 0;
}

static void blink(Field *f)
{
    f->blink_ticks += timer_ticks_since_last();
    if (f->blink_ticks > BLINK_TICKS) {
        f->cursor_on ^= 1;
        draw(f, f->cursor_on);
        f->blink_ticks = 0;
    }
}

static int run(Field *f, char *text, int free_text)
{
    for (;;) {
        /* TODO(0046417c, 0043d4c0): service music and the network. */
        int c = next_char();
        if (c == 0x0D && only_spaces(f)) c = 0;
        if (c == 0) {
            blink(f);
        } else if (c == 0x0D) {
            memcpy(text, f->work, (size_t)f->max_length + 1);
            return 1;
        } else if (c == 0x1B) {
            return 0;
        } else {
            edit(f, c, free_text);
            draw(f, 0);
            timer_ticks_since_last();
        }
    }
}

int text_field_edit(char *text, int max_length, int cx, int y, int free_text)
{
    Field f = { { 0 }, 0, max_length, 0, y, 0, NULL, 0, 0 };
    const int half = (max_length / 2) * 30 + 5;
    f.x0 = cx - half;
    f.width = 2 * half;
    snprintf(f.work, sizeof f.work, "%.*s", max_length, text);
    f.length = (int)strlen(f.work);
    f.saved = malloc((size_t)f.width * FIELD_HEIGHT);
    if (f.saved == NULL) return 0;

    while (input_get_key() != KEY_NONE || pointer_read_buttons() != POINTER_BUTTON_NONE) {
        /* wait for release; TODO(0046417c): service music */
    }
    while (input_next_char() != 0) {
        /* drop characters typed before the field opened */
    }
    video_read_rect(f.x0, f.y, f.width, FIELD_HEIGHT, f.saved);
    draw(&f, 1);
    timer_ticks_since_last();
    const int accepted = run(&f, text, free_text);
    video_write_rect(f.x0, f.y, f.width, FIELD_HEIGHT, f.saved);
    video_flip();
    free(f.saved);
    return accepted;
}

/* The key capture: a key waited for and its name written (004657d0). */
#ifndef TNFS_KEY_CAPTURE_H
#define TNFS_KEY_CAPTURE_H

#include <stdint.h>

typedef struct KeyCaptureOps {
    int32_t (*keys_pending)(void *ctx);  /* 00464bd0 */
    void (*music)(void *ctx);            /* 0046417c */
    int32_t (*pointer_poll)(void *ctx);  /* [004c6194]: the buttons */
    void (*layout_on)(void *ctx);        /* 004770a0, German only */
    int32_t (*ticks)(void *ctx);         /* 00473334 */
    void (*drain)(void *ctx);            /* 004770ac */
    int32_t (*key_read)(void *ctx);      /* 00476f94: 0 when none */
    void (*layout_off)(void *ctx);       /* 0047706c, German only */
    void *ctx;
} KeyCaptureOps;

typedef struct KeyCaptureState {
    int16_t pointer_active;  /* the short 00524790 */
    int16_t buttons;         /* the short 00524792: [004c6194]'s while the pointer is active */
    uint8_t shift_left, shift_right;  /* 004c6966, 004c6972 (the flags of scancodes 0x2a, 0x36): no key read */
    int16_t quit;            /* the short 0050a5b6: 1 ends the wait */
} KeyCaptureState;

/* 004657d0 (eax name; the language the short 0052b31c): the keys pending
 * drained (the music served, the pointer polled while active), then keys
 * read until one names itself: SPACE, ENTER, INSERT, DELETE, HOME, END, PGUP,
 * PGDN, UP, DOWN, LEFT, RIGHT, BCKSPC, TAB, CENTER, or a printable character
 * (a..z made capitals; in German also the umlauts, ä ö ü as Ä Ö Ü, and ß, but
 * not '`'). Some characters are never taken (TAB among them, so its name is
 * never reached). Escape, the right button (buttons 2) or quit 1 give 0;
 * otherwise the key, as named. In German a space or Backspace reads one more
 * key and drops it. name needs 7 bytes. */
int32_t key_capture(char *name, int german, KeyCaptureState *st, const KeyCaptureOps *ops);

#endif

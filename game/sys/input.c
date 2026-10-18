#include "game/sys/input.h"

#include "game/sys/pointer.h"
#include "game/sys/video.h"
#include "platform/platform.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    SC_CTRL = 0x1D,
    SC_ALT = 0x38,
    SC_F5 = 0x3F,
    SC_F10 = 0x44
};

/* 00464bd0's list after the Ctrl+F10 and F5 checks: scancode -> result. */
static const struct {
    int scancode;
    int key;
} PRIORITY[] = {
    { 0x01, KEY_ESCAPE }, { 0x39, KEY_SPACE }, { 0x1C, KEY_ENTER },
    { 0x48, KEY_UP }, { 0x50, KEY_DOWN }, { 0x4B, KEY_LEFT }, { 0x4D, KEY_RIGHT },
    { 0x52, KEY_INSERT }, { 0x53, KEY_DELETE },
    { 0x3B, KEY_F1 }, { 0x3C, KEY_F2 }, { 0x3D, KEY_F3 }, { 0x3E, KEY_F4 },
    { 0x2A, KEY_LSHIFT }, { 0x36, KEY_RSHIFT },
};

static void (*on_close)(void);

void input_on_close(void (*hook)(void)) { on_close = hook; }

void input_pump(void)
{
    if (!platform_poll_events()) {
        /* The original only quits through its menus; closing the window here
         * ends the process the way a fatal error (00477a40) would, after
         * input_on_close's hook. */
        if (on_close != NULL) on_close();
        video_shutdown();
        platform_shutdown();
        exit(EXIT_SUCCESS);
    }
    pointer_sync_from_os();
    /* The game's wait loops spin on this call; yield so the OS (and the
     * browser, under ASYNCIFY) keeps running. */
    platform_sleep_ms(1);
}

int input_get_key(void)
{
    input_pump();
    if (platform_key_down(SC_F10) && platform_key_down(SC_CTRL)) {
        return KEY_NONE;  /* TODO(network): Ctrl+F10 drops a net game (0052b314) */
    }
    if (platform_key_down(SC_F5)) {
        return KEY_NONE;  /* TODO(00464a68): F5 handler */
    }
    for (size_t i = 0; i < sizeof PRIORITY / sizeof PRIORITY[0]; i++) {
        if (platform_key_down(PRIORITY[i].scancode)) return PRIORITY[i].key;
    }
    /* TODO(00464bd0): the Ctrl/Alt/J tail is read from a partial decompile;
     * confirm the exact nesting against the disassembly. */
    if (platform_key_down(SC_CTRL)) return platform_key_down(SC_ALT) ? KEY_ALT : KEY_CTRL;
    if (platform_key_down(0x15)) return KEY_Y;
    if (platform_key_down(0x31)) return KEY_N;
    if (platform_key_down(0x24)) return KEY_J;
    return KEY_NONE;
}

/* German characters the name field accepts (004626d0), Unicode -> CP437. */
static int to_cp437(uint32_t cp)
{
    static const struct { uint32_t unicode; int cp437; } MAP[] = {
        { 0xFC, 0x81 }, { 0xE4, 0x84 }, { 0xF6, 0x94 }, { 0xDF, 0xE1 }, { 0xB5, 0xE6 },
        { 0xC4, 0x8E }, { 0xDC, 0x9A }, { 0xD6, 0x99 }, { 0xB0, 0xF8 }, { 0xA7, 0x15 },
    };
    if (cp >= 0x20 && cp < 0x7F) return (int)cp;
    for (size_t i = 0; i < sizeof MAP / sizeof MAP[0]; i++) {
        if (MAP[i].unicode == cp) return MAP[i].cp437;
    }
    return 0;
}

int input_next_char(void)
{
    input_pump();
    for (uint32_t cp; (cp = platform_next_text()) != 0;) {
        switch (cp) {
        case PLATFORM_TEXT_BACKSPACE:
        case PLATFORM_TEXT_ENTER:
        case PLATFORM_TEXT_ESCAPE:
            return (int)cp;
        case PLATFORM_TEXT_DELETE:
            return KEY_DELETE;
        default: {
            const int c = to_cp437(cp);
            if (c != 0) return c;
        }
        }
    }
    return 0;
}

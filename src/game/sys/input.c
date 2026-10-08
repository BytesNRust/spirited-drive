#include "game/sys/input.h"

#include "game/sys/pointer.h"
#include "game/sys/video.h"
#include "platform/platform.h"

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

static void pump_events(void)
{
    if (!platform_poll_events()) {
        /* The original only quits through its menus; closing the window here
         * ends the process the way a fatal error (00477a40) would. */
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
    pump_events();
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

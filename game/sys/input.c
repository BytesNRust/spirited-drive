#include "game/sys/input.h"

#include "game/sys/hot_key.h"
#include "game/sys/pointer.h"
#include "game/sys/video.h"
#include "platform/platform.h"

#include <stdint.h>
#include <stdlib.h>

enum { KEYS_READ = 0x80 };  /* 004c693c's flags, one a scancode */

/* 00464bd0's hooks: F5's 00464a68, the short 004f9e8c, the joystick (0043d4c0's) */
static void (*on_mute)(void);
static const int16_t *function_keys;
static void (*joystick)(JoyState **j, int32_t *present, int32_t *device, const JoyOps **ops);

static void mute(void *ctx)
{
    (void)ctx;
    if (on_mute != NULL) on_mute();
}

static void no_shot(void *ctx, const char *name) { (void)ctx, (void)name; }

static int32_t no_joy_read(void *ctx, int32_t device, uint32_t *buttons, int32_t axis[JOY_AXES])
{
    (void)ctx, (void)device, (void)buttons, (void)axis;
    return 0;
}

void input_on_keys(void (*mute_hook)(void),
                   void (*joystick_hook)(JoyState **j, int32_t *present, int32_t *device, const JoyOps **ops))
{
    on_mute = mute_hook, joystick = joystick_hook;
}

void input_function_keys(const int16_t *flag) { function_keys = flag; }

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
    uint8_t flags[KEY_FLAGS];
    for (int sc = 0; sc < KEYS_READ; sc++) flags[sc] = platform_key_down(sc) ? 1 : 0;
    static JoyState none;
    static const JoyOps NO_JOY = { NULL, no_joy_read };
    JoyState *j = &none;
    int32_t present = 0, device = 0;
    const JoyOps *joy_ops = &NO_JOY;
    if (joystick != NULL) joystick(&j, &present, &device, &joy_ops);
    /* Ctrl+F10's drop of a net game (0052b314) is the link's: TODO(network) */
    HotKeyState s = { 0 };
    return menu_hot_key(&s, flags, function_keys != NULL ? *function_keys : 0, j, present, device,
                        &(const HotKeyOps){ NULL, no_shot, mute }, joy_ops);
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

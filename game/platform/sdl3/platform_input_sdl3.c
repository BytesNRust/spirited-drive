/* The SDL3 platform's keys, text, mouse and cursor (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/sys/key_latch.h"
#include "game/sys/data_path.h"
#include "game/sys/file_url.h"
#include "game/sys/port_log.h"
#include "tnfs_version.h"
#include "platform/sdl3/input_script.h"
#include "platform/sdl3/input_shared.h"
#include "platform/sdl3/pad_keys.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <unistd.h>   /* fsync */
#include <limits.h>   /* realpath: symbolic links out of the mods directory are refused */
#define TNFS_HAVE_REALPATH 1
#endif
#include "platform/sdl3/platform_sdl3_private.h"

enum { KEY_QUEUE_SIZE = 16 };
static PlatformKey key_queue[KEY_QUEUE_SIZE];
static int key_head;
static int key_count;

enum { TEXT_QUEUE_SIZE = 32 };
static uint32_t text_queue[TEXT_QUEUE_SIZE];
static int text_head, text_count;

static float mouse_x, mouse_y;
static unsigned mouse_buttons;
static bool mouse_moved;

void sdl3_text_push(uint32_t code)
{
    if (text_count < TEXT_QUEUE_SIZE) text_queue[(text_head + text_count++) % TEXT_QUEUE_SIZE] = code;
}

void sdl3_text_push_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        uint32_t cp = *p;
        int extra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : cp >= 0xC0 ? 1 : 0;
        cp &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        p++;
        for (; extra > 0 && (*p & 0xC0) == 0x80; extra--, p++) cp = (cp << 6) | (*p & 0x3F);
        sdl3_text_push(cp);
    }
}

static void push_editing_key(SDL_Keycode key)
{
    if (key == SDLK_BACKSPACE) sdl3_text_push(PLATFORM_TEXT_BACKSPACE);
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) sdl3_text_push(PLATFORM_TEXT_ENTER);
    if (key == SDLK_ESCAPE) sdl3_text_push(PLATFORM_TEXT_ESCAPE);
    if (key == SDLK_DELETE) sdl3_text_push(PLATFORM_TEXT_DELETE);
}

void platform_text_input(bool on)
{
    if (window == NULL) return;
    if (on) SDL_StartTextInput(window);
    else SDL_StopTextInput(window);
}

bool platform_text_input_active(void)
{
    return window != NULL && SDL_TextInputActive(window);
}

uint32_t platform_next_text(void)
{
    if (input_script_active()) input_script_apply_text();
    if (text_count == 0) return 0;
    const uint32_t code = text_queue[text_head];
    text_head = (text_head + 1) % TEXT_QUEUE_SIZE;
    text_count--;
    return code;
}

/* The menus draw their own pointer and a race has none, so the system cursor is hidden over the window, in fullscreen
 * and windowed alike; it is shown again when the window loses the focus and when the game ends. */
void cursor_apply(void)
{
    if (window_focused) SDL_HideCursor();
    else SDL_ShowCursor();
}

bool platform_cursor_hidden(void)
{
    return window != NULL && !SDL_CursorVisible();
}

static PlatformKey translate_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_LEFT: return PLATFORM_KEY_LEFT;
    case SDLK_RIGHT: return PLATFORM_KEY_RIGHT;
    case SDLK_UP: return PLATFORM_KEY_UP;
    case SDLK_DOWN: return PLATFORM_KEY_DOWN;
    case SDLK_RETURN: return PLATFORM_KEY_ENTER;
    case SDLK_ESCAPE: return PLATFORM_KEY_ESCAPE;
    case SDLK_SPACE: return PLATFORM_KEY_SPACE;
    default: return PLATFORM_KEY_NONE;
    }
}

bool platform_poll_events(void)
{
    static bool quit;  /* the window closed: every caller after the first sees it too (a race's loop, then a screen's) */
    SDL_Event event;
    if (quit) return false;
    /* port: a binding's key held or let go while nothing reads the pad (the pause's wait, a race on the keyboard) */
    pad_keys_refresh();
    key_latch_pump();
    if (input_script_active()) input_script_latch_presses();
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            quit = true;
            return false;
        }
        input_script_log_window_event(event.type);
        input_script_watch_key_event(&event);
        if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) key_latch_clear(), input_script_watch_reset();
            window_focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
            cursor_apply();
            continue;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            SDL_ConvertEventToRenderCoordinates(renderer, &event);
            mouse_x = event.motion.x;
            mouse_y = event.motion.y;
            mouse_moved = true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            const unsigned bit = event.button.button == SDL_BUTTON_LEFT ? PLATFORM_MOUSE_LEFT
                : event.button.button == SDL_BUTTON_RIGHT ? PLATFORM_MOUSE_RIGHT : 0;
            mouse_buttons = event.button.down ? (mouse_buttons | bit) : (mouse_buttons & ~bit);
        }
        if (event.type == SDL_EVENT_TEXT_INPUT) sdl3_text_push_utf8(event.text.text);
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) key_latch_press((int)event.key.scancode);   /* port: a tap shorter than a poll still reaches the sim */
        if (event.type == SDL_EVENT_KEY_DOWN) push_editing_key(event.key.key);
        if (event.type == SDL_EVENT_KEY_DOWN && key_count < KEY_QUEUE_SIZE) {
            const PlatformKey key = translate_key(event.key.key);
            if (key != PLATFORM_KEY_NONE) key_queue[(key_head + key_count++) % KEY_QUEUE_SIZE] = key;
        }
    }
    input_script_watch_check();
    return true;
}

PlatformKey platform_next_key(void)
{
    if (input_script_active()) input_script_apply_keys();
    if (key_count == 0) return PLATFORM_KEY_NONE;
    const PlatformKey key = key_queue[key_head];
    key_head = (key_head + 1) % KEY_QUEUE_SIZE;
    key_count--;
    return key;
}

bool platform_set_clipboard_text(const char *text)
{
    return text != NULL && SDL_SetClipboardText(text);
}

bool platform_key_down(int scancode)
{
    if (input_script_active() && input_script_key_down(scancode)) return true;
    if (platform_joy_virtual_down(scancode)) return true;   /* port: a pad's binding of that key */
    const SDL_Scancode sc = sdl3_from_set1(scancode);
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    const bool *state = SDL_GetKeyboardState(NULL);
    if (key_latch_down((int)sc)) return true;   /* port: a key tapped since the last poll */
    /* Both Ctrl / Alt / Enter keys count, as on a PC keyboard. */
    if (sc == SDL_SCANCODE_LCTRL) return state[sc] || state[SDL_SCANCODE_RCTRL];
    if (sc == SDL_SCANCODE_LALT) return state[sc] || state[SDL_SCANCODE_RALT];
    if (sc == SDL_SCANCODE_RETURN) return state[sc] || state[SDL_SCANCODE_KP_ENTER];
    return state[sc];
}

/* the state the input script drives: its events enter the queues and the pointer as a real device's do (input_shared.h) */
void sdl3_key_push(PlatformKey key)
{
    if (key_count < KEY_QUEUE_SIZE) key_queue[(key_head + key_count++) % KEY_QUEUE_SIZE] = key;
}

void sdl3_mouse_place(float x, float y)
{
    mouse_x = x;
    mouse_y = y;
    mouse_moved = true;
}

void sdl3_mouse_buttons(unsigned press, unsigned release)
{
    mouse_buttons = (mouse_buttons | press) & ~release;
}

bool platform_mouse(int *x, int *y, unsigned *buttons)
{
    if (input_script_active()) input_script_apply_mouse();
    *x = (int)mouse_x;
    *y = (int)mouse_y;
    *buttons = mouse_buttons;
    const bool moved = mouse_moved;
    mouse_moved = false;
    return moved;
}

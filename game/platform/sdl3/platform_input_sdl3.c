/* The SDL3 platform's keys, text, mouse and cursor (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/platform/fullscreen_key.h"
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
static int wheel_notches;   /* port (dep59 showcase): the wheel since platform_mouse_wheel last took it */
static unsigned mouse_buttons;
#define MOUSE_LATCH_MS 250
static unsigned mouse_latch;      /* port: the buttons pressed since the last read, kept down until a read has seen them (a touchpad tap whose down and up come in one poll) */
static Uint64 mouse_latch_at;       /* when the latch was set: a press nobody read for MOUSE_LATCH_MS (a fade, a movie, a load) is let go, not fired as a ghost click on the next screen */
static bool mouse_latch_seen;     /* a read has seen the latch: the next poll of the events lets it go */
static unsigned mouse_new;        /* port: the buttons that went down since platform_mouse_take_presses last had them: every press, even one right after another that the state never showed up between */
static Uint64 mouse_new_at;       /* when the last of them went down (the poll that saw it): a press nobody took for MOUSE_LATCH_MS (a fade, a movie) is let go too */
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
    if (sdl3_window == NULL) return;
    if (on) SDL_StartTextInput(sdl3_window);
    else SDL_StopTextInput(sdl3_window);
}

bool platform_text_input_active(void)
{
    return sdl3_window != NULL && SDL_TextInputActive(sdl3_window);
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
void sdl3_cursor_apply(void)
{
    if (sdl3_window_focused) SDL_HideCursor();
    else SDL_ShowCursor();
}

bool platform_cursor_hidden(void)
{
    return sdl3_window != NULL && !SDL_CursorVisible();
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

/* the numbers game/platform/fullscreen_key.h writes are SDL's */
typedef char fullscreen_key_f_is_sdls[FULLSCREEN_SC_F == SDL_SCANCODE_F ? 1 : -1];
typedef char fullscreen_key_return_is_sdls[FULLSCREEN_SC_RETURN == SDL_SCANCODE_RETURN ? 1 : -1];
typedef char fullscreen_key_kp_enter_is_sdls[FULLSCREEN_SC_KP_ENTER == SDL_SCANCODE_KP_ENTER ? 1 : -1];

static void (*fullscreen_toggle)(void);

void platform_set_fullscreen_toggle(void (*toggle)(void)) { fullscreen_toggle = toggle; }

/* a key press's modifiers as fullscreen_key.h counts them */
static unsigned chord_mods(SDL_Keymod mod)
{
    return ((mod & SDL_KMOD_ALT) != 0 ? FULLSCREEN_MOD_ALT : 0u) | ((mod & SDL_KMOD_CTRL) != 0 ? FULLSCREEN_MOD_CTRL : 0u) |
           ((mod & SDL_KMOD_GUI) != 0 ? FULLSCREEN_MOD_GUI : 0u) | ((mod & SDL_KMOD_SHIFT) != 0 ? FULLSCREEN_MOD_SHIFT : 0u);
}

bool platform_poll_events(void)
{
    static bool quit;  /* the window closed: every caller after the first sees it too (a race's loop, then a screen's) */
    SDL_Event event;
    bool toggle_pending = false;
    if (quit) return false;
    /* port: a binding's key held or let go while nothing reads the pad (the pause's wait, a race on the keyboard) */
    pad_keys_refresh();
    key_latch_pump();
    if (mouse_latch_seen) mouse_latch = 0, mouse_latch_seen = false;   /* what was read once is let go: a press that came since stays until it is read */
    if (input_script_active()) input_script_latch_presses();
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            quit = true;
            return false;
        }
        input_script_log_window_event(event.type);
        input_script_watch_key_event(&event);
        if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) key_latch_clear(), mouse_latch = 0, mouse_latch_seen = false, mouse_new = 0, input_script_watch_reset();
            sdl3_window_focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
            sdl3_cursor_apply();
            continue;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            SDL_ConvertEventToRenderCoordinates(sdl3_renderer, &event);
            mouse_x = event.motion.x;
            mouse_y = event.motion.y;
            mouse_moved = true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            const unsigned bit = event.button.button == SDL_BUTTON_LEFT ? PLATFORM_MOUSE_LEFT
                : event.button.button == SDL_BUTTON_RIGHT ? PLATFORM_MOUSE_RIGHT : 0;
            mouse_buttons = event.button.down ? (mouse_buttons | bit) : (mouse_buttons & ~bit);
            if (event.button.down) mouse_latch |= bit, mouse_latch_at = SDL_GetTicks(), mouse_new |= bit, mouse_new_at = mouse_latch_at;   /* port: a tap shorter than a poll still reads as a press (as the keys' latch does) */
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL) wheel_notches += event.wheel.y > 0 ? 1 : event.wheel.y < 0 ? -1 : 0;
        if (event.type == SDL_EVENT_TEXT_INPUT) sdl3_text_push_utf8(event.text.text);
        if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != NULL) sdl3_drop_push(event.drop.data);   /* port (dep59 step 4b) */
#if defined(__APPLE__)
        const bool apple = true;
#else
        const bool apple = false;
#endif
        if (event.type == SDL_EVENT_KEY_DOWN && fullscreen_toggle != NULL && fullscreen_key_is_toggle((int)event.key.scancode, chord_mods(event.key.mod), apple)) {
            toggle_pending = toggle_pending || !event.key.repeat;   /* held, it toggles once; and it never reaches the game as an Enter or an F */
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) key_latch_press((int)event.key.scancode);   /* port: a tap shorter than a poll still reaches the sim */
        if (event.type == SDL_EVENT_KEY_DOWN) push_editing_key(event.key.key);
        if (event.type == SDL_EVENT_KEY_DOWN && key_count < KEY_QUEUE_SIZE) {
            const PlatformKey key = translate_key(event.key.key);
            if (key != PLATFORM_KEY_NONE) key_queue[(key_head + key_count++) % KEY_QUEUE_SIZE] = key;
        }
    }
    input_script_watch_check();
    if (toggle_pending && fullscreen_toggle != NULL) fullscreen_toggle();   /* outside the event loop: the window's change waits for its events */
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

int platform_mouse_wheel(void)
{
    const int notches = wheel_notches;
    wheel_notches = 0;
    return notches;
}

bool platform_mouse(int *x, int *y, unsigned *buttons)
{
    if (input_script_active()) input_script_apply_mouse();
    *x = (int)mouse_x;
    *y = (int)mouse_y;
    if (mouse_latch != 0 && SDL_GetTicks() - mouse_latch_at > MOUSE_LATCH_MS) mouse_latch = 0, mouse_latch_seen = false;
    *buttons = mouse_buttons | mouse_latch;
    if (mouse_latch != 0) mouse_latch_seen = true;
    const bool moved = mouse_moved;
    mouse_moved = false;
    return moved;
}

unsigned platform_mouse_take_presses(void)
{
    const unsigned fresh = mouse_new != 0 && SDL_GetTicks() - mouse_new_at <= MOUSE_LATCH_MS ? mouse_new : 0;
    mouse_new = 0;
    return fresh;
}

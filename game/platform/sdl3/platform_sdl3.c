#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/sys/port_log.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <limits.h>   /* realpath: symbolic links out of the mods directory are refused */
#define TNFS_HAVE_REALPATH 1
#endif

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t *rgb_buffer;
static int fb_width;
static int fb_height;

enum { KEY_QUEUE_SIZE = 16, VBLANK_HZ = 60 };
static int refresh_hz = VBLANK_HZ;

/* TNFS_CAPTURE_DIR=dir [TNFS_CAPTURE_EVERY=n] [TNFS_CAPTURE_FROM=m]: write every n-th presented
 * frame from the m-th on as dir/frame_NNNNN.ppm, to check the game's screens headless. */
static const char *capture_dir;
static int capture_every = 30;
static unsigned capture_from;
static unsigned presented_frames;
static uint64_t next_vblank_ns;
static PlatformKey key_queue[KEY_QUEUE_SIZE];
static int key_head;
static int key_count;

/* TNFS_INPUT_SCRIPT="ms:KEY,ms:MOUSE:x:y,...": scripted input for headless
 * runs. KEY (UP, DOWN, LEFT, RIGHT, ENTER, ESC, SPACE, a letter, a digit, F1..F12, CTRL, ALT, SHIFT, KP1;
 * several at one ms are held together) is held for 100 ms
 * starting ms milliseconds after platform_init, and longer if the game has
 * not polled it yet (slow frame capture must not drop presses); MOUSE moves
 * the pointer; ms:CLICK:x:y moves it there and, from the next read on (so the move is taken first), holds the
 * left button 100 ms;
 * ms:TEXT:abc types abc (ASCII). ENTER and ESC also reach the
 * typed-text queue, and the arrows, ENTER, ESC and SPACE the key queue (platform_next_key).
 * TNFS_INPUT_SETTLE=N: the events one after another, each ms after the last one is done (a key let go, a click
 * read, text queued), and a click or a mouse move only once no frame has been presented for N ms and the pointer
 * has been read all that time with no gap over SCRIPT_READ_GAP_MS (the menus present only on a change and poll
 * the pointer while they wait, so that is a screen waiting for its input, however slow the build runs; a load
 * presents nothing either, but reads no pointer) or, on a screen that never stills (a movie, a blinking
 * cursor), after SCRIPT_SETTLE_CAP_MS. */
enum { SCRIPT_MAX = 64, SCRIPT_HOLD_MS = 100, SCRIPT_SETTLE_CAP_MS = 12000, SCRIPT_READ_GAP_MS = 250 };
typedef struct {
    uint32_t at_ms;
    int scancode;  /* set-1, 0 for mouse moves, -2 text, -3 clicks */
    int x, y;
    bool seen;     /* held until the game has read it at least once */
    bool queued;   /* text / editing code already pushed to the text queue */
    bool keyed;    /* pushed to the key queue, as a real press is */
    char text[16]; /* TEXT events */
    uint32_t delay; /* TNFS_INPUT_SETTLE: ms after the event before it */
    bool placed;    /* CLICK: the pointer moved there, the press due on the next read */
} ScriptEvent;
static ScriptEvent script[SCRIPT_MAX];
static int script_count;
static uint32_t script_start_ms;
static uint32_t settle_ms, last_present_ms, script_last_ms;  /* TNFS_INPUT_SETTLE */
static uint32_t last_read_ms, reading_since_ms;               /* the pointer read with no gap since */
static int script_next;

static void load_script(void);

enum { TEXT_QUEUE_SIZE = 32 };
static uint32_t text_queue[TEXT_QUEUE_SIZE];
static int text_head, text_count;

static void push_text(uint32_t code)
{
    if (text_count < TEXT_QUEUE_SIZE) text_queue[(text_head + text_count++) % TEXT_QUEUE_SIZE] = code;
}

static void push_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        uint32_t cp = *p;
        int extra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : cp >= 0xC0 ? 1 : 0;
        cp &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        p++;
        for (; extra > 0 && (*p & 0xC0) == 0x80; extra--, p++) cp = (cp << 6) | (*p & 0x3F);
        push_text(cp);
    }
}

static void push_editing_key(SDL_Keycode key)
{
    if (key == SDLK_BACKSPACE) push_text(PLATFORM_TEXT_BACKSPACE);
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) push_text(PLATFORM_TEXT_ENTER);
    if (key == SDLK_ESCAPE) push_text(PLATFORM_TEXT_ESCAPE);
    if (key == SDLK_DELETE) push_text(PLATFORM_TEXT_DELETE);
}

static void apply_script_text(void);

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
    if (script_count > 0) apply_script_text();
    if (text_count == 0) return 0;
    const uint32_t code = text_queue[text_head];
    text_head = (text_head + 1) % TEXT_QUEUE_SIZE;
    text_count--;
    return code;
}

static float mouse_x, mouse_y;
static unsigned mouse_buttons;
static bool mouse_moved;

static bool window_focused;  /* the window has the keyboard focus: the system cursor is hidden over it then */

/* The menus draw their own pointer and a race has none, so the system cursor is hidden over the window, in fullscreen
 * and windowed alike; it is shown again when the window loses the focus and when the game ends. */
static void cursor_apply(void)
{
    if (window_focused) SDL_HideCursor();
    else SDL_ShowCursor();
}

bool platform_cursor_hidden(void)
{
    return window != NULL && !SDL_CursorVisible();
}

bool platform_init(const PlatformConfig *config)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        port_fatal("platform: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    fb_width = config->width;
    fb_height = config->height;
    refresh_hz = config->refresh_hz > 0 ? config->refresh_hz : VBLANK_HZ;
    next_vblank_ns = 0;

    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (config->fullscreen != 0 ? SDL_WINDOW_FULLSCREEN : 0);
    if (!SDL_CreateWindowAndRenderer(config->title, fb_width, fb_height, flags, &window, &renderer)) {
        port_fatal("platform: window creation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetRenderLogicalPresentation(renderer, fb_width, fb_height,
                                     config->scale_integer != 0 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                                : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    load_script();
    window_focused = true;
    cursor_apply();
    SDL_StartTextInput(window);
    capture_dir = SDL_getenv("TNFS_CAPTURE_DIR");
    const char *every = SDL_getenv("TNFS_CAPTURE_EVERY");
    if (every != NULL && SDL_atoi(every) > 0) capture_every = SDL_atoi(every);
    const char *from = SDL_getenv("TNFS_CAPTURE_FROM");
    if (from != NULL && SDL_atoi(from) > 0) capture_from = (unsigned)SDL_atoi(from);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, fb_width, fb_height);
    rgb_buffer = malloc((size_t)fb_width * (size_t)fb_height * sizeof *rgb_buffer);
    if (texture == NULL || rgb_buffer == NULL) {
        port_fatal("platform: framebuffer allocation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return true;
}

bool platform_reconfigure(const PlatformConfig *config)
{
    if (window == NULL || renderer == NULL || config->width <= 0 || config->height <= 0) return false;
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, config->width,
                                       config->height);
    uint32_t *rgb = malloc((size_t)config->width * (size_t)config->height * sizeof *rgb);
    if (t == NULL || rgb == NULL) {
        port_log("platform: framebuffer reallocation failed: %s\n", SDL_GetError());
        if (t != NULL) SDL_DestroyTexture(t);
        free(rgb);
        return false;
    }
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_DestroyTexture(texture);
    free(rgb_buffer);
    texture = t, rgb_buffer = rgb;
    fb_width = config->width, fb_height = config->height;
    refresh_hz = config->refresh_hz > 0 ? config->refresh_hz : VBLANK_HZ;
    next_vblank_ns = 0;
    SDL_SetWindowFullscreen(window, config->fullscreen != 0);
    cursor_apply();   /* the mode change may have brought the system's back */
    if (config->fullscreen == 0) {
        int w = 0, h = 0;
        if (SDL_GetWindowSize(window, &w, &h) && h > 0) SDL_SetWindowSize(window, h * fb_width / fb_height, h);
    }
    SDL_SetRenderLogicalPresentation(renderer, fb_width, fb_height,
                                     config->scale_integer != 0 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                                : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    return true;
}

void platform_framebuffer_size(int *width, int *height)
{
    *width = fb_width;
    *height = fb_height;
}

bool platform_desktop_size(int *width, int *height)
{
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = display != 0 ? SDL_GetDesktopDisplayMode(display) : NULL;
    if (mode == NULL || mode->w <= 0 || mode->h <= 0) return false;
    *width = mode->w;
    *height = mode->h;
    return true;
}

uint64_t platform_refresh_period_ns(void)
{
    return SDL_NS_PER_SECOND / (uint64_t)refresh_hz;
}

static SDL_AudioStream *audio;
static PlatformAudioFill audio_fill;
static void *audio_user;

enum { AUDIO_FRAME = 4, AUDIO_CHUNK = 0x200 };  /* bytes per 16-bit stereo frame; frames per fill */

/* the device's thread, the stream locked: what it asks for, filled a chunk at a time */
static void SDLCALL audio_get(void *user, SDL_AudioStream *stream, int additional, int total)
{
    (void)user, (void)total;
    static int16_t chunk[2 * AUDIO_CHUNK];
    for (int left = additional / AUDIO_FRAME; left > 0; left -= AUDIO_CHUNK) {
        const uint32_t n = left < AUDIO_CHUNK ? (uint32_t)left : AUDIO_CHUNK;
        audio_fill(audio_user, chunk, n);
        if (!SDL_PutAudioStreamData(stream, chunk, (int)(n * AUDIO_FRAME))) {
            port_log("platform: audio: %s\n", SDL_GetError());
            return;
        }
    }
}

bool platform_audio_open(int rate, PlatformAudioFill fill, void *user)
{
    if (audio != NULL) return true;
    if (fill == NULL) return false;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        port_log("platform: no audio: %s\n", SDL_GetError());
        return false;
    }
    audio_fill = fill, audio_user = user;
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, rate };
    audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_get, NULL);
    if (audio == NULL || !SDL_ResumeAudioStreamDevice(audio)) {
        port_log("platform: no audio device: %s\n", SDL_GetError());
        platform_audio_close();
        return false;
    }
    return true;
}

void platform_audio_close(void)
{
    if (audio != NULL) SDL_DestroyAudioStream(audio);
    audio = NULL;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void platform_audio_lock(void)
{
    if (audio != NULL) SDL_LockAudioStream(audio);
}

void platform_audio_unlock(void)
{
    if (audio != NULL) SDL_UnlockAudioStream(audio);
}

void platform_set_window_icon(const uint8_t *rgba, int w, int h)
{
    if (window == NULL || rgba == NULL || w <= 0 || h <= 0) return;
    SDL_Surface *const icon = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, (void *)(uintptr_t)rgba, w * 4);
    if (icon == NULL) return;
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

#ifndef __EMSCRIPTEN__
static void pool_stop(void);
#endif

void platform_shutdown(void)
{
#ifndef __EMSCRIPTEN__
    pool_stop();
#endif
    if (window != NULL) SDL_ShowCursor();
    window_focused = false;
    if (audio != NULL) platform_audio_close();
    free(rgb_buffer);
    rgb_buffer = NULL;
    if (texture != NULL) SDL_DestroyTexture(texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    texture = NULL;
    renderer = NULL;
    window = NULL;
    SDL_Quit();
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

static void (*joy_refresh)(void);  /* port: reads the open pads' keys again (platform_joy_virtual_refresher) */

bool platform_poll_events(void)
{
    static bool quit;  /* the window closed: every caller after the first sees it too (a race's loop, then a screen's) */
    SDL_Event event;
    if (quit) return false;
    /* port: a binding's key held or let go while nothing reads the pad (the pause's wait, a race on the keyboard) */
    if (joy_refresh != NULL) joy_refresh();
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            quit = true;
            return false;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
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
        if (event.type == SDL_EVENT_TEXT_INPUT) push_utf8(event.text.text);
        if (event.type == SDL_EVENT_KEY_DOWN) push_editing_key(event.key.key);
        if (event.type == SDL_EVENT_KEY_DOWN && key_count < KEY_QUEUE_SIZE) {
            const PlatformKey key = translate_key(event.key.key);
            if (key != PLATFORM_KEY_NONE) key_queue[(key_head + key_count++) % KEY_QUEUE_SIZE] = key;
        }
    }
    return true;
}

static void apply_script_keys(void)
{
    static const struct { int scancode; PlatformKey key; } KEYS[] = {
        { 0x4B, PLATFORM_KEY_LEFT }, { 0x4D, PLATFORM_KEY_RIGHT }, { 0x48, PLATFORM_KEY_UP }, { 0x50, PLATFORM_KEY_DOWN },
        { 0x1C, PLATFORM_KEY_ENTER }, { 0x01, PLATFORM_KEY_ESCAPE }, { 0x39, PLATFORM_KEY_SPACE }
    };
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    for (int i = 0; i < script_count; i++) {
        ScriptEvent *e = &script[i];
        if (e->keyed || now < e->at_ms) continue;
        for (size_t k = 0; k < sizeof KEYS / sizeof KEYS[0]; k++)
            if (KEYS[k].scancode == e->scancode && key_count < KEY_QUEUE_SIZE)
                key_queue[(key_head + key_count++) % KEY_QUEUE_SIZE] = KEYS[k].key;
        e->keyed = true;
    }
}

PlatformKey platform_next_key(void)
{
    if (script_count > 0) apply_script_keys();
    if (key_count == 0) return PLATFORM_KEY_NONE;
    const PlatformKey key = key_queue[key_head];
    key_head = (key_head + 1) % KEY_QUEUE_SIZE;
    key_count--;
    return key;
}

static int script_key(const char *name)
{
    /* port: the page keys, so a page walk can be scripted by keyboard: PGUP and PGDN are the set-1 scancodes the
     * BIOS codes 0x4900 and 0x5100 are read from (page_keys.c), and HOME and END the ones the exe's own key names
     * use (key_capture.c). END is the code KP1 already had: the port's set-1 codes carry no extended prefix, so the
     * keypad's 1 and End are one code here. */
    static const struct { const char *name; int scancode; } KEYS[] = {
        { "UP", 0x48 }, { "DOWN", 0x50 }, { "LEFT", 0x4B }, { "RIGHT", 0x4D },
        { "ENTER", 0x1C }, { "ESC", 0x01 }, { "SPACE", 0x39 },
        { "F1", 0x3B }, { "F2", 0x3C }, { "F3", 0x3D }, { "F4", 0x3E }, { "F5", 0x3F }, { "F6", 0x40 },
        { "F7", 0x41 }, { "F8", 0x42 }, { "F9", 0x43 }, { "F10", 0x44 }, { "F11", 0x57 }, { "F12", 0x58 },
        { "CTRL", 0x1D }, { "ALT", 0x38 }, { "SHIFT", 0x2A },
        { "KP1", 0x4F },
        { "PGUP", 0x49 }, { "PGDN", 0x51 }, { "HOME", 0x47 }, { "END", 0x4F },
    };
    /* the letters' set-1 scancodes, A..Z */
    static const uint8_t LETTERS[26] = { 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                         0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C };
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
        if (SDL_strcmp(name, KEYS[i].name) == 0) return KEYS[i].scancode;
    }
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0') return LETTERS[name[0] - 'A'];
    if (name[0] >= '1' && name[0] <= '9' && name[1] == '\0') return 0x02 + (name[0] - '1');  /* the digits' row */
    if (name[0] == '0' && name[1] == '\0') return 0x0B;
    return -1;
}

static void load_script(void)
{
    const char *text = SDL_getenv("TNFS_INPUT_SCRIPT");
    char buf[2048];
    if (text == NULL) return;
    SDL_strlcpy(buf, text, sizeof buf);
    char *save = NULL;
    for (char *item = SDL_strtok_r(buf, ",", &save); item != NULL && script_count < SCRIPT_MAX;
         item = SDL_strtok_r(NULL, ",", &save)) {
        ScriptEvent e = { 0, 0, 0, 0, false, false, false, "", 0, false };
        char name[16] = "";
        if (SDL_sscanf(item, "%u:MOUSE:%d:%d", &e.at_ms, &e.x, &e.y) == 3) {
            script[script_count++] = e;
        } else if (SDL_sscanf(item, "%u:CLICK:%d:%d", &e.at_ms, &e.x, &e.y) == 3) {
            e.scancode = -3;
            script[script_count++] = e;
        } else if (SDL_sscanf(item, "%u:TEXT:%15s", &e.at_ms, e.text) == 2) {
            e.scancode = -2;
            script[script_count++] = e;
        } else if (SDL_sscanf(item, "%u:%15s", &e.at_ms, name) == 2 && (e.scancode = script_key(name)) > 0) {
            script[script_count++] = e;
        } else {
            port_log("platform: bad TNFS_INPUT_SCRIPT item '%s'\n", item);
        }
    }
    script_start_ms = (uint32_t)SDL_GetTicks();
    const char *settle = SDL_getenv("TNFS_INPUT_SETTLE");
    settle_ms = settle != NULL ? (uint32_t)SDL_atoi(settle) : 0;
    if (settle_ms == 0) return;
    for (int i = 0; i < script_count; i++) script[i].delay = script[i].at_ms, script[i].at_ms = UINT32_MAX;
}

static bool script_done(const ScriptEvent *e, uint32_t now)
{
    if (e->scancode == -1) return true;                       /* a mouse move, consumed */
    if (e->scancode == -3 || e->scancode == -2) return e->seen || (e->scancode == -2 && e->queued);
    return now >= e->at_ms + SCRIPT_HOLD_MS && (e->seen || e->keyed || e->queued);
}

/* TNFS_INPUT_SETTLE: the next event armed (its at_ms now) once the one before is done and, for the pointer's, the
 * screen has been still for settle_ms */
static void script_sequence(void)
{
    if (settle_ms == 0) return;
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    while (script_next < script_count) {
        ScriptEvent *e = &script[script_next];
        if (e->at_ms != UINT32_MAX) {
            if (!script_done(e, now)) return;
            script_next++, script_last_ms = now;
            continue;
        }
        const bool pointer = e->scancode == 0 || e->scancode == -3;
        const uint32_t due = script_last_ms + e->delay;
        const bool still = now - last_present_ms >= settle_ms && now - reading_since_ms >= settle_ms;
        if (now < due || (pointer && !still && now < due + SCRIPT_SETTLE_CAP_MS)) return;
        e->at_ms = now;
        return;
    }
}

static bool script_key_down(int scancode)
{
    script_sequence();
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    for (int i = 0; i < script_count; i++) {
        ScriptEvent *e = &script[i];
        if (e->scancode == scancode && now >= e->at_ms && (!e->seen || now < e->at_ms + SCRIPT_HOLD_MS)) {
            e->seen = true;
            return true;
        }
    }
    return false;
}

static void apply_script_text(void)
{
    script_sequence();
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    for (int i = 0; i < script_count; i++) {
        ScriptEvent *e = &script[i];
        if (e->queued || e->scancode == -3 || now < e->at_ms) continue;
        if (e->scancode == -2) push_utf8(e->text);
        if (e->scancode == 0x1C) push_text(PLATFORM_TEXT_ENTER);
        if (e->scancode == 0x01) push_text(PLATFORM_TEXT_ESCAPE);
        e->queued = true;
    }
}

static void apply_script_mouse(void)
{
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    if (now - last_read_ms > SCRIPT_READ_GAP_MS) reading_since_ms = now;
    last_read_ms = now;
    script_sequence();
    for (int i = 0; i < script_count; i++) {
        if (script[i].scancode == 0 && now >= script[i].at_ms) {
            mouse_x = (float)script[i].x;
            mouse_y = (float)script[i].y;
            mouse_moved = true;
            script[i].scancode = -1;  /* consumed */
        }
        if (script[i].scancode == -3 && now >= script[i].at_ms && !script[i].queued && !script[i].placed) {
            mouse_x = (float)script[i].x;  /* moved first: a game reading the button before the move would press */
            mouse_y = (float)script[i].y;  /* where its pointer was */
            mouse_moved = true;
            script[i].placed = true;
        } else if (script[i].scancode == -3 && script[i].placed && !script[i].queued) {  /* pressed */
            mouse_buttons |= PLATFORM_MOUSE_LEFT;
            script[i].queued = true;
            script[i].at_ms = now;  /* held from the press */
        } else if (script[i].scancode == -3 && script[i].queued && !script[i].seen &&
                   now >= script[i].at_ms + SCRIPT_HOLD_MS) {  /* released */
            mouse_buttons &= ~(unsigned)PLATFORM_MOUSE_LEFT;
            script[i].seen = true;
        }
    }
}

static SDL_Scancode from_set1(int scancode)
{
    switch (scancode) {
    case 0x01: return SDL_SCANCODE_ESCAPE;
    case 0x02: return SDL_SCANCODE_1;
    case 0x03: return SDL_SCANCODE_2;
    case 0x04: return SDL_SCANCODE_3;
    case 0x05: return SDL_SCANCODE_4;
    case 0x06: return SDL_SCANCODE_5;
    case 0x07: return SDL_SCANCODE_6;
    case 0x08: return SDL_SCANCODE_7;
    case 0x09: return SDL_SCANCODE_8;
    case 0x0A: return SDL_SCANCODE_9;
    case 0x0B: return SDL_SCANCODE_0;
    case 0x0C: return SDL_SCANCODE_MINUS;
    case 0x0D: return SDL_SCANCODE_EQUALS;
    case 0x0E: return SDL_SCANCODE_BACKSPACE;
    case 0x0F: return SDL_SCANCODE_TAB;
    case 0x10: return SDL_SCANCODE_Q;
    case 0x11: return SDL_SCANCODE_W;
    case 0x12: return SDL_SCANCODE_E;
    case 0x13: return SDL_SCANCODE_R;
    case 0x14: return SDL_SCANCODE_T;
    case 0x15: return SDL_SCANCODE_Y;
    case 0x16: return SDL_SCANCODE_U;
    case 0x17: return SDL_SCANCODE_I;
    case 0x18: return SDL_SCANCODE_O;
    case 0x19: return SDL_SCANCODE_P;
    case 0x1A: return SDL_SCANCODE_LEFTBRACKET;
    case 0x1B: return SDL_SCANCODE_RIGHTBRACKET;
    case 0x1C: return SDL_SCANCODE_RETURN;
    case 0x1D: return SDL_SCANCODE_LCTRL;
    case 0x1E: return SDL_SCANCODE_A;
    case 0x1F: return SDL_SCANCODE_S;
    case 0x20: return SDL_SCANCODE_D;
    case 0x21: return SDL_SCANCODE_F;
    case 0x22: return SDL_SCANCODE_G;
    case 0x23: return SDL_SCANCODE_H;
    case 0x24: return SDL_SCANCODE_J;
    case 0x25: return SDL_SCANCODE_K;
    case 0x26: return SDL_SCANCODE_L;
    case 0x27: return SDL_SCANCODE_SEMICOLON;
    case 0x28: return SDL_SCANCODE_APOSTROPHE;
    case 0x29: return SDL_SCANCODE_GRAVE;
    case 0x2A: return SDL_SCANCODE_LSHIFT;
    case 0x2B: return SDL_SCANCODE_BACKSLASH;
    case 0x2C: return SDL_SCANCODE_Z;
    case 0x2D: return SDL_SCANCODE_X;
    case 0x2E: return SDL_SCANCODE_C;
    case 0x2F: return SDL_SCANCODE_V;
    case 0x30: return SDL_SCANCODE_B;
    case 0x31: return SDL_SCANCODE_N;
    case 0x32: return SDL_SCANCODE_M;
    case 0x33: return SDL_SCANCODE_COMMA;
    case 0x34: return SDL_SCANCODE_PERIOD;
    case 0x35: return SDL_SCANCODE_SLASH;
    case 0x36: return SDL_SCANCODE_RSHIFT;
    case 0x37: return SDL_SCANCODE_KP_MULTIPLY;
    case 0x38: return SDL_SCANCODE_LALT;
    case 0x39: return SDL_SCANCODE_SPACE;
    case 0x3A: return SDL_SCANCODE_CAPSLOCK;
    case 0x3B: return SDL_SCANCODE_F1;
    case 0x3C: return SDL_SCANCODE_F2;
    case 0x3D: return SDL_SCANCODE_F3;
    case 0x3E: return SDL_SCANCODE_F4;
    case 0x3F: return SDL_SCANCODE_F5;
    case 0x40: return SDL_SCANCODE_F6;
    case 0x41: return SDL_SCANCODE_F7;
    case 0x42: return SDL_SCANCODE_F8;
    case 0x43: return SDL_SCANCODE_F9;
    case 0x44: return SDL_SCANCODE_F10;
    case 0x45: return SDL_SCANCODE_NUMLOCKCLEAR;
    case 0x46: return SDL_SCANCODE_SCROLLLOCK;
    case 0x47: return SDL_SCANCODE_HOME;
    case 0x48: return SDL_SCANCODE_UP;
    case 0x49: return SDL_SCANCODE_PAGEUP;
    case 0x4A: return SDL_SCANCODE_KP_MINUS;
    case 0x4B: return SDL_SCANCODE_LEFT;
    case 0x4D: return SDL_SCANCODE_RIGHT;
    case 0x4E: return SDL_SCANCODE_KP_PLUS;
    case 0x4F: return SDL_SCANCODE_END;
    case 0x50: return SDL_SCANCODE_DOWN;
    case 0x51: return SDL_SCANCODE_PAGEDOWN;
    case 0x52: return SDL_SCANCODE_INSERT;
    case 0x53: return SDL_SCANCODE_DELETE;
    case 0x57: return SDL_SCANCODE_F11;
    case 0x58: return SDL_SCANCODE_F12;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

int platform_key_sdl(int scancode) { return (int)from_set1(scancode); }

int platform_script_key(const char *name) { return script_key(name); }

bool platform_key_name(int scancode, char *out, size_t size)
{
    if (size == 0) return false;
    out[0] = '\0';
    const SDL_Scancode sc = from_set1(scancode);
    const char *name = sc != SDL_SCANCODE_UNKNOWN ? SDL_GetScancodeName(sc) : NULL;
    if (name == NULL || name[0] == '\0') return false;
    size_t n = 0;
    for (; name[n] != '\0' && n + 1 < size; n++) out[n] = (char)SDL_toupper((unsigned char)name[n]);
    out[n] = '\0';
    return true;
}

/* port: the keys a pad's or wheel's bindings press (platform.h): what each device holds down as of its last read, and
 * the scancode each action's key is */
enum { VIRTUAL_DEVICES = 16 };
static uint32_t virtual_keys[VIRTUAL_DEVICES];
static int virtual_scancode[ACT_COUNT] = { [ACT_PAUSE] = 0x19, [ACT_VIEW] = 0x3b };
void platform_joy_virtual_refresher(void (*refresh)(void)) { joy_refresh = refresh; }

void platform_virtual_key(int action, int scancode)
{
    if (action >= 0 && action < ACT_COUNT) virtual_scancode[action] = scancode;
}

void platform_joy_virtual_set(int device, uint32_t keys)
{
    if (device >= 0 && device < VIRTUAL_DEVICES) virtual_keys[device] = keys;
}

bool platform_joy_virtual_down(int scancode)
{
    if (scancode == 0) return false;
    for (int action = 0; action < ACT_COUNT; action++) {
        if (virtual_scancode[action] != scancode) continue;
        for (int d = 0; d < VIRTUAL_DEVICES; d++)
            if ((virtual_keys[d] >> action & 1u) != 0) return true;
    }
    return false;
}

bool platform_key_down(int scancode)
{
    if (script_count > 0 && script_key_down(scancode)) return true;
    if (platform_joy_virtual_down(scancode)) return true;   /* port: a pad's binding of that key */
    const SDL_Scancode sc = from_set1(scancode);
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    const bool *state = SDL_GetKeyboardState(NULL);
    /* Both Ctrl / Alt / Enter keys count, as on a PC keyboard. */
    if (sc == SDL_SCANCODE_LCTRL) return state[sc] || state[SDL_SCANCODE_RCTRL];
    if (sc == SDL_SCANCODE_LALT) return state[sc] || state[SDL_SCANCODE_RALT];
    if (sc == SDL_SCANCODE_RETURN) return state[sc] || state[SDL_SCANCODE_KP_ENTER];
    return state[sc];
}

bool platform_mouse(int *x, int *y, unsigned *buttons)
{
    if (script_count > 0) apply_script_mouse();
    *x = (int)mouse_x;
    *y = (int)mouse_y;
    *buttons = mouse_buttons;
    const bool moved = mouse_moved;
    mouse_moved = false;
    return moved;
}

/* ---- mods overlay: the rules are in formats/modpath.c, the validation in formats/validate.c */

static char mods_data_root[1024];
static char mods_dir_path[1024];
static char mods_real_path[1100];
static char mods_error[1024];

bool platform_set_mods(const char *data_root, const char *mods_dir)
{
    mods_error[0] = '\0';
    if (mods_dir == NULL || mods_dir[0] == '\0') { mods_dir_path[0] = '\0'; return true; }
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(mods_dir, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
        SDL_snprintf(mods_error, sizeof mods_error, "mods directory %s does not exist", mods_dir);
        port_fatal("platform: %s\n", mods_error);
        return false;
    }
    SDL_snprintf(mods_data_root, sizeof mods_data_root, "%s", data_root);
    SDL_snprintf(mods_dir_path, sizeof mods_dir_path, "%s", mods_dir);
#ifdef TNFS_HAVE_REALPATH
    char real[PATH_MAX];
    if (realpath(mods_dir, real) == NULL) { port_fatal("platform: cannot resolve %s\n", mods_dir); mods_dir_path[0] = '\0'; return false; }
    SDL_snprintf(mods_real_path, sizeof mods_real_path, "%s", real);
#else
    SDL_snprintf(mods_real_path, sizeof mods_real_path, "%s", mods_dir);
#endif
    return true;
}

const char *platform_mods_error(void)
{
    return mods_error;
}

/* The mod's copy of `path`, validated; *refused is set when there is a copy the game must not get. */
static uint8_t *read_mod_file(const char *path, size_t *size, bool *refused)
{
    char rel[512], candidate[1600], upper[512];
    *refused = false;
    if (!modpath_relative(mods_data_root, path, rel, sizeof rel)) return NULL;
    if (rel[0] == '\0') return NULL;
    const char *names[2] = { rel, upper };
    if (!modpath_upper(rel, upper, sizeof upper)) return NULL;
    for (int k = 0; k < 2; ++k) {
        if (!modpath_join(mods_dir_path, names[k], candidate, sizeof candidate)) continue;
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(candidate, &info) || info.type != SDL_PATHTYPE_FILE) continue;
#ifdef TNFS_HAVE_REALPATH
        char real[PATH_MAX];
        if (realpath(candidate, real) == NULL || !modpath_inside(mods_real_path, real)) {
            SDL_snprintf(mods_error, sizeof mods_error, "mod file %s leaves the mods directory (symbolic link)", names[k]);
            port_fatal("platform: %s\n", mods_error);
            *refused = true;
            return NULL;
        }
#endif
        uint8_t *data = SDL_LoadFile(candidate, size);
        if (data == NULL) {
            SDL_snprintf(mods_error, sizeof mods_error, "mod file %s cannot be read: %s", names[k], SDL_GetError());
            port_fatal("platform: %s\n", mods_error);
            *refused = true;
            return NULL;
        }
        Report report;
        report_init(&report);
        validate_file(data, *size, names[k], &report);
        if (report_fatal_count(&report) > 0) {
            char text[700];
            report_text(&report, VALIDATE_COMPATIBLE, text, sizeof text);
            SDL_snprintf(mods_error, sizeof mods_error, "mod file %s is not valid for the original game: %s", names[k], text);
            port_fatal("platform: %s\n", mods_error);
            SDL_free(data);
            report_free(&report);
            *refused = true;
            return NULL;
        }
        report_free(&report);
        return data;
    }
    return NULL;
}

uint8_t *platform_read_file(const char *path, size_t *size)
{
    if (mods_dir_path[0] != '\0') {
        bool refused;
        uint8_t *mod = read_mod_file(path, size, &refused);
        if (mod != NULL || refused) return mod;
    }
    uint8_t *data = SDL_LoadFile(path, size);
    if (data == NULL) port_log("platform: cannot read %s: %s\n", path, SDL_GetError());
    return data;
}

void platform_free_file(uint8_t *data)
{
    SDL_free(data);
}

/* The writable directory of the game, from SDL_GetPrefPath; NULL when the
 * platform has none. */
static const char *user_dir(char *out, size_t out_size)
{
    static char *pref;
    if (mods_dir_path[0] != '\0') {   /* with a mods directory the player's files stay beside the mod */
        SDL_snprintf(out, out_size, "%s/user/", mods_dir_path);
        SDL_CreateDirectory(out);
        return out;
    }
    if (pref == NULL) pref = SDL_GetPrefPath("tnfs", "tnfs");
    if (pref == NULL) return NULL;
    SDL_snprintf(out, out_size, "%s", pref);
    return out;
}

static const char *user_path(const char *name, char *out, size_t out_size)
{
    char dir[1200];
    if (strpbrk(name, "/\\") != NULL || strcmp(name, "..") == 0 || user_dir(dir, sizeof dir) == NULL) return NULL;
    SDL_snprintf(out, out_size, "%s%s", dir, name);
    return out;
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    char path[1024];
    if (user_path(name, path, sizeof path) == NULL) return NULL;
    return SDL_LoadFile(path, size);
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    char path[1024], temp[1100];
    if (user_path(name, path, sizeof path) == NULL) return false;
    /* Write beside the target and rename, so a crash cannot truncate a good file. */
    SDL_snprintf(temp, sizeof temp, "%s.tmp", path);
    if (!SDL_SaveFile(temp, data, size)) {
        port_log("platform: cannot write %s: %s\n", temp, SDL_GetError());
        return false;
    }
    if (!SDL_RenamePath(temp, path)) {
        port_log("platform: cannot rename %s: %s\n", temp, SDL_GetError());
        return false;
    }
    return true;
}

bool platform_remove_user_file(const char *name)
{
    char path[1024];
    return user_path(name, path, sizeof path) != NULL && SDL_RemovePath(path);
}

bool platform_user_file_exists(const char *name)
{
    char path[1024];
    SDL_PathInfo info;
    return user_path(name, path, sizeof path) != NULL && SDL_GetPathInfo(path, &info)
           && info.type == SDL_PATHTYPE_FILE;
}

static int compare_names(const void *a, const void *b)
{
    return SDL_strcasecmp(*(char *const *)a, *(char *const *)b);
}

char **platform_list_user_files(const char *pattern, int *count)
{
    char dir[1200];
    *count = 0;
    if (user_dir(dir, sizeof dir) == NULL) return NULL;
    return platform_list_files(dir, pattern, count);
}

char **platform_list_files(const char *dir, const char *pattern, int *count)
{
    *count = 0;
    char **names = SDL_GlobDirectory(dir, pattern, SDL_GLOB_CASEINSENSITIVE, count);
    if (names == NULL || *count == 0) {
        SDL_free(names);
        *count = 0;
        return NULL;
    }
    SDL_qsort(names, (size_t)*count, sizeof *names, compare_names);
    return names;
}

void platform_free_names(char **names)
{
    SDL_free(names);
}

static void capture_frame(void)
{
    if (capture_dir == NULL || presented_frames++ % (unsigned)capture_every != 0 || presented_frames <= capture_from) return;
    char path[1024];
    SDL_snprintf(path, sizeof path, "%s/frame_%05u.ppm", capture_dir, presented_frames - 1);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fprintf(f, "P6\n%d %d\n255\n", fb_width, fb_height);
    for (int i = 0; i < fb_width * fb_height; i++) {
        const uint8_t px[3] = { (uint8_t)(rgb_buffer[i] >> 16), (uint8_t)(rgb_buffer[i] >> 8), (uint8_t)rgb_buffer[i] };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

void platform_wait_vblank(void)
{
    const uint64_t period = platform_refresh_period_ns();
    const uint64_t now = SDL_GetTicksNS();
    if (next_vblank_ns == 0 || now > next_vblank_ns + period) next_vblank_ns = now;  /* resync after stalls */
    next_vblank_ns += period;
    if (next_vblank_ns > now) platform_sleep_ms((uint32_t)((next_vblank_ns - now) / SDL_NS_PER_MS));
}

typedef struct {
    const uint8_t *pixels;
    const uint32_t *lut;
    uint32_t *out;
    int width, height, count;
    size_t stride;   /* out's row, in pixels (the texture's pitch may be wider than width) */
} LutJob;

static void lut_band(void *ctx, int index)
{
    const LutJob *j = ctx;
    const int from = (int)((int64_t)j->height * index / j->count), to = (int)((int64_t)j->height * (index + 1) / j->count);
    for (int y = from; y < to; y++) {
        const uint8_t *in = j->pixels + (size_t)y * (size_t)j->width;
        uint32_t *row = j->out + (size_t)y * j->stride;
        for (int x = 0; x < j->width; x++) row[x] = j->lut[in[x]];
    }
}

void platform_palette_to_xrgb_pitch(const uint8_t *pixels, const PlatformColor palette[256], void *out, int pitch, int width,
                                    int height, int threads)
{
    uint32_t lut[256];
    for (int i = 0; i < 256; i++) {
        lut[i] = ((uint32_t)palette[i].r << 16) | ((uint32_t)palette[i].g << 8) | palette[i].b;
    }
    enum { MIN_ROWS = 32 };   /* a band of fewer rows costs more to hand out than to convert */
    int count = threads > 1 ? threads * 2 : 1;
    if (count > height / MIN_ROWS) count = height / MIN_ROWS;
    if (count < 1) count = 1;
    const LutJob job = { pixels, lut, out, width, height, count, (size_t)pitch / sizeof(uint32_t) };
    platform_parallel_for(count, lut_band, (void *)&job, threads);
}

void platform_palette_to_xrgb(const uint8_t *pixels, const PlatformColor palette[256], uint32_t *out, int width, int height,
                              int threads)
{
    platform_palette_to_xrgb_pitch(pixels, palette, out, width * (int)sizeof *out, width, height, threads);
}

static int present_threads;

void platform_set_present_threads(int threads) { present_threads = threads; }

static int present_lock = 1;

void platform_set_present_lock(int on) { present_lock = on; }

void platform_present(const uint8_t *pixels, const PlatformColor palette[256])
{
    last_present_ms = (uint32_t)SDL_GetTicks() - script_start_ms;
    void *locked = NULL;
    int pitch = 0;
    /* the frame goes straight into the texture's memory, no second copy; a capture needs rgb_buffer, the web build keeps
     * the upload, and a failed lock falls back to it */
#ifndef __EMSCRIPTEN__
    if (capture_dir == NULL && present_lock && SDL_LockTexture(texture, NULL, &locked, &pitch)) {
        platform_palette_to_xrgb_pitch(pixels, palette, locked, pitch, fb_width, fb_height, present_threads);
        SDL_UnlockTexture(texture);
    } else
#endif
    {
        platform_palette_to_xrgb(pixels, palette, rgb_buffer, fb_width, fb_height, present_threads);
        capture_frame();
        SDL_UpdateTexture(texture, NULL, rgb_buffer, fb_width * (int)sizeof *rgb_buffer);
    }
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

uint32_t platform_ticks_ms(void)
{
    return (uint32_t)SDL_GetTicks();
}

void platform_sleep_ms(uint32_t ms)
{
#ifdef __EMSCRIPTEN__
    /* With ASYNCIFY this unwinds to the browser so the tab stays responsive. */
    emscripten_sleep(ms);
#else
    SDL_Delay(ms);
#endif
}

/* The job pool (platform.h). Workers are made the first time a batch asks for them and wait on `wake`; a batch is
 * published, `needed` workers woken, and the caller claims indexes with them. Every woken worker checks in on `parked`
 * once it can claim no more, and the caller waits for all of them before it returns, so no worker is ever inside a
 * batch while the next one is set up. */
#ifndef __EMSCRIPTEN__
enum { POOL_MAX = 15, POOL_STACK_BYTES = 1 << 20 };
static struct {
    SDL_Thread *thread[POOL_MAX];
    int workers;
    SDL_Semaphore *wake, *parked;
    void (*job)(void *ctx, int index);
    void *ctx;
    int count;
    SDL_AtomicInt next, quit;
} pool;

static void pool_run(void)
{
    for (;;) {
        const int i = SDL_AddAtomicInt(&pool.next, 1);
        if (i >= pool.count) return;
        pool.job(pool.ctx, i);
    }
}

static int pool_worker(void *unused)
{
    (void)unused;
    for (;;) {
        SDL_WaitSemaphore(pool.wake);
        if (SDL_GetAtomicInt(&pool.quit) != 0) return 0;
        pool_run();
        SDL_SignalSemaphore(pool.parked);
    }
}

static int pool_grow(int workers)
{
    if (workers > POOL_MAX) workers = POOL_MAX;
    if (pool.wake == NULL) {
        pool.wake = SDL_CreateSemaphore(0);
        pool.parked = SDL_CreateSemaphore(0);
        if (pool.wake == NULL || pool.parked == NULL) return 0;
    }
    while (pool.workers < workers) {
        /* port: a stack of its own size: the rasterizers keep their edge tables on it (quad.c's about 34 KB at 1440 rows) */
        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)pool_worker);
        SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "tnfs-job");
        SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, POOL_STACK_BYTES);
        pool.thread[pool.workers] = SDL_CreateThreadWithProperties(props);
        SDL_DestroyProperties(props);
        if (pool.thread[pool.workers] == NULL) break;
        pool.workers++;
    }
    return pool.workers;
}

static void pool_stop(void)
{
    if (pool.workers == 0) return;
    SDL_SetAtomicInt(&pool.quit, 1);
    for (int i = 0; i < pool.workers; i++) SDL_SignalSemaphore(pool.wake);
    for (int i = 0; i < pool.workers; i++) SDL_WaitThread(pool.thread[i], NULL);
    pool.workers = 0;
    SDL_SetAtomicInt(&pool.quit, 0);
}
#endif

int platform_cpu_count(void)
{
#ifdef __EMSCRIPTEN__
    return 1;
#else
    const int n = SDL_GetNumLogicalCPUCores();
    return n < 1 ? 1 : n;
#endif
}

void platform_parallel_for(int count, void (*job)(void *ctx, int index), void *ctx, int threads)
{
#ifndef __EMSCRIPTEN__
    if (threads > 1 && count > 1) {
        const int wanted = (threads < count ? threads : count) - 1;
        const int workers = pool_grow(wanted);
        const int used = workers < wanted ? workers : wanted;
        if (used > 0) {
            pool.job = job, pool.ctx = ctx, pool.count = count;
            SDL_SetAtomicInt(&pool.next, 0);
            for (int i = 0; i < used; i++) SDL_SignalSemaphore(pool.wake);
            pool_run();
            for (int i = 0; i < used; i++) SDL_WaitSemaphore(pool.parked);
            return;
        }
    }
#else
    (void)threads;
#endif
    for (int i = 0; i < count; i++) job(ctx, i);
}

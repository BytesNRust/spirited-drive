#include "platform/platform.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t *rgb_buffer;
static int fb_width;
static int fb_height;

enum { KEY_QUEUE_SIZE = 16, VBLANK_HZ = 60 };

/* TNFS_CAPTURE_DIR=dir [TNFS_CAPTURE_EVERY=n]: write every n-th presented
 * frame as dir/frame_NNNNN.ppm, to check the game's screens headless. */
static const char *capture_dir;
static int capture_every = 30;
static unsigned presented_frames;
static uint64_t next_vblank_ns;
static PlatformKey key_queue[KEY_QUEUE_SIZE];
static int key_head;
static int key_count;

/* TNFS_INPUT_SCRIPT="ms:KEY,ms:MOUSE:x:y,...": scripted input for headless
 * runs. KEY (UP, DOWN, LEFT, RIGHT, ENTER, ESC, SPACE, Y, N, J) is held for 100 ms
 * starting ms milliseconds after platform_init, and longer if the game has
 * not polled it yet (slow frame capture must not drop presses); MOUSE moves
 * the pointer; ms:TEXT:abc types abc (ASCII). ENTER and ESC also reach the
 * typed-text queue. */
enum { SCRIPT_MAX = 64, SCRIPT_HOLD_MS = 100 };
typedef struct {
    uint32_t at_ms;
    int scancode;  /* set-1, 0 for mouse moves and text */
    int x, y;
    bool seen;     /* held until the game has read it at least once */
    bool queued;   /* text / editing code already pushed to the text queue */
    char text[16]; /* TEXT events */
} ScriptEvent;
static ScriptEvent script[SCRIPT_MAX];
static int script_count;
static uint32_t script_start_ms;

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

bool platform_init(const PlatformConfig *config)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "platform: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    fb_width = config->width;
    fb_height = config->height;

    if (!SDL_CreateWindowAndRenderer(config->title, fb_width, fb_height, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        fprintf(stderr, "platform: window creation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetRenderLogicalPresentation(renderer, fb_width, fb_height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    load_script();
    SDL_StartTextInput(window);
    capture_dir = SDL_getenv("TNFS_CAPTURE_DIR");
    const char *every = SDL_getenv("TNFS_CAPTURE_EVERY");
    if (every != NULL && SDL_atoi(every) > 0) capture_every = SDL_atoi(every);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, fb_width, fb_height);
    rgb_buffer = malloc((size_t)fb_width * (size_t)fb_height * sizeof *rgb_buffer);
    if (texture == NULL || rgb_buffer == NULL) {
        fprintf(stderr, "platform: framebuffer allocation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return true;
}

void platform_shutdown(void)
{
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

bool platform_poll_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return false;
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

PlatformKey platform_next_key(void)
{
    if (key_count == 0) return PLATFORM_KEY_NONE;
    const PlatformKey key = key_queue[key_head];
    key_head = (key_head + 1) % KEY_QUEUE_SIZE;
    key_count--;
    return key;
}

static int script_key(const char *name)
{
    static const struct { const char *name; int scancode; } KEYS[] = {
        { "UP", 0x48 }, { "DOWN", 0x50 }, { "LEFT", 0x4B }, { "RIGHT", 0x4D },
        { "ENTER", 0x1C }, { "ESC", 0x01 }, { "SPACE", 0x39 },
        { "Y", 0x15 }, { "N", 0x31 }, { "J", 0x24 },
    };
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
        if (SDL_strcmp(name, KEYS[i].name) == 0) return KEYS[i].scancode;
    }
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
        ScriptEvent e = { 0, 0, 0, 0, false, false, "" };
        char name[16] = "";
        if (SDL_sscanf(item, "%u:MOUSE:%d:%d", &e.at_ms, &e.x, &e.y) == 3) {
            script[script_count++] = e;
        } else if (SDL_sscanf(item, "%u:TEXT:%15s", &e.at_ms, e.text) == 2) {
            e.scancode = -2;
            script[script_count++] = e;
        } else if (SDL_sscanf(item, "%u:%15s", &e.at_ms, name) == 2 && (e.scancode = script_key(name)) > 0) {
            script[script_count++] = e;
        } else {
            fprintf(stderr, "platform: bad TNFS_INPUT_SCRIPT item '%s'\n", item);
        }
    }
    script_start_ms = (uint32_t)SDL_GetTicks();
}

static bool script_key_down(int scancode)
{
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
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    for (int i = 0; i < script_count; i++) {
        ScriptEvent *e = &script[i];
        if (e->queued || now < e->at_ms) continue;
        if (e->scancode == -2) push_utf8(e->text);
        if (e->scancode == 0x1C) push_text(PLATFORM_TEXT_ENTER);
        if (e->scancode == 0x01) push_text(PLATFORM_TEXT_ESCAPE);
        e->queued = true;
    }
}

static void apply_script_mouse(void)
{
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    for (int i = 0; i < script_count; i++) {
        if (script[i].scancode == 0 && now >= script[i].at_ms) {
            mouse_x = (float)script[i].x;
            mouse_y = (float)script[i].y;
            mouse_moved = true;
            script[i].scancode = -1;  /* consumed */
        }
    }
}

static SDL_Scancode from_set1(int scancode)
{
    switch (scancode) {
    case 0x01: return SDL_SCANCODE_ESCAPE;
    case 0x15: return SDL_SCANCODE_Y;
    case 0x1C: return SDL_SCANCODE_RETURN;
    case 0x1D: return SDL_SCANCODE_LCTRL;
    case 0x24: return SDL_SCANCODE_J;
    case 0x2A: return SDL_SCANCODE_LSHIFT;
    case 0x31: return SDL_SCANCODE_N;
    case 0x36: return SDL_SCANCODE_RSHIFT;
    case 0x38: return SDL_SCANCODE_LALT;
    case 0x39: return SDL_SCANCODE_SPACE;
    case 0x3B: return SDL_SCANCODE_F1;
    case 0x3C: return SDL_SCANCODE_F2;
    case 0x3D: return SDL_SCANCODE_F3;
    case 0x3E: return SDL_SCANCODE_F4;
    case 0x3F: return SDL_SCANCODE_F5;
    case 0x44: return SDL_SCANCODE_F10;
    case 0x48: return SDL_SCANCODE_UP;
    case 0x4B: return SDL_SCANCODE_LEFT;
    case 0x4D: return SDL_SCANCODE_RIGHT;
    case 0x50: return SDL_SCANCODE_DOWN;
    case 0x52: return SDL_SCANCODE_INSERT;
    case 0x53: return SDL_SCANCODE_DELETE;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

bool platform_key_down(int scancode)
{
    if (script_count > 0 && script_key_down(scancode)) return true;
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

uint8_t *platform_read_file(const char *path, size_t *size)
{
    uint8_t *data = SDL_LoadFile(path, size);
    if (data == NULL) fprintf(stderr, "platform: cannot read %s: %s\n", path, SDL_GetError());
    return data;
}

void platform_free_file(uint8_t *data)
{
    SDL_free(data);
}

static void capture_frame(void)
{
    if (capture_dir == NULL || presented_frames++ % (unsigned)capture_every != 0) return;
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
    const uint64_t period = SDL_NS_PER_SECOND / VBLANK_HZ;
    const uint64_t now = SDL_GetTicksNS();
    if (next_vblank_ns == 0 || now > next_vblank_ns + period) next_vblank_ns = now;  /* resync after stalls */
    next_vblank_ns += period;
    if (next_vblank_ns > now) platform_sleep_ms((uint32_t)((next_vblank_ns - now) / SDL_NS_PER_MS));
}

void platform_present(const uint8_t *pixels, const PlatformColor palette[256])
{
    uint32_t lut[256];
    for (int i = 0; i < 256; i++) {
        lut[i] = ((uint32_t)palette[i].r << 16) | ((uint32_t)palette[i].g << 8) | palette[i].b;
    }
    const size_t count = (size_t)fb_width * (size_t)fb_height;
    for (size_t i = 0; i < count; i++) {
        rgb_buffer[i] = lut[pixels[i]];
    }
    capture_frame();
    SDL_UpdateTexture(texture, NULL, rgb_buffer, fb_width * (int)sizeof *rgb_buffer);
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

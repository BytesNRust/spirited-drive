/* port: the scripted input of headless runs (TNFS_INPUT_SCRIPT, TNFS_INPUT_SETTLE) and the --verbose key and window watch.
 * Moved out of platform_sdl3.c; it drives the key queue, the text queue and the pointer through input_shared.h. */
#include "platform/platform.h"

#include "game/sys/key_latch.h"
#include "game/sys/port_log.h"
#include "platform/sdl3/input_script.h"
#include "platform/sdl3/input_shared.h"
#include "platform/sdl3/pad_keys.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TNFS_INPUT_SCRIPT="ms:KEY,ms:MOUSE:x:y,...": scripted input for headless
 * runs. KEY (UP, DOWN, LEFT, RIGHT, ENTER, ESC, SPACE, a letter, a digit, F1..F12, CTRL, ALT, SHIFT, KP1,
 * PGUP, PGDN, HOME, END;
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
 * cursor), after SCRIPT_SETTLE_CAP_MS.
 * ms:WAIT:text:secs (with TNFS_INPUT_SETTLE only): the script holds there, ms after the event before it, until the game logs a line
 * containing `text` (port_log_watch, armed at that moment: a line logged before does not count) -- the game's marks are the
 * "mark:..." lines (tools/link_loopback.sh) -- and goes on; if no such line comes in `secs` seconds the process says which step
 * it stopped at ("input script: step N: WAIT text timed out after S s") and exits with status 3. */
enum { SCRIPT_MAX = 64, SCRIPT_HOLD_MS = 100, SCRIPT_SETTLE_CAP_MS = 12000, SCRIPT_READ_GAP_MS = 250 };
typedef struct {
    uint32_t at_ms;
    int scancode;  /* set-1, 0 for mouse moves, -2 text, -3 clicks */
    int x, y;
    bool seen;     /* held until the game has read it at least once */
    bool queued;   /* text / editing code already pushed to the text queue */
    bool keyed;    /* pushed to the key queue, as a real press is */
    char text[32]; /* TEXT events; WAIT: the log line's text */
    uint32_t delay; /* TNFS_INPUT_SETTLE: ms after the event before it */
    bool placed;    /* CLICK: the pointer moved there, the press due on the next read */
    bool latched;   /* a key's press latched, as the event of a real one does */
} ScriptEvent;
static ScriptEvent script[SCRIPT_MAX];
static int script_count;
static uint32_t script_start_ms;
static uint32_t settle_ms, last_present_ms, script_last_ms;  /* TNFS_INPUT_SETTLE */
static uint32_t last_read_ms, reading_since_ms;               /* the pointer read with no gap since */
static int script_next;

/* port (--verbose): the window's focus and visibility changes, to see what releases a held key (SDL lets every key go when the window loses the focus) */
void input_script_log_window_event(Uint32 type)
{
    const char *name = type == SDL_EVENT_WINDOW_FOCUS_GAINED ? "focus gained" : type == SDL_EVENT_WINDOW_FOCUS_LOST ? "focus lost"
        : type == SDL_EVENT_WINDOW_OCCLUDED ? "occluded" : type == SDL_EVENT_WINDOW_HIDDEN ? "hidden"
        : type == SDL_EVENT_WINDOW_MINIMIZED ? "minimized" : type == SDL_EVENT_WINDOW_RESTORED ? "restored"
        : type == SDL_EVENT_WINDOW_EXPOSED ? "exposed" : NULL;
    if (name != NULL) port_log("platform: window %s at %llu ms\n", name, (unsigned long long)SDL_GetTicks());
}

/* port (--verbose): the arrows and the race's letter keys as the events say they are held, against SDL's keyboard state, to catch a held key the
 * state reads up (a missed key up, a focus lost, a stalled queue): each press, release and disagreement is logged with the time */
static const SDL_Scancode WATCHED[] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_A, SDL_SCANCODE_Z };
enum { WATCHED_COUNT = sizeof WATCHED / sizeof WATCHED[0] };
static bool watched_held[WATCHED_COUNT], watched_disagree[WATCHED_COUNT];

void input_script_watch_key_event(const SDL_Event *event)
{
    if (event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP) return;
    for (int k = 0; k < WATCHED_COUNT; k++) {
        if (event->key.scancode != WATCHED[k]) continue;
        if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat) return;
        watched_held[k] = event->type == SDL_EVENT_KEY_DOWN;
        port_log("platform: key %s %s at %llu ms\n", SDL_GetScancodeName(WATCHED[k]), watched_held[k] ? "down" : "up", (unsigned long long)SDL_GetTicks());
    }
}

void input_script_watch_check(void)
{
    const bool *state = SDL_GetKeyboardState(NULL);
    for (int k = 0; k < WATCHED_COUNT; k++) {
        const bool disagree = watched_held[k] != state[WATCHED[k]];
        if (disagree != watched_disagree[k])
            port_log("platform: key %s: the events say %s, the state says %s (%s) at %llu ms\n", SDL_GetScancodeName(WATCHED[k]),
                     watched_held[k] ? "held" : "up", state[WATCHED[k]] ? "held" : "up", disagree ? "now they disagree" : "agree again", (unsigned long long)SDL_GetTicks());
        watched_disagree[k] = disagree;
    }
}

void input_script_apply_keys(void)
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
            if (KEYS[k].scancode == e->scancode) sdl3_key_push(KEYS[k].key);
        e->keyed = true;
    }
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

void input_script_load(void)
{
    const char *text = SDL_getenv("TNFS_INPUT_SCRIPT");
    char buf[2048];
    if (text == NULL) return;
    SDL_strlcpy(buf, text, sizeof buf);
    char *save = NULL;
    for (char *item = SDL_strtok_r(buf, ",", &save); item != NULL && script_count < SCRIPT_MAX;
         item = SDL_strtok_r(NULL, ",", &save)) {
        ScriptEvent e = { 0, 0, 0, 0, false, false, false, "", 0, false, false };
        char name[16] = "";
        const char *wait = SDL_strstr(item, ":WAIT:");
        const char *last = wait != NULL ? SDL_strrchr(item, ':') : NULL;   /* WAIT: ms:WAIT:text:secs, the text may hold colons */
        if (wait != NULL && last > wait + 6 && last - (wait + 6) < (int)sizeof e.text && SDL_sscanf(item, "%u:", &e.at_ms) == 1 &&
            SDL_sscanf(last + 1, "%d", &e.x) == 1 && e.x > 0) {   /* x: the seconds */
            SDL_memcpy(e.text, wait + 6, (size_t)(last - (wait + 6)));
            e.scancode = -4;
            script[script_count++] = e;
        } else if (SDL_sscanf(item, "%u:MOUSE:%d:%d", &e.at_ms, &e.x, &e.y) == 3) {
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
    if (settle_ms == 0) {
        for (int i = 0; i < script_count; i++)
            if (script[i].scancode == -4) port_log("platform: WAIT %s in TNFS_INPUT_SCRIPT needs TNFS_INPUT_SETTLE; it does nothing\n", script[i].text);
        return;
    }
    for (int i = 0; i < script_count; i++) script[i].delay = script[i].at_ms, script[i].at_ms = UINT32_MAX;
}

static bool script_done(const ScriptEvent *e, uint32_t now)
{
    if (e->scancode == -1) return true;                       /* a mouse move, consumed */
    if (e->scancode == -4) return port_log_watch_hit() != 0;  /* WAIT: the game logged its line */
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
            if (!script_done(e, now)) {
                if (e->scancode == -4 && now - e->at_ms > (uint32_t)e->x * 1000u) {   /* the step the script stopped at, named */
                    port_log("input script: step %d: WAIT %s timed out after %d s\n", script_next + 1, e->text, e->x);
                    if (!port_log_verbose()) fprintf(stderr, "input script: step %d: WAIT %s timed out after %d s\n", script_next + 1, e->text, e->x);
                    exit(3);
                }
                return;
            }
            if (e->scancode == -4) port_log("input script: step %d: %s came\n", script_next + 1, e->text);
            script_next++, script_last_ms = now;
            continue;
        }
        const bool pointer = e->scancode == 0 || e->scancode == -3;
        const uint32_t due = script_last_ms + e->delay;
        const bool still = now - last_present_ms >= settle_ms && now - reading_since_ms >= settle_ms;
        if (now < due || (pointer && !still && now < due + SCRIPT_SETTLE_CAP_MS)) return;
        e->at_ms = now;
        if (e->scancode == -4) {   /* WAIT: from now on (its own line first: it holds the text it waits for) */
            port_log("input script: step %d: waiting for %s\n", script_next + 1, e->text);
            port_log_watch(e->text);
        }
        return;
    }
}

bool input_script_key_down(int scancode)
{
    script_sequence();
    input_script_latch_presses();   /* the press is latched as its event is pumped, before the read that sees it */
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

void input_script_apply_text(void)
{
    script_sequence();
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    for (int i = 0; i < script_count; i++) {
        ScriptEvent *e = &script[i];
        if (e->queued || e->scancode == -3 || now < e->at_ms) continue;
        if (e->scancode == -2) sdl3_text_push_utf8(e->text);
        if (e->scancode == 0x1C) sdl3_text_push(PLATFORM_TEXT_ENTER);
        if (e->scancode == 0x01) sdl3_text_push(PLATFORM_TEXT_ESCAPE);
        e->queued = true;
    }
}

void input_script_apply_mouse(void)
{
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    if (now - last_read_ms > SCRIPT_READ_GAP_MS) reading_since_ms = now;
    last_read_ms = now;
    script_sequence();
    for (int i = 0; i < script_count; i++) {
        if (script[i].scancode == 0 && now >= script[i].at_ms) {
            sdl3_mouse_place((float)script[i].x, (float)script[i].y);
            script[i].scancode = -1;  /* consumed */
        }
        if (script[i].scancode == -3 && now >= script[i].at_ms && !script[i].queued && !script[i].placed) {
            sdl3_mouse_place((float)script[i].x, (float)script[i].y);  /* moved first: a game reading the button before the move would press where its pointer was */
            script[i].placed = true;
        } else if (script[i].scancode == -3 && script[i].placed && !script[i].queued) {  /* pressed */
            sdl3_mouse_buttons(PLATFORM_MOUSE_LEFT, 0);
            script[i].queued = true;
            script[i].at_ms = now;  /* held from the press */
        } else if (script[i].scancode == -3 && script[i].queued && !script[i].seen &&
                   now >= script[i].at_ms + SCRIPT_HOLD_MS) {  /* released */
            sdl3_mouse_buttons(0, PLATFORM_MOUSE_LEFT);
            script[i].seen = true;
        }
    }
}

/* a scripted key press latches as a real one does (its KEY_DOWN event), so the script walks the same paths a finger does */
void input_script_latch_presses(void)
{
    const uint32_t now = (uint32_t)SDL_GetTicks() - script_start_ms;
    for (int i = 0; i < script_count; i++) {
        ScriptEvent *e = &script[i];
        if (e->latched || e->scancode <= 0 || now < e->at_ms) continue;
        const SDL_Scancode sc = sdl3_from_set1(e->scancode);
        if (sc != SDL_SCANCODE_UNKNOWN) key_latch_press((int)sc);
        e->latched = true;
    }
}

bool input_script_active(void) { return script_count > 0; }

void input_script_watch_reset(void) { memset(watched_held, 0, sizeof watched_held); }

/* a frame went to the display: the settle clock restarts */
void input_script_note_present(void) { last_present_ms = (uint32_t)SDL_GetTicks() - script_start_ms; }

int platform_script_key(const char *name) { return script_key(name); }

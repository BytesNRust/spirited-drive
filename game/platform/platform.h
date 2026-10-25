/* Platform layer: the only interface the game code uses to reach the OS.
 * The original shipped the same game on DOS and Win95 by swapping this layer;
 * the port does the same with SDL3 (native + web) as the first backend. */
#ifndef TNFS_PLATFORM_H
#define TNFS_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "game/sys/control_map.h"

typedef struct {
    uint8_t r, g, b;
} PlatformColor;

typedef struct {
    int width;
    int height;
    const char *title;
    int scale_integer;          /* 0 fill the window (letterboxed); 1 whole multiples only, with bands */
    int fullscreen;             /* 1: the desktop's full screen */
    int refresh_hz;             /* platform_wait_vblank's rate; 0 = 60 */
} PlatformConfig;

bool platform_init(const PlatformConfig *config);
/* The open window's framebuffer, scaling, fullscreen and refresh taken from `config` (its title is not), the
 * window's height kept and its width following the new proportion; false (and all as it was) when it cannot. */
bool platform_reconfigure(const PlatformConfig *config);
/* The framebuffer platform_init (or platform_reconfigure) made (width x height of its config). */
void platform_framebuffer_size(int *width, int *height);
/* The desktop's size of the display the window is on; false when it cannot be told. */
bool platform_desktop_size(int *width, int *height);
/* One refresh of platform_wait_vblank, in nanoseconds (1e9 / refresh_hz). */
uint64_t platform_refresh_period_ns(void);
void platform_shutdown(void);
/* The window's icon (and the Dock's on macOS): w x h RGBA pixels, top row first; nothing without a window. */
void platform_set_window_icon(const uint8_t *rgba, int w, int h);

typedef enum {
    PLATFORM_KEY_NONE = 0,
    PLATFORM_KEY_LEFT,
    PLATFORM_KEY_RIGHT,
    PLATFORM_KEY_UP,
    PLATFORM_KEY_DOWN,
    PLATFORM_KEY_ENTER,
    PLATFORM_KEY_ESCAPE,
    PLATFORM_KEY_SPACE
} PlatformKey;

/* Pumps OS events. Returns false once the user asked to quit. */
bool platform_poll_events(void);

/* Next key pressed since the last call, or PLATFORM_KEY_NONE. */
PlatformKey platform_next_key(void);

/* Whether a key is held now, by IBM PC set-1 scancode (0x01 Esc, 0x1C Enter,
 * 0x48 Up ...), the codes the 1996 game itself polls. Extended keys use the
 * same numbers as their BIOS codes (0x48 Up, 0x52 Insert, 0x53 Delete). */
bool platform_key_down(int scancode);

/* The SDL scancode platform_key_down reads a set-1 scancode from, 0 when none. */
int platform_key_sdl(int scancode);

/* The set-1 scancode TNFS_INPUT_SCRIPT's name is ("UP", "PGDN", "A", "1"), or -1 for a name the table does not have.
 * The names are the exe's own key names (key_capture.c): PGUP, PGDN, HOME and END among them. */
int platform_script_key(const char *name);

/* The words for a key, by set-1 scancode, in capitals ("A", "LEFT SHIFT", "SPACE"); false (out left empty) for a code
 * with no key. */
bool platform_key_name(int scancode, char *out, size_t size);

/* Typed text, for name entry: the next Unicode code point typed, one of the
 * editing codes below, or 0 when the queue is empty. */
enum {
    PLATFORM_TEXT_BACKSPACE = 0x08,
    PLATFORM_TEXT_ENTER = 0x0D,
    PLATFORM_TEXT_ESCAPE = 0x1B,
    PLATFORM_TEXT_DELETE = 0x7F
};
uint32_t platform_next_text(void);
/* Whether the window takes typed text (on from platform_init, for the name boxes). A race turns it off: with it on, macOS
 * sends each letter key through its input client, and a letter held down (the horn) opens the accent chooser, which
 * swallows the key events of the keys held with it. */
void platform_text_input(bool on);
bool platform_text_input_active(void);

/* The system's mouse cursor is hidden over the window (the menus draw their own pointer): while the window has the focus,
 * in fullscreen and windowed alike; shown again on losing the focus and at platform_shutdown. */
bool platform_cursor_hidden(void);

enum {
    PLATFORM_MOUSE_LEFT = 1,
    PLATFORM_MOUSE_RIGHT = 2
};

/* Mouse in framebuffer coordinates and its button mask. Returns true when
 * the mouse moved since the previous call. */
bool platform_mouse(int *x, int *y, unsigned *buttons);

/* Mods overlay (BYTES-0009). With a mods directory set, a file the game asks for under `data_root` is read from
 * the mods directory when the mod has its own copy (the same relative name, or its upper-case form), else
 * from `data_root`. A mod file is validated against the original executable's limits first (formats/validate.h):
 * an invalid one is not handed to the game, and platform_mods_error() says which file failed and why. Names
 * that would leave the mods directory (.. components, absolute names, symbolic links out of it) are refused.
 * User files (config, saves, replays) go to <mods>/user/ and are read from there, so the CD data and the mod's
 * own files are never written. Returns false when the directory does not exist. */
bool platform_set_mods(const char *data_root, const char *mods_dir);
/* The last mod file refused, with the reason; "" when none. */
const char *platform_mods_error(void);

/* Whole file in a platform-owned buffer (release with platform_free_file), or NULL. */
uint8_t *platform_read_file(const char *path, size_t *size);
void platform_free_file(uint8_t *data);

/* Per-user writable storage for what the game saves (config.dat). The CD
 * data is read-only and must never be written. Names are plain file names,
 * no directories. */
uint8_t *platform_read_user_file(const char *name, size_t *size);  /* release with platform_free_file */
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size);
bool platform_remove_user_file(const char *name);
bool platform_user_file_exists(const char *name);
/* The user files whose names match pattern ('*' and '?', case ignored), sorted, in a platform-owned list released
 * with platform_free_names; NULL when none. */
char **platform_list_user_files(const char *pattern, int *count);
/* The same for the files of a directory of the data (the CD's own, read only) */
char **platform_list_files(const char *dir, const char *pattern, int *count);
void platform_free_names(char **names);

/* Shows an 8-bit indexed framebuffer of config->width x config->height. */
void platform_present(const uint8_t *pixels, const PlatformColor palette[256]);

/* Blocks until the next display refresh (paced at config->refresh_hz, 60 by default, on every target).
 * The original waited on DirectDraw's WaitForVerticalBlank. */
void platform_wait_vblank(void);

/* Sound out: 16-bit stereo frames at `rate` Hz, pulled from `fill` on the
 * device's own thread as it needs them (the original's mixer ran on its own
 * thread into DirectSound's buffer, so a load on the game's thread never
 * starved it). Without a device (none, or SDL's dummy driver refusing) open
 * fails. The lock keeps `fill` out while the game changes what it reads; it
 * is recursive, and does nothing without a device. */
typedef void (*PlatformAudioFill)(void *user, int16_t *frames, uint32_t count);
bool platform_audio_open(int rate, PlatformAudioFill fill, void *user);
void platform_audio_close(void);
void platform_audio_lock(void);
void platform_audio_unlock(void);

/* Joysticks, as the exe's WinMM calls (joyGetNumDevs, joyGetDevCaps,
 * joyGetPosEx) saw them: devices 0..count-1; the caps give the name, the
 * buttons and the axes there (1 x, 2 y, 4 z, 8 r); a read gives the buttons
 * (bit k button k) and the axes x, y, z, r in 0..0xffff, the middle 0x8000.
 * Without a joystick count is 0 and caps / read fail. */
int platform_joy_count(void);
bool platform_joy_caps(int device, char *name, size_t name_size, int *buttons, uint32_t *axes);
bool platform_joy_read(int device, uint32_t *buttons, int32_t axis[4]);

/* port: the bindings platform_joy_read reads a pad or wheel through (control_map.h), from user/controls.cfg until
 * one is set; and the keys they press. */
void platform_controls_set(const PortControls *controls);
/* The device as it is now, whole: every button, axis and hat (what a binding is captured from). */
bool platform_joy_raw(int device, RawPad *out);
/* The scancode the virtual key of action ACT_HORN, ACT_PAUSE, ACT_VIEW or ACT_NITRO presses (P and F1 until set; the
 * horn's is config.dat's, set when a race starts; the Nitro's is RACE_KEYS_PAD_NITRO, set then too); platform_key_down
 * answers true for it while a binding of the action is held. */
void platform_virtual_key(int action, int scancode);
bool platform_joy_virtual_down(int scancode);
void platform_joy_virtual_set(int device, uint32_t keys);   /* what platform_joy_read leaves: control_map's `keys` */
/* what platform_poll_events calls to read the open pads' keys again, so a binding let go is seen while nothing else
 * reads the pad (the pause's wait for its key) */
void platform_joy_virtual_refresher(void (*refresh)(void));

/* Force feedback on a joystick (the exe's IFORCE.DLL): haptic effects where
 * the device has them, else its rumble for the jolts, the buffeting and the
 * vibrations. open fails with neither, and then the rest do nothing. A jolt:
 * strength 0..100 from `direction` degrees for `ms`; the buffeting and the
 * vibrations (axis 0 x, 1 y: a, b strengths, c the rate in Hz) run until
 * cleared; the vector force (x, y in -100..100) holds until changed or
 * cleared. */
bool platform_forces_open(int device);
void platform_forces_close(void);
void platform_forces_jolt(int strength, int direction, int ms);
void platform_forces_buffeting(int level);
void platform_forces_vibration(int axis, int a, int b, int c);
void platform_forces_vibration_clear(int axis);
void platform_forces_vector(int x, int y);
void platform_forces_enable(bool on);
void platform_forces_clear(void);

uint32_t platform_ticks_ms(void);

/* Yields to the OS (and to the browser event loop on the web build). */
void platform_sleep_ms(uint32_t ms);

/* Worker threads for jobs that touch disjoint memory (the render bands). platform_cpu_count is the logical cores (1
 * where there are no threads: the web build). platform_parallel_for runs job(ctx, i) for i = 0 .. count - 1, each
 * once, on up to `threads` threads including the caller, and returns when all have finished (the caller sees their
 * writes). The order the indexes are claimed in is not defined. With one thread, one job, or no thread support it is
 * the plain loop on the caller. The job must not call back into the platform. */
int platform_cpu_count(void);
void platform_parallel_for(int count, void (*job)(void *ctx, int index), void *ctx, int threads);

#endif

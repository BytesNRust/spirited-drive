/* Platform layer: the only interface the game code uses to reach the OS.
 * The original shipped the same game on DOS and Win95 by swapping this layer;
 * the port does the same with SDL3 (native + web) as the first backend. */
#ifndef TNFS_PLATFORM_H
#define TNFS_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} PlatformColor;

typedef struct {
    int width;
    int height;
    const char *title;
} PlatformConfig;

bool platform_init(const PlatformConfig *config);
void platform_shutdown(void);

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

/* Typed text, for name entry: the next Unicode code point typed, one of the
 * editing codes below, or 0 when the queue is empty. */
enum {
    PLATFORM_TEXT_BACKSPACE = 0x08,
    PLATFORM_TEXT_ENTER = 0x0D,
    PLATFORM_TEXT_ESCAPE = 0x1B,
    PLATFORM_TEXT_DELETE = 0x7F
};
uint32_t platform_next_text(void);

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

/* Blocks until the next display refresh (paced at 60 Hz on every target).
 * The original waited on DirectDraw's WaitForVerticalBlank. */
void platform_wait_vblank(void);

/* Sound out: 16-bit stereo frames at `rate` Hz, queued by the game (the
 * original's mixer filled DirectSound's buffer the same way). Without a
 * device (none, or SDL's dummy driver refusing) open fails, and push and
 * queued do nothing. */
bool platform_audio_open(int rate);
void platform_audio_close(void);
uint32_t platform_audio_queued(void);  /* frames not yet played */
void platform_audio_push(const int16_t *frames, uint32_t count);

uint32_t platform_ticks_ms(void);

/* Yields to the OS (and to the browser event loop on the web build). */
void platform_sleep_ms(uint32_t ms);

#endif

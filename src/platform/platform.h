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
    PLATFORM_KEY_ENTER,
    PLATFORM_KEY_ESCAPE
} PlatformKey;

/* Pumps OS events. Returns false once the user asked to quit. */
bool platform_poll_events(void);

/* Next key pressed since the last call, or PLATFORM_KEY_NONE. */
PlatformKey platform_next_key(void);

/* Whole file in a platform-owned buffer (release with platform_free_file), or NULL. */
uint8_t *platform_read_file(const char *path, size_t *size);
void platform_free_file(uint8_t *data);

/* Shows an 8-bit indexed framebuffer of config->width x config->height. */
void platform_present(const uint8_t *pixels, const PlatformColor palette[256]);

uint32_t platform_ticks_ms(void);

/* Yields to the OS (and to the browser event loop on the web build). */
void platform_sleep_ms(uint32_t ms);

#endif

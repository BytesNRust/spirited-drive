/* Platform layer: the only interface the game code uses to reach the OS.
 * The original shipped the same game on DOS and Win95 by swapping this layer;
 * the port does the same with SDL3 (native + web) as the first backend. */
#ifndef TNFS_PLATFORM_H
#define TNFS_PLATFORM_H

#include <stdbool.h>
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

/* Pumps OS events. Returns false once the user asked to quit. */
bool platform_poll_events(void);

/* Shows an 8-bit indexed framebuffer of config->width x config->height. */
void platform_present(const uint8_t *pixels, const PlatformColor palette[256]);

uint32_t platform_ticks_ms(void);

/* Yields to the OS (and to the browser event loop on the web build). */
void platform_sleep_ms(uint32_t ms);

#endif

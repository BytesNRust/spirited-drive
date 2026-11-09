/* what platform_sdl3.c lends the input script: the queues and the pointer it drives as a real device does */
#ifndef TNFS_INPUT_SHARED_H
#define TNFS_INPUT_SHARED_H

#include "platform/platform.h"

#include <stdint.h>

void sdl3_key_push(PlatformKey key);                   /* the key queue (platform_next_key), if it has room */
void sdl3_text_push(uint32_t code);                    /* the typed-text queue (platform_next_text) */
void sdl3_text_push_utf8(const char *s);
void sdl3_mouse_place(float x, float y);               /* the pointer moved there */
void sdl3_mouse_buttons(unsigned press, unsigned release);

#endif

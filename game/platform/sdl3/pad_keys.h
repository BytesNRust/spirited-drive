/* set-1 scancodes as SDL's, and the pad refresh hook (pad_keys.c) */
#ifndef TNFS_PAD_KEYS_H
#define TNFS_PAD_KEYS_H

#include <SDL3/SDL.h>

SDL_Scancode sdl3_from_set1(int scancode);   /* SDL_SCANCODE_UNKNOWN for a code with no key */
void pad_keys_refresh(void);                 /* the pad's keys, read again if a refresher is set */

#endif

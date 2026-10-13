/* The menus' sounds, through EA's sound driver. */
#ifndef TNFS_MENU_SOUND_H
#define TNFS_MENU_SOUND_H

#include "game/frontend/config_dat.h"

#include <stdint.h>

typedef struct MenuSoundOps {
    /* 0048eea0: starts `sound` at pan, volume and pitch; returns the voice. */
    int32_t (*voice_start)(void *ctx, int32_t sound, int32_t pan, int32_t volume, int32_t pitch);
    void *ctx;
} MenuSoundOps;

typedef struct MenuSound {
    int sound_on;      /* the short 0052b304 */
    int32_t voice_4a;  /* 005247e8: the voice of sound 0x4a */
    int32_t voice;     /* 00524808: the voice of any other */
} MenuSound;

/* 0046492c: with the sound on, starts the short `sound` centred (0x40) at
 * config.dat's volume (the short 00524e69, read as the high half of the
 * dword 00524e67) and pitch 0, and keeps its voice (0x4a's apart). */
void menu_sound_play(MenuSound *m, const ConfigDat *c, int sound, const MenuSoundOps *ops);

#endif

/* The menus' sounds, through EA's sound driver. */
#ifndef TNFS_MENU_SOUND_H
#define TNFS_MENU_SOUND_H

#include "game/frontend/config_dat.h"

#include <stdint.h>

typedef struct MenuSoundOps {
    /* 0048eea0: starts `sound` at pan, volume and pitch; returns the voice. */
    int32_t (*voice_start)(void *ctx, int32_t sound, int32_t pan, int32_t volume, int32_t pitch);
    /* 00473b00: eax a voice, edx and ebx two words (0x64 and -1 fade the
     * music out); what they mean in the driver is not known. */
    void (*voice_ramp)(void *ctx, int32_t voice, int32_t edx, int32_t ebx);
    void (*voice_stop)(void *ctx, int32_t voice);  /* 00473c68: eax a voice */
    void (*voice_volume)(void *ctx, int32_t voice, int32_t volume);  /* 00473b4c */
    int32_t (*voice_stopped)(void *ctx, int32_t voice);              /* 00473d20: non-zero once stopped */
    void (*bank_unregister)(void *ctx, int32_t bank);                /* 0048edfc: eax 0048ed30's handle */
    void (*file_free)(void *ctx, void *data);                        /* 0047529c (cdecl) */
    void *ctx;
} MenuSoundOps;

typedef struct MenuSound {
    int sound_on;      /* the short 0052b304 */
    int32_t voice_4a;  /* 005247e8: the voice of sound 0x4a */
    int32_t bank;      /* 005247ec: front_mw.bnk's handle (0048ed30, 004648dc) */
    void *bank_data;   /* 00524804: front_mw.bnk's file (00483ec0, 004648dc) */
    int32_t voice;     /* 00524808: the voice of any other */
    int32_t music;     /* 0052480c: the music's voice */
    int music_off;     /* the short 0052b31e */
    int music_on;      /* the short 004c5cc8: the music is playing */
} MenuSound;

/* 0046492c: with the sound on, starts the short `sound` centred (0x40) at
 * config.dat's volume (the short 00524e69, read as the high half of the
 * dword 00524e67) and pitch 0, and keeps its voice (0x4a's apart). */
void menu_sound_play(MenuSound *m, const ConfigDat *c, int sound, const MenuSoundOps *ops);

/* 004644f0: fades the music out (00473b00 with 0x64 and -1), only while it
 * plays, with music allowed (0052b31e 0), on in config.dat (the short
 * 00524e63) and the sound on. */
void menu_sound_music_fade_out(const MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops);

/* 00464534: 00473c68 on the music's voice, under the same four guards. */
void menu_sound_music_stop(const MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops);

/* 00464604: 00473b00 on the music's voice with 0x32 and the short volume,
 * under the guards but whether the music plays. */
void menu_sound_music_fade_to(const MenuSound *m, const ConfigDat *c, int volume, const MenuSoundOps *ops);

/* 0046463c: the same with 0 for 0x32. */
void menu_sound_music_volume(const MenuSound *m, const ConfigDat *c, int volume, const MenuSoundOps *ops);

/* 004645d8: with the sound on, 00473b4c gives the short volume to the
 * effects' voice, then to 0x4a's. */
void menu_sound_effects_volume(const MenuSound *m, int volume, const MenuSoundOps *ops);

/* 00464980: with the sound on, 00473c68 on 0x4a's voice. */
void menu_sound_stop_4a(const MenuSound *m, const MenuSoundOps *ops);

/* 00464994: 1 when 0x4a has played out (no voice, or 00473d20 says
 * stopped); 0 while it plays and with the sound off. */
int menu_sound_4a_done(const MenuSound *m, const MenuSoundOps *ops);

/* 004649c8: with the sound on, unregisters front_mw.bnk (0048edfc) and
 * frees its file (0047529c); both are left set. */
void menu_sound_bank_free(const MenuSound *m, const MenuSoundOps *ops);

#endif

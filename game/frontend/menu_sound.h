/* The menus' sounds, through EA's sound driver. */
#ifndef TNFS_MENU_SOUND_H
#define TNFS_MENU_SOUND_H

#include "game/frontend/config_dat.h"

#include <stdint.h>

enum { MENU_SOUND_PATH = 0x50 };

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
    void (*wait)(void *ctx);                    /* 00475c14(0), cdecl: what it does is not known */
    void (*stream_close)(void *ctx, int32_t stream);  /* 0048a92c (cdecl) */
    void (*drain)(void *ctx);                   /* 004770ac: loops until 0049b6fc gives 0 */
    int32_t (*stream_open)(void *ctx, int32_t size, int32_t block, int32_t count);  /* 0048a674 (cdecl) */
    void (*stream_load)(void *ctx, int32_t stream, const char *path);  /* 0048aa78 (cdecl) */
    void (*stream_start)(void *ctx, int32_t stream);  /* 0048de54: eax the stream */
    /* 004640d0's: what each gives is guessed from how it is used. */
    int32_t (*ticks)(void *ctx);                      /* 00473334: summed against 0x64 */
    int32_t (*queued)(void *ctx);                     /* 0048e054(0): compared with the stream's size */
    int32_t (*stream_read)(void *ctx, int32_t stream);  /* 0048bcfc (cdecl): a chunk or 0 */
    void (*chunk_play)(void *ctx, int32_t chunk);     /* 0048df3c: eax the chunk */
    int32_t (*stream_voice)(void *ctx);               /* 0048dedc: the voice the stream plays on */
    void (*block_free)(void *ctx, int32_t block);     /* 00475260 (cdecl) */
    void (*track_stop)(void *ctx);                    /* 00492620 */
    void (*track_play)(void *ctx);                    /* 004925a8: [004c7c0c] = 1 */
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
    int music_voiced;  /* the short 004c5cc6: the music's voice started (0046417c) */
    int32_t stream;    /* 00524830: the music's stream (0048a674, 00464428) */
    int32_t chunk;     /* 0052481c: the last chunk 0048bcfc gave (004640d0) */
    int small_stream;  /* the short 004c5cca: a 0x19000-byte stream, else 0x7d000 */
    char music_path[MENU_SOUND_PATH];  /* 00524798: the music's file (00464428) */
    int32_t track_music;  /* 00524828: the track screen's music (00464788's 00483e80) */
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

/* 00464568: under 004644f0's four guards, waits (00475c14) while the music
 * has not stopped (00473d20) and music_voiced is set, closes the stream
 * (0048a92c) and clears music_on and music_voiced. */
void menu_sound_music_end(MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops);

/* 00464428: with music allowed, on in config.dat and the sound on: the
 * music playing is stopped (00473c68) and played out (00464568); 004770ac;
 * music_path = dir + name ("%s%s"; the two must fit); a stream opened
 * (0048a674 with 0x19000 or 0x7d000, 0x800, 0x20), loaded with the path
 * (0048aa78) and started (0048de54); music_on = 1. */
void menu_sound_music_start(MenuSound *m, const ConfigDat *c, const char *dir, const char *name,
                            const MenuSoundOps *ops);

/* 004640d0: keeps the music fed. With music allowed and on in config.dat:
 * 00475c14(0); then, with the sound on and the music playing, reads and
 * plays chunks of the stream while 0048e054 is under the stream's size
 * (0x14000 small, else 0x64000), and stops early once the ticks summed
 * as a short pass 0x64 and 0048e054 gives 0. */
void menu_sound_music_feed(MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops);

/* 0046417c: services the music. With music allowed: 00475c14(0) in a
 * 0052b312 game; then with it on in config.dat, the sound on and the music
 * playing, reads a chunk (0048bcfc); the first time 0048e054 is past the
 * stream's size (music_voiced still 0) the voice starts (0048dedc), takes
 * config.dat's music volume (the short 00524e67, 00473b4c) and music_voiced
 * becomes 1; a chunk that is not 0 is played (0048df3c). */
void menu_sound_music_service(MenuSound *m, const ConfigDat *c, const FrontendSession *f, const MenuSoundOps *ops);

/* 004648a8: with the sound on and the short 00524e6b set in config.dat,
 * frees the track music (00475260) when there is one and clears it. */
void menu_sound_track_music_free(MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops);

/* 0046486c: under 004648a8's two guards, stops the track music (00492620). */
void menu_sound_track_music_stop(const MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops);

/* 00464850: under the same guards, lets the track music play (004925a8). */
void menu_sound_track_music_play(const MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops);

#endif

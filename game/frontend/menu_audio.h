/* The menus' sound and music over --drive's audio (drive_audio.h): the
 * device, front_mw.bnk, the effects (0046492c, menu_sound.c) and the music
 * (race_music.c's 00464428 / 004640d0 / 00464240 at config.dat's menu
 * volume, the short 00524e67). Glue, no oracle. */
#ifndef TNFS_MENU_AUDIO_H
#define TNFS_MENU_AUDIO_H

#include <stddef.h>
#include <stdint.h>

void menu_audio_open(void);   /* the device; nothing plays without one */
void menu_audio_close(void);  /* the music stopped, the banks freed, the device closed */

void menu_audio_bank_load(void);                /* 004648dc */
void menu_audio_bank_free(void);                /* 004649c8 */
void menu_audio_sound(int32_t sound);           /* 0046492c */
void menu_audio_stop_4a(void);                  /* 00464980 */
int32_t menu_audio_4a_done(void);               /* 00464994 */
void menu_audio_effects_volume(int32_t volume); /* 004645d8 */
void menu_audio_voice_volume(int32_t voice, int32_t volume);  /* 00473b4c */
void menu_audio_music_start(const char *name);  /* 00464428 */
void menu_audio_music_feed(void);               /* 004640d0 */
void menu_audio_music_service(void);            /* 0046417c */
void menu_audio_music_fade_out(void);           /* 004644f0 */
void menu_audio_music_end(void);                /* 00464568 */
void menu_audio_music_fade_to(int32_t volume);  /* 00464604 */
void menu_audio_music_pick(void);               /* 004562e8 */
void menu_audio_music_track(void);              /* 00456244 */
/* The track music (the screens' spoken words, frontend/speech/ or gspeech/, 0042a188's 00524238) on drive_audio's
 * speech queue */
void menu_audio_track_music(const char *name);  /* 00464788 */
void menu_audio_track_play(void);               /* 00464850 */
void menu_audio_track_stop(void);               /* 0046486c */
void menu_audio_track_free(void);               /* 004648a8 */
void menu_audio_music_by_idle(void);            /* 0045638c, the idle timer the ticks since the start */
/* A movie's sound, an EACS sample kept by the caller until it stops (tgv_player.h); 0 when nothing plays */
int menu_audio_movie_sound(const uint8_t *eacs, size_t size);
void menu_audio_movie_sound_stop(void);

#endif

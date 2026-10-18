/* The menus' sound and music over --drive's audio (drive_audio.h): the
 * device, front_mw.bnk, the effects (0046492c, menu_sound.c) and the music
 * (race_music.c's 00464428 / 004640d0 / 00464240 at config.dat's menu
 * volume, the short 00524e67). Glue, no oracle. */
#ifndef TNFS_MENU_AUDIO_H
#define TNFS_MENU_AUDIO_H

#include <stdint.h>

void menu_audio_open(void);   /* the device; nothing plays without one */
void menu_audio_close(void);  /* the music stopped, the banks freed, the device closed */

void menu_audio_bank_load(void);                /* 004648dc */
void menu_audio_bank_free(void);                /* 004649c8 */
void menu_audio_sound(int32_t sound);           /* 0046492c */
void menu_audio_music_start(const char *name);  /* 00464428 */
void menu_audio_music_feed(void);               /* 004640d0 */
void menu_audio_music_service(void);            /* 0046417c */
void menu_audio_music_fade_out(void);           /* 004644f0 */
void menu_audio_music_end(void);                /* 00464568 */
void menu_audio_music_fade_to(int32_t volume);  /* 00464604 */
void menu_audio_music_pick(void);               /* 004562e8 */
void menu_audio_music_by_idle(void);            /* 0045638c, the idle timer the ticks since the start */

#endif

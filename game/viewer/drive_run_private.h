/* --drive's loop, shared by drive_run.c (the window, the loop's steps) and drive_run_shot.c (the headless screenshots): the steps the two of them use. */
#ifndef TNFS_DRIVE_RUN_PRIVATE_H
#define TNFS_DRIVE_RUN_PRIVATE_H

#include "viewer/drive_session.h"
#include "viewer/track_viewer.h"

#include <stdint.h>

enum { KEY_F1 = 0x3b, KEY_F2 = 0x3c, KEY_F3 = 0x3d, KEY_F9 = 0x43 };

extern RaceSoundBanks drive_banks;
extern int present_dry;   /* port (dev, TNFS_FRAME_TIMES): present_drive without the platform */

void take_detail(int32_t *pending);
void drive_tick(uint32_t input);
void run_music_volume(void *user, int16_t volume);
void police_keep(RaceState *race);
void start_at_node(RaceState *race);
int f3_env_press(int32_t frame);
int env_press(int32_t frame);
int write_drive_ppm(const char *path, const uint8_t *shown, int width, const uint8_t *palette);
void present_drive(const Track *t, uint8_t *pixels);
uint32_t screenshot_input(uint32_t i, uint32_t ticks);

#endif

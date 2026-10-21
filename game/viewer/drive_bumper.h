/* port: the bumper view's extras: EXTRAS' Bumper Mirror and Bumper Radar (port_extras.h), F7 / F8 turning them over
 * in a race, the mirror's picture under the scoreboard's right end. The original shows neither outside the car. */
#ifndef TNFS_DRIVE_BUMPER_H
#define TNFS_DRIVE_BUMPER_H

#include "game/viewer/screen_layout.h"

#include <stdint.h>

enum {
    DRIVE_BUMPER_KIND = 1,     /* the chase kind of the bumper view */
    DRIVE_BUMPER_MARGIN = 6,   /* between the scoreboard or the screen's edge and the frame */
    DRIVE_BUMPER_FRAME = 0     /* the frame's colour (black in the race's palettes) */
};

typedef struct {
    int mirror, radar;
    int f7_down, f8_down;
} DriveBumper;

/* A race's start: the switches as EXTRAS has them. */
void drive_bumper_start(DriveBumper *b, int mirror, int radar);
/* This frame's keys: a press (up to down) turns its switch over. */
void drive_bumper_keys(DriveBumper *b, int f7, int f8);
int drive_bumper_view(int32_t chase_kind);
/* Where a mirror picture `mirror_w` wide goes on a `width` screen: its right side DRIVE_BUMPER_MARGIN in from the edge,
 * its top as far under the scoreboard. */
ScreenPoint drive_bumper_mirror_at(int width, int mirror_w);
/* The picture (w x h) at `at` with a one-pixel frame round it, cut to the screen. */
void drive_bumper_blit(uint8_t *screen, int screen_w, int screen_h, const uint8_t *picture, int w, int h, ScreenPoint at);

/* The radar detector's lights as the dash draws them (cockpit_lights, 004212d4): the dash art's four shapes, a NULL
 * one draws nothing. */
typedef struct {
    const uint8_t *big_lit, *big_dark, *small_lit, *small_dark;   /* 'bglt', 'bgdk', 'smlt', 'smdk' */
} DriveBumperRadarArt;
enum { DRIVE_BUMPER_RADAR_SMALL = 5, DRIVE_BUMPER_RADAR_PAD = 2, DRIVE_BUMPER_RADAR_GAP = 3 };
/* The detector's size (x, y): the big light then the five small ones in a row, a pad round them; 0 x 0 without art. */
ScreenPoint drive_bumper_radar_size(const DriveBumperRadarArt *art);
/* Where it goes on a `width` screen: the same margin under the scoreboard's left end as the mirror has at the right. */
ScreenPoint drive_bumper_radar_at(int width);
/* The detector at `at`, cut to the screen, the dash's way: `count` (race->unknown_4b92d4) lights that many of the
 * small ones, none at 0; the big light shows `big_lit` (the police lights' toggle) while count is not 0, else dark. */
void drive_bumper_radar(uint8_t *screen, int screen_w, int screen_h, const DriveBumperRadarArt *art, int32_t count,
                        int32_t big_lit, ScreenPoint at);

#endif

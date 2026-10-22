/* port: the bumper view's extras: EXTRAS' Bumper Mirror and Bumper Radar (port_extras.h), F1 / F2 turning them over
 * in a race (the exe's F1, F2 and F4 off: race_keys.h port_bumper_keys; F7 and F8 are the HUD's), the mirror's picture
 * under the scoreboard's right end. The original shows neither outside the car. */
#ifndef TNFS_DRIVE_BUMPER_H
#define TNFS_DRIVE_BUMPER_H

#include "game/viewer/screen_layout.h"

#include <stdint.h>

enum {
    DRIVE_BUMPER_KIND = 1,     /* the chase kind of the bumper view */
    DRIVE_BUMPER_MARGIN = 6,   /* between the scoreboard or the screen's edge and the frame */
    DRIVE_BUMPER_MIRROR_PERCENT = 150,   /* the mirror's picture against the mirror camera's own size (the owner's choice) */
    DRIVE_BUMPER_FRAME = 0     /* the frame's colour (black in the race's palettes) */
};

typedef struct {
    int mirror, radar;
    int mirror_down, radar_down;   /* F1 / F2 were down the frame before */
} DriveBumper;

/* A race's start: the switches as EXTRAS has them. */
void drive_bumper_start(DriveBumper *b, int mirror, int radar);
/* This frame's F1 (the mirror) and F2 (the radar): a press (up to down) turns its switch over. */
void drive_bumper_keys(DriveBumper *b, int mirror_key, int radar_key);
int drive_bumper_view(int32_t chase_kind);
/* Where a mirror picture `mirror_w` wide goes on a `width` screen: its right side DRIVE_BUMPER_MARGIN in from the edge,
 * its top as far under the scoreboard. */
ScreenPoint drive_bumper_mirror_at(int width, int mirror_w);
/* The picture (w x h) at `at` with a one-pixel frame round it, cut to the screen. */
void drive_bumper_blit(uint8_t *screen, int screen_w, int screen_h, const uint8_t *picture, int w, int h, ScreenPoint at);

/* The radar detector: the lights as the dash draws them (cockpit_lights, 004212d4), the dash art's four shapes (a
 * NULL one draws nothing), in a grey case. The case is the same for every car: a raised edge (`light` on the top and
 * left, `dark` on the bottom and right) round a `face`, and in it a sunken black well (the edges the other way
 * round) holding the lights. The three greys are palette indices, the race palette's nearest to grey. */
typedef struct {
    const uint8_t *big_lit, *big_dark, *small_lit, *small_dark;   /* 'bglt', 'bgdk', 'smlt', 'smdk' */
    uint8_t light, face, dark;
} DriveBumperRadarArt;
enum {
    DRIVE_BUMPER_RADAR_SMALL = 5, DRIVE_BUMPER_RADAR_GAP = 3,
    DRIVE_BUMPER_RADAR_BEVEL = 1,       /* the case's edge and the well's */
    DRIVE_BUMPER_RADAR_FACE = 3,        /* the face between the two */
    DRIVE_BUMPER_RADAR_WELL = 2,        /* black round the lights, inside the well's edge */
    /* the lights' inset from the case's edge */
    DRIVE_BUMPER_RADAR_PAD = 2 * DRIVE_BUMPER_RADAR_BEVEL + DRIVE_BUMPER_RADAR_FACE + DRIVE_BUMPER_RADAR_WELL
};
/* The grey for each of the case's three tones, as RGB 0..255 a channel (what the palette's nearest is taken to). */
enum { DRIVE_BUMPER_RADAR_LIGHT_GREY = 190, DRIVE_BUMPER_RADAR_FACE_GREY = 120, DRIVE_BUMPER_RADAR_DARK_GREY = 55 };
/* The detector's size (x, y): the case round the big light and the five small ones in a row; 0 x 0 without art. */
ScreenPoint drive_bumper_radar_size(const DriveBumperRadarArt *art);
/* Where it goes on a `width` screen: the same margin under the scoreboard's left end as the mirror has at the right. */
ScreenPoint drive_bumper_radar_at(int width);
/* The detector at `at`, cut to the screen, the dash's way: `count` (race->unknown_4b92d4) lights that many of the
 * small ones, none at 0; the big light shows `big_lit` (the police lights' toggle) while count is not 0, else dark. */
void drive_bumper_radar(uint8_t *screen, int screen_w, int screen_h, const DriveBumperRadarArt *art, int32_t count,
                        int32_t big_lit, ScreenPoint at);

#endif

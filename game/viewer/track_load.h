/* A track read for the viewers (`--track`, `--drive`): its TRI and FAM, the art, the sky's INFO, the scenery's objects
 * and models, loaded from the data root (track_load.c). */
#ifndef TNFS_TRACK_LOAD_H
#define TNFS_TRACK_LOAD_H

#include "game/race/race_palette.h"
#include "game/race/track_art.h"
#include "game/race/track_fam.h"
#include "game/race/track_info.h"
#include "game/race/track_objects.h"
#include "viewer/model_view.h"


enum {
    SCREEN_W = 640,
    SCREEN_H = 480,
    DRAW_SEGMENTS = 40,   /* segments ahead of the camera */
    STRIPS = 10,          /* 5 on each side of the centre */
    SKY_COLOUR = 0,
    TRACK_MODELS = 64      /* 005150f0, 0x138 bytes each */
};

enum { CAR_AHEAD = 2 };

typedef struct {
    uint8_t *tri_file;
    uint8_t *fam_file;
    Tri tri;
    Chunk fam;
    TrackArt art;
    TrackObject *objects;   /* one per scenery record */
    uint32_t object_count;
    TrackInfo info;
    RacePaletteTint tint;   /* 0050b774: 0043a370's 004471ac for the time of day */
    ViewModel models[TRACK_MODELS];
    int model_ok[TRACK_MODELS];
    uint8_t *car_file;      /* TNFS_CAR=NAME: CARFAMS/NAME.CFM parked ahead */
    ViewModel car[2];       /* high and low detail */
    int car_ok;
} Track;

/* The track `name` with 004510e8's art and 0043a370's sky for `look`; 0 and a message when a file is missing. */
int track_viewer_load(Track *t, const char *data_root, const char *name, const TrackFamInputs *look);

/* port: a CFM parked ahead on the track (TNFS_CAR=NAME). */
void track_viewer_load_car(Track *t, const char *data_root, const char *name);

void track_viewer_free(Track *t);

/* port: --drive without the menus has no setup to give 004fa0ec: whether the CD holds no FAM for `look`. */
int track_viewer_fam_missing(const char *data_root, const char *name, const TrackFamInputs *look);

/* SIMDATA/`dir`/`name` with the first of `suffixes` the CD holds (NULL ends them); NULL and a message without. */
uint8_t *track_viewer_read_any(const char *data_root, const char *dir, const char *name, const char *const *suffixes,
                               size_t *size);

/* A PPM of `pixels` (w x h) through the palette `rgb`. */
int track_viewer_write_ppm(const char *path, const uint8_t *pixels, int w, int h, const uint8_t *rgb);

#endif

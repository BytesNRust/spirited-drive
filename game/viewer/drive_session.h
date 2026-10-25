/* The race --drive runs, as track_viewer.c starts and ends it: the state and calls it shares with drive_draw.c (the
 * frame), drive_wide.c (the wide screen), drive_bumper_draw.c (the bumper view) and drive_run.c (the loop, the window
 * and the screenshots). */
#ifndef TNFS_DRIVE_SESSION_H
#define TNFS_DRIVE_SESSION_H

#include "viewer/track_load.h"
#include "viewer/track_viewer.h"
#include "game/race/frame_draw.h"
#include "game/sys/port_settings.h"
#include "viewer/drive_bumper.h"
#include "viewer/drive_keys.h"
#include "viewer/drive_menu.h"
#include "viewer/drive_pause.h"
#include "viewer/drive_palette.h"
#include "viewer/ported_view.h"


/* The drive's cars: each slot's CFM (drawn) and PDN (its body). */
typedef struct {
    uint8_t *file[RACE_CARS];   /* each slot's CFM as read: the loader works in it */
    CarLoad load[RACE_CARS];    /* 00457d40's results */
    int loaded[RACE_CARS];
    RacePalette tint;           /* the tint the loader applies (0050b774): neutral */
    uint8_t *carpal;            /* CARFAMS/CARPAL.FSH */
    CarPdn pdns[RACE_CARS];
    const char *data_root;
    int ended;
    int ticked;                 /* a tick since the last frame drawn */
    int livery[RACE_CARS];      /* port: 1 when the slot wears the PS1 alternate livery (car_livery_pick), 0 the exe's */
    char psx_dir[PSX_DIR_MAX];  /* port: port.cfg's psx_dir for this race, "" none */
} DriveCars;

/* port: the wide screen (port.cfg's format; TNFS_SCREEN=4:3 / 16:9 / 21:9 overrides it for the checks). The race
 * draws its 2D as ever into the 640 x 480 frame, where the 3D view is left LAYER_KEY; the 3D view alone is drawn
 * at the screen's width into `pixels`, and layer_compose puts the frame over it (layer_compose.h). */
typedef struct {
    int width;                          /* 640: none of this */
    uint8_t *pixels;                    /* width x 480 (x scale each way) */
    ViewLayout rows[VIEW_LAYOUTS];      /* 004b7080's high rows at the width (screen_layouts) */
    int fov_adjust;
    int ready;                          /* the frame composed since the last present */
    int scale;                          /* port (render scale): 1, 2 or 3; `pixels` is width * scale x 480 * scale */
    uint8_t *layer;                     /* with scale > 1: the 2D over the 3D, width x 480, LAYER_KEY where the 3D shows */
} DriveWide;

enum { HIGH_SCREEN = 1 };  /* 00512018 for 640 x 480 */

/* The race's words; the ones the other files read are defined where the comment above each says. */
extern RaceSim drive_sim;
extern DriveInput drive_input;
extern TrackViewerLink drive_link;
extern int drive_link_on;
extern DriveKeys drive_keys;
extern DriveReplay drive_replay;
extern DrivePause drive_pause;
extern DriveMenu drive_menu;
extern DriveCars drive_cars;
extern DriveHud drive_hud;
extern DriveBumper drive_bumper;
extern DriveMirror bumper_mirror;
extern TrackViewerMenus drive_menus;
extern int drive_menus_on;
extern TrackViewerResults drive_results;
extern int32_t link_wait_ticks;
extern RaceViews drive_views;
extern FrameDraw drive_frame;
extern DriveWide drive_wide;

/* track_viewer.c: the race's end (once): 0045af80 on the race, the results kept for the menus and printed. */
void drive_end(void);

/* drive_wide.c */
ScreenFormat drive_screen_format(void);
/* port (render scale): the wide path runs when the screen is wider than 640 or the 3D is drawn at more than 1x */
int drive_wide_active(void);
/* where the 2D layer is composed: `pixels` at scale 1, else `layer` (width x 480) */
uint8_t *drive_wide_canvas(void);
/* with scale > 1, the layer's pixels put over the 3D as scale x scale blocks (nearest); the end of a frame */
void drive_wide_finish(void);
int drive_output_height(void);
/* the platform could not give the larger framebuffer: the scale stays 1 from now on */
void drive_wide_scale_off(void);
/* port (render scale): the mirror drawn at the scale: its zoom (drive_mirror_zoom), 0 at scale 1 or when it stays on the dash */
int32_t drive_wide_mirror_zoom(const DriveMirror *m);
int32_t drive_wide_bumper_zoom(void);
/* the larger picture `m` made put over the screen at x, y (w x h of the 2D layer's pixels) by drive_wide_finish */
void drive_wide_hires_place(const DriveMirror *m, int x, int y, int w, int h);
void drive_wide_size(int width);
const uint8_t *drive_output(const uint8_t *pixels, int *width);
void drive_wide_set_viewport(const RaceState *race);
PortedViewport drive_wide_viewport(const RaceState *race);
void drive_wide_key_box(const RaceState *race, uint8_t *surface);
void drive_wide_compose(const RaceState *race, const uint8_t *frame);
void drive_wide_centre(const uint8_t *frame);
void drive_wide_map_grab(uint8_t *back);

/* drive_draw.c */
void drive_frame_setup(RaceState *race);
void render_drive(const Track *t, uint8_t *pixels);
PortedCamera chase_camera(const RaceChase *chase, uint32_t ring_nodes);
int32_t drive_draw_rand(void *ctx, int32_t n);

/* drive_bumper_draw.c: the bumper view's mirror's pass (0 without a picture) and, after the frame's composed, its
 * mirror and radar */
int drive_bumper_mirror_render(const DriveMirrorWorld *mirror, int32_t mirror_count);
void drive_bumper_draw(const RaceState *race, uint8_t *pixels, int mirror);

/* drive_run.c: the window's race, TNFS_RUN's headless one and the plain screenshot's */
int drive_window(const Track *t, uint8_t *pixels);
int drive_run_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot);
int drive_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot);
void drive_link_stand_in(int16_t player);
/* port: the frames' refresh rate, set again when the pause menu's Graphics screen closes (track_viewer.c) */
void drive_run_refresh_set(int32_t hz);

#endif

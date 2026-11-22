/* The editor's state and what it does to a project: the trace is edited in place, the road (nodes and limits) is recomputed from it when it
 * changes, and save / build / install run on the project folder. The window (main.c) and the drawing (draw.c) only read and call these. */
#ifndef TNFS_TRACK_EDITOR_APP_H
#define TNFS_TRACK_EDITOR_APP_H

#include "car_build_job.h"
#include "car_edit.h"
#include "mask_edit.h"
#include "mask_tex.h"
#include "psd_stale.h"
#include "original_track.h"
#include "track_masks.h"
#include "night_view.h"
#include "car_export.h"
#include "car_project.h"
#include "car_race.h"
#include "light_ui.h"
#include "project.h"
#include "psd_job.h"
#include "selection.h"
#include "race_sheet.h"
#include "session.h"
#include "track_undo.h"
#include "view.h"
#include "workspace.h"

#include <stdint.h>

enum { APP_PHOTOS_MADE = 16 };
enum {
    APP_PATH = 4096,
    APP_MSG = 320,
    APP_NEW_STEP = 40        /* units between a point and the one added after it */
};

/* what the window has open: nothing (the start screen), a track project, a car project or the Track light window (a light folder, no project). Exactly one; the fields of the other kinds are not read. */
typedef enum { APP_KIND_NONE = 0, APP_KIND_TRACK, APP_KIND_CAR, APP_KIND_LIGHT } AppKind;
/* Studio is two rooms behind one hub: Cars, Tracks (the Track light window is in Tracks), and the main menu with nothing open */
typedef enum { APP_ROOM_HUB = 0, APP_ROOM_CARS, APP_ROOM_TRACKS } AppRoom;

/* what the window was asked to do that ends or changes the project: quit, start a new project, open one, open a recent one */
typedef enum {
    APP_ACT_NONE = 0,
    APP_ACT_QUIT,
    APP_ACT_NEW,       /* a folder for the new project is asked for (main.c's dialog) */
    APP_ACT_OPEN,      /* a folder holding a project is asked for */
    APP_ACT_RECENT,    /* the folder is in the path */
    APP_ACT_NEW_AT,    /* the folder the New dialog gave, in the path: asked through the same question, as the project may have changed meanwhile */
    APP_ACT_OPEN_AT,   /* the same for Open */
    APP_ACT_OPEN_CAR,  /* a car's folder is asked for (its kit's folder) */
    APP_ACT_OPEN_CAR_AT, /* the folder that dialog gave, in the path */
    APP_ACT_EXPORT_PICK, /* a folder to make the exported car's project in is asked for (main.c's dialog); the car is a->export_pick */
    APP_ACT_EXPORT_AT,   /* the folder that dialog gave, in the path: the project is made in it and opened */
    APP_ACT_OPEN_LIGHT,  /* the Track light window on the light folder a->light_dir */
    APP_ACT_LIGHT_PICK,  /* a light folder is asked for (main.c's dialog), from the Track light window's Choose... */
    APP_ACT_LIGHT_AT,    /* the folder that dialog gave, in the path: the window is on it (app_light_set_folder) */
    APP_ACT_MAIN,        /* back to the main menu (the hub): the project is closed (app_close_project), after the question about unsaved changes */
    APP_ACT_WORKSPACE_PICK, /* a workspace folder is asked for (main.c's dialog); the folder that dialog gives is the workspace at once (app_workspace_set), with no question about unsaved changes */
    APP_ACT_EXPORT_TRACK,   /* the Export original track window (a->export_track_dialog) is asked for, after the question about unsaved changes */
    APP_ACT_NEW_CAR,    /* the New car window (a->new_car_dialog) is asked for, after the question about unsaved changes */
    APP_ACT_PHOTO_PICK, /* main.c's file dialog is on screen for a showcase photo (a->photo_dialog asks for it): the picture is copied into the car's folder when it comes back, so no question */
    APP_ACT_BACKDROP_PICK /* main.c's file dialog is on screen for the card photo's backdrop picture (a->backdrop_dialog asks for it): nothing in the project changes, so no question */
} AppAction;

/* the answers to "unsaved changes" */
enum { APP_ANSWER_CANCEL = 0, APP_ANSWER_DISCARD = 1, APP_ANSWER_SAVE = 2 };

/* the Masks window's state for the open car: the textures of its CFM, the mask shown over its texture and how it is shown */

/* a mask file as it was last read or written: its modification time (ns) and size, both 0 while there is no file */
typedef struct {
    long long mtime_ns, size;
} MaskStamp;

/* what the open car's own files looked like when the editor last read them (app_card_watch): mod.ini, the car file, and a sum over the PNG files of
 * modelo/c1 and c3 */
typedef struct {
    MaskStamp card, cfm;
    unsigned long long pngs;
    double looked;                /* when app_card_watch last looked (its clock); 0: never */
    int card_changed;             /* mod.ini changed on disk while the card has unsaved edits: Reload or Keep mine, and Save waits */
    unsigned serial;              /* counts the times the car file changed on disk: the Model reads it again */
} CarDisk;

typedef struct {
    MaskTexInfo tex[MASK_TEX_LIST];   /* the textures of the car file, both banks */
    int tex_count;
    char has_mask[MASK_TEX_LIST];     /* the car's masks folder holds the texture's mask */
    int sel;                          /* the texture whose mask is shown (index into tex), -1 for none */
    MaskEdit me;                      /* its mask as the file has it (read only), when sel >= 0 */
    char note[256];                   /* the size note of the open mask, "" when it matches its texture */
    char tex_file[ME_PATH];           /* the open mask's texture as a PNG of the car's kit (texture_file.h), "" when it has none: Show texture file */
    char error[APP_MSG];         /* why a mask could not be opened, until the next try */
    int show, class_on[ME_CLASSES];   /* the overlay on or off, and each class of it */
    float opacity;                    /* 0..1 */
    float zoom;                       /* screen pixels a mask texel; 0: fit the window (the panel sets it) */
    unsigned generation;              /* counts every mask opened, closed or changed on disk: a picture made from the mask compares with it */
    int focus;                        /* the mask view has the keyboard: + - 0 zoom (set by the panel every frame) */
    MaskStamp disk[MASK_TEX_LIST];    /* each texture's mask file as Studio last saw it (app_mask_watch) */
    double disk_looked;               /* when app_mask_watch last looked at the files (its clock); 0: never */
} MaskUi;

typedef struct {
    AppKind kind;                /* what is open: the track fields below are read for APP_KIND_TRACK, `car` for APP_KIND_CAR */
    CarProject car;              /* the car project, when kind is APP_KIND_CAR */
    int preview_paint;           /* the card's paint the Preview shows: an index of the card's paints, -1 for the car as its file has it */
    char preview_focus[5];       /* a texture the Preview scrolls to (--preview-tex NAME, for a headless picture); "" none */
    NightSettings night;         /* the Preview's Day / Night and lamp switches (editor/night_view.h) */
    int lights_open;             /* --steps lights-open: 1 the Preview's Lights header is unfolded by the next frame (then 0), as a click on it */
    LightUi light;               /* the Track light window (light_ui.h), when kind is APP_KIND_LIGHT */
    char light_dir[APP_PATH];    /* the light folder the Track light window tunes: its track_light/ and track/ (--light-dir, else the source tree's data/); "" for none */
    MaskUi mask;                 /* the masks window's state (editor/mask_edit.h), when kind is APP_KIND_CAR */
    char repo_arg[APP_PATH];     /* --port-data DIR: the game repo's data/ for this run; wins over the kept setting */
    char repo_cfg[APP_PATH];     /* the kept setting ("repo data" in workspace.txt beside the recent list); "" for none */
    char data_dir[APP_PATH];     /* the game's data/ as a source tree has it (--port-data, --light-dir, the tree the program was built from): where the original tracks are added from and published to; "" for none (a packaged app) */
    char art_dir[APP_PATH];      /* the owner's track mask PSDs, <TRACK>/<name>.psd (--art-dir, else art/tnfs-masks/tracks beside the source tree); "" for none */
    char original_dir[APP_PATH]; /* the original track's folder the Track light window is on (<workspace>/tracks/<STEM>); "" for the plain window of a light folder */
    TrackMasks tmasks;           /* the masks of the original track on screen (track_masks.h), scanned when it opens and after Regenerate; tmasks_loaded 0 until then */
    int tmasks_loaded;
    int tmask_sel;               /* the row on show, -1 for none */
    uint8_t *tmask_rgba;         /* its picture, tmask_w x tmask_h: the track's texture with the mask over it (just the mask when the game's files are not found); NULL for none */
    int tmask_w, tmask_h;
    unsigned tmask_serial;       /* counts the pictures made, for the texture */
    double tmask_psd_seen;       /* the newest mask PSD time the scan saw; a PSD newer than this was saved again (app_track_masks_poll) */
    double tmask_check_at;       /* the clock of the last look at the PSDs */
    char tmask_note[200];        /* why there is no picture, or what Regenerate did */
    int original_dialog;         /* the Open original track window is up */
    int publish_dialog;          /* the Publish to the game window is up, with its preview in publish_text */
    int publish_car;             /* ...and it is for a car's masks (app_car_copy), not a track's files */
    OriginalPublish publish_plan;   /* what the preview lists: the light files and the masks of the original track against data/ */
    char publish_text[4096];
    CarDisk cdisk;               /* mod.ini, the car file and the textures as the editor last saw them (app_card_watch) */
    CarEdit edit;                /* its card as it is being edited (car_edit.h), when kind is APP_KIND_CAR; only mod.ini is ever written */
    char dir[APP_PATH];          /* the project folder: a track's, a car's kit; "" with nothing open */
    char game[APP_PATH];         /* the game program, run for install */
    char art_note[160];          /* what opening the project did to or noticed about its copy of the generic art (project_art_refresh); "" for nothing */
    int art_generic;             /* the project draws with Studio's current generic art (project_art_is_generic), as it was when opened */
    char game_tried[2048];       /* every place the game was looked for (game_find), for the message when it is not there */
    char game_memo[APP_PATH];    /* the file that remembers the game the user located ("" for none) */
    char user[APP_PATH];         /* the player folder the install goes to */
    ExportSource export_src;     /* where Export original car reads the game's files from (car_export_source) */
    char photos_made[APP_PHOTOS_MADE][25];   /* the showcase photo files Studio copied into the car's folder (app_photos.c): the ones Save removes when the card stops naming them */
    int photos_made_n;
    int photo_dialog;            /* the Car window's Add photo... asks main.c for a PNG file dialog; main.c takes it back to 0 */
    int backdrop_dialog;         /* the Model window's Choose... asks main.c for a PNG file dialog (the card photo's backdrop); main.c takes it back to 0 */
    int export_dialog;           /* the Export original car window is up */
    int export_pick;             /* the car chosen in it (car_export_car's index), -1 for none */
    RaceJob race;                /* Test in race / Race photo of the open original car (car_race.h), running or last run */
    int race_night;              /* the preset: 0 Day, 1 Night */
    int race_track;              /* the track chosen (car_race_track's index) */
    char race_mod[64];           /* "": the track is race_track's; else the id of an installed track chosen in the Track picker (race_mods_scan over the player's folder) */
    int photo_view_open;         /* the In-game photo's large window (photo_view_panel) is asked for */
    RaceLook race_look;          /* the Race window's look switches (T6): the session's, never written to the car's project; all on by default */
    PsdJob psd;                  /* Convert PSD: the owner's PSDs of this car turned into its masks, in the background (psd_job.h, app_psd.c) */
    char psd_root[APP_PATH];     /* the folder of the owner's PSDs, one folder a car ("" for none): --psd-root, TNFS_PSD_ROOT, else art/tnfs-masks in the workspace */
    CarBuildJob car_build;       /* the kit built in a thread: when the car file is not there at Open, and by Build (car_build_job.h) */
    int model_wheels;            /* the Model window's Wheels: 0 Still, 1 Slow (the slow pair swapped at the game's rate), 2 Fast (the blur); the window's, not the project's */
    int model_wheels_fixed;      /* a headless run: Slow shows its first frame, so a picture is the same every time */
    InstallJob install;          /* Install (U): the game's installer, run in the background (install_job.h) */
    int drive_pick;              /* the car Test drive gives the player: the game's car menu's number (0..7, car_race_menu_car); the editor's settings keep it */
    int dash_check;              /* Check dash (app_dash.h): 1 while its Convert PSD runs (the cockpit photo follows) */
    char dash_stale[320];        /* what is older than its PSD, a sentence ("" when nothing): the Masks window's warning */
    int dash_stale_ticks;        /* frames since it was last worked out */
    StaleReport stale;           /* each PSD of the car against its mask (psd_stale.h), when stale_ok: the Masks window's state column */
    int stale_ok;                /* stale was worked out for this project (a PSD folder was found) */
    unsigned psd_auto_sig;       /* the stale state (which PSDs, how, and when they were saved) Convert PSD was last started for by itself, when psd_auto_set: the same state is not tried again */
    int psd_auto_set;
    int psd_auto_fail;           /* a conversion failed: the next stale state seen is the one that is not tried again (blocked after failure) */
    RaceSheet sheet;             /* the Race window's Sheet: the cells and their photos (app_sheet.h) */
    int sheet_active, sheet_cur, sheet_stop, sheet_serial;   /* it runs; the cell being photographed (-1 none); it is being stopped; changes when a cell does */
    char sheet_saved[APP_PATH];  /* the PNG the last Save sheet wrote */
    char reveal[APP_PATH];       /* a folder the window is asked to show in the Finder, or a file to show selected in its folder (main.c does it and clears this) */
    Project project;
    Trace trace;
    View view;
    float canvas_x, canvas_y;    /* where the Viewport's canvas starts in the window (viewport.cpp, every frame): --steps clicks a point there */
    int selected;                /* index of the selected control point, -1 for none */
    Selection sel;               /* the selected control points (dep87, app_select.h): `selected` is always one of them when there are any */
    TracePoint *rotate_from;     /* the points as they were when the Rotate points field was taken (app_sel_rotate_live), else NULL */
    int rotate_from_count;
    struct {                     /* Add curve and Add straight's fields (app_points.h); all 0 until the Points panel first shows them */
        int right, count, straight_count;   /* a count of 0: the default (arc_block.h) */
        int preview;             /* the Viewport draws what Accept would make: 1 the curve, 2 the straight, 0 nothing (the fields hovered or edited) */
        int block_open;          /* the Add curve / straight block as last drawn: 1 open, 2 folded, 0 not drawn yet (curve_panel.cpp) */
        int block_chosen;        /* the user has clicked the block's header: it keeps what they chose, else it follows the room */
        int block_force;         /* --steps curve-fold: 1 unfold, 2 fold the block at the next frame, as a click on its header (then 0) */
        int preview_hold;        /* the same, held whatever the mouse does (--steps preview-curve, for a picture) */
        double degrees, radius_m, length_m;
    } curve;
    TracePoint *clip_points;     /* Copy's points and their keys on 0..clip_count-1 (app_points.h), NULL before a copy */
    int clip_count;
    TraceKey *clip_keys;
    int clip_key_count;
    int dirty;                   /* the trace changed since the road was computed */
    struct {                     /* the height profile as the Distance strip drew it this frame (strip_panel.cpp); drawn 0 while its tab is not on screen: --steps profile-drag */
        int drawn;
        float x, width, top, bottom;   /* the curve's left edge and width in the window, the rows of its highest and lowest height */
        double lo, hi, total;          /* the heights at those rows, the road's length in metres */
    } profile_drawn;
    struct {                     /* the slope tool's fields (app_slope.h); all 0 until app_slope_defaults */
        int ready, by_rise, keep_after;
        int preview, preview_hold;   /* the profile draws what Accept would make: 1 (the fields hovered or edited), held by --steps; 0 nothing */
        double grade_pct, rise_m, ease_in_m, ease_out_m;
    } slope;
    struct {                     /* the place on the profile last clicked (app_profile_place): between point seg and the next, at fraction frac; the Insert point here button's target */
        int set, seg;
        double frac;
    } profile_place;
    struct {                     /* the Distance strip's window as it was drawn (strip_panel.cpp): its top edge is the splitter under the Viewport; --steps strip-split, print-strip */
        int drawn;
        float x, y, w, h;
    } strip_win;
    int focus_strip;             /* set: the Distance strip's tab is brought to the front by the next frame (--steps strip) */
    int colour_by;               /* the Viewport's Colour by mode (TraceColourBy, 0 Limits); not saved with the project */
    uint8_t *node_class;         /* each node's class in colour_by, made by app_colour_classes; NULL until then and whenever the road is made again */
    int node_class_mode;

    /* the road, computed from the trace by app_recompute */
    TrackGenNode *nodes;         /* malloc'd when road_made */
    int node_count;
    TraceFlags *flags;           /* per node, the TRACE_FLAG_* bits, when road_made */
    int road_made;               /* 0 when the trace cannot become nodes (see road_text) */
    TraceProblem *problems;      /* malloc'd, `problem_count` of them: the nodes that break a limit, red first (track_trace_problems) */
    int problem_count;
    TraceSummaryLine summary[TRACE_SUMMARY_LINES];   /* a line per rule some node breaks, red first */
    int summary_count;
    int stage_short;             /* the road is too short for a stage (track_trace_stage_short): red in Problems and Limits, `stage_text` says why */
    char stage_text[2 * TRACE_SUMMARY_TEXT];
    double length;               /* the centre line's length, in units (TRACE_METRES gives metres) */
    double *node_dist;           /* metres from node 0 to each node, when road_made */
    double *point_dist;          /* metres along the curve to each control point (point_count values, when road_made) */
    int point_count;             /* the control points the road was computed from: trace.count has moved on when a point was added since */
    double *point_turn;          /* the signed turn, in degrees, at the node nearest each control point */
    double *point_width;         /* the road's width in metres at each control point as the keys make it (track_trace_point_widths): the inherited values show dimmed */
    double (*point_sides)[TRACE_SIDE_FIELDS][2];   /* each control point's side values as the keys make them (track_trace_point_sides), with the road: the Road key block's */
    double *point_bank;          /* the road's bank in degrees at each control point as the keys make it (track_trace_point_banks), the same way */
    int node;                    /* the node the Node window shows, -1 for none */
    TraceKey profile_clip;       /* Copy profile: every road value as it is at the point copied (profile_clip.has says which; 0 before the first copy) */
    int reach_from, reach_to;    /* the points a Road profile value under the mouse reaches (road_reach_hover), set each frame the mouse is over one; the Viewport draws them and clears it */
    int node_for;                /* the control point `node` was taken from (app_sync_selection) */
    char road_text[APP_MSG];     /* why the road is not made; "" when it is */
    char build_text[APP_MSG];    /* why Build would refuse the road made (build_road_check); "" when it takes it */

    char saved_edge[8];          /* the verge material as it is in project.ini: app_modified compares with project.edge */
    TrackUndo track_undo;        /* a track's undo and redo, and the project as its files have it: app_modified compares with that */
    AppAction confirm;           /* an action waiting for the answer to "unsaved changes" */
    char confirm_path[APP_PATH];
    AppAction go;                /* an action to run now (main.c): the confirmation is answered, or there was nothing unsaved */
    char go_path[APP_PATH];
    int discard_accepted;        /* Save or Discard was answered for a folder picker (Open, New, Export...): the open the picker gives is not asked about again; spent by it, or by app_pick_cancelled */
    int fit_pending;             /* the view is fitted to the trace on the next frame (a project was just opened) */
    char generic[APP_PATH];      /* the editor's own GENERIC.FAM, for the projects it creates ("" when there is none) */
    Recent recent;               /* the project folders opened lately, newest first */
    char recent_file[APP_PATH];  /* where they are kept, "" when they are not */
    char workspace[APP_PATH];    /* the workspace folder (cars/ and tracks/ in it), "" for none */
    int workspace_put_aside;     /* the offer to choose one was put aside: the start screen does not show it again */
    char workspace_file[APP_PATH]; /* where the choice is kept, "" when it is not */
    int new_track_dialog;        /* the New track window (a workspace is set: only a name is asked for) is up */
    char new_track_name[64];     /* what is typed in it, the id it makes, whether the id was typed over, and why Create was refused */
    char new_track_id[40];
    int new_track_id_typed;
    char new_track_error[APP_MSG];
    int new_car_dialog;          /* the New car window is up: a name, the id it makes (may be typed over), the car it starts as (0..7, the eight you drive) */
    char new_car_name[64];
    char new_car_id[40];
    int new_car_id_typed;
    int new_car_template;
    char new_car_error[APP_MSG];   /* why Create was refused */
    int export_track_dialog;     /* the Export original track window is up: a name, the id it makes (may be typed over), which original track (original_track_stem's index) */
    char export_track_name[64];
    char export_track_id[40];
    int export_track_id_typed;
    int export_track_pick;
    char export_track_error[APP_MSG];   /* why Create was refused */
    int physics_dialog;          /* the Physics from question (what it overwrites) is up: Copy or Cancel */
    int physics_pick;            /* the car it names, 0..CAR_PHYSICS_WORDS-1 (editor/car_physics_from.h); meaningful once physics_chosen */
    int physics_chosen;          /* the row's car was picked: until then the row says «not set» and Copy... is off (nothing is the car's own value there) */
    WorkspaceList workspace_list;  /* the projects in it, scanned when it is set and when the window starts */

    char zip[APP_PATH];          /* the last package built, "" before one */
    char status[APP_MSG];        /* the last thing done, for the window's bottom line */
    char open_error[APP_MSG];    /* why the last Open, New or recent could not open its folder, until it is acknowledged (the window says it in a message); "" for none */
} App;

/* An app with nothing open: the start screen. `game`, `user` and `generic_art` as for app_open. */
void app_begin(App *a, const char *game, const char *user, const char *generic_art);

/* Opens what `dir` holds, for the folder the command line gave: a track project (it has a project.ini), else a car's kit (a mod.ini and a model/ in it),
 * else a new track project there (app_open). 1 with the app ready, 0 with status naming the fault. */
int app_open_any(App *a, const char *dir, const char *game, const char *user, const char *generic_art);

/* Opens the track project in `dir`, creating its files where they are missing (project_create: `generic_art` is the editor's GENERIC.FAM or NULL).
 * 1 with the app ready, 0 with status naming the fault. */
int app_open(App *a, const char *dir, const char *game, const char *user, const char *generic_art);

/* Test in race (the game in its window, on a scratch player folder, with this project's masks) and Race photo (the same headless, one picture in a->race.photo): for
 * the open car when it is one of the game's (car_race_original_of). Refused, with the status saying why, when no car is open, it is not an original, the game is not
 * there or a race goes. app_race_poll (every frame) takes the output and the picture; 1 while a job runs. While one runs nothing else is opened (app_ask). */
int app_race(App *a, int photo);
int app_race_poll(App *a);
void app_race_stop(App *a);
int app_race_busy(const App *a);

/* Test drive of the open track (the Drive window): refused, with the status saying why (the Problems' text for a road the game does not race, no game), else the
 * piece is built (app_build) and the game runs on it in its window as a time trial, from a scratch player folder it is installed into, never a->user, with the
 * car a->drive_pick and the look a->race_look. The job is a->race: app_race_poll, app_race_stop and app_race_busy run it as Test in race's. 1 when it started. */
int app_test_drive(App *a);

/* Where the game's files are, for Export original car; also remembered across the projects opened from this window. */
void app_set_export_source(App *a, const ExportSource *src);
/* Makes the kit of the exported car a->export_pick (car_export_kit: the current layout, the migration's own shape) in a folder named for its stem inside `parent` and opens it: the window stays as it was, with
 * open_error saying why, when it cannot. 1 when it is open. */
int app_export_car(App *a, const char *parent);

/* The Masks window (masks are painted elsewhere; Studio shows them). app_mask_scan lists the textures of the open car's file and which have a mask in car_masks/<folder>/ (done when a car opens and after Convert PSD);
 * app_mask_open shows texture `index`'s mask over its texture, app_mask_close lets it go. app_mask_open_by_name takes "topv" or "frnt_lo" (the mask file's name without _mask.png). Each sets the status and mask.error when it refuses. */
void app_mask_scan(App *a);
int app_mask_open(App *a, int index);
int app_mask_open_by_name(App *a, const char *name);
void app_mask_close(App *a);
/* Undo and redo: a track's project (track_undo.h); a car's card edits (car_edit.h) */
int app_undo(App *a);
int app_redo(App *a);
int app_can_undo(const App *a);
int app_can_redo(const App *a);
/* How texture `index`'s mask stands, for the Masks window's list: "in sync", "PSD newer" or "PSD found, no mask yet" when the car has PSDs to compare with
 * (a->stale, a->stale_ok), else "PNG only" (the PNG is the source) or "no mask". Never NULL. */
const char *app_mask_state(const App *a, int index);
/* the state of the dash's mask against its dash.psd, as app_mask_state words it; NULL when the car has no dash.psd to show (the Masks window's read-only row) */
const char *app_mask_dash_state(const App *a);
/* «Reveal masks folder»: asks the window to show the folder the masks are edited in (car_project_edit_dir: a kit's art-source/masks, not the generated PNGs); 0, with the status saying why, when it is not there */
int app_reveal_masks(App *a);
/* Masks changed on disk by another program (W4). app_mask_watch looks at the car's mask files when `now` (seconds, any clock that only grows) is a
 * second or more after its last look: a changed, new or removed file moves mask.generation (the Preview and the Model read the masks again) and
 * has_mask; the open mask is read again (closed when its file went). 1 when something changed. */
MaskStamp app_file_stamp(const char *path);   /* a file's time and size now; zeros when there is none */
int app_mask_watch(App *a, double now);
/* The same for the card and the car's files (W4, app_card_watch.c). mod.ini changed by another program: read again when the card has no unsaved
 * edits; else the edits stay, cdisk.card_changed says «Changed on disk», Save refuses until app_card_reload (the file's card over the edits) or
 * app_card_keep_mine (the next Save writes the edits over the file). The car file or a texture of modelo/c1 or c3 changed: the lists are read again; the car file
 * also moves cdisk.serial and mask.generation (Model, Preview), and the open mask is opened again.
 * 1 when something changed. app_card_arm records the files as they are now (a car opened or read again). */
int app_card_watch(App *a, double now);
int app_card_reload(App *a);
void app_card_keep_mine(App *a);
void app_card_arm(App *a);
int app_card_save_ok(App *a);   /* 0, with the status, when mod.ini changed on disk under the card's edits */
/* A track's edit taken as one step to undo when it differs from the last (the window calls it when no field is typed in and no point dragged) */
void app_track_commit(App *a);
/* A track's step back (redo: forward); the road is made again from it, the selection kept when its point is still there. 1 when there was one. */
int app_track_undo(App *a, int redo);

/* The Track light window (dep98 step 2, part C; app_light.c). app_set_light_dir says which folder it tunes (a->light_dir). app_open_light closes what is open (the
 * caller asked first: app_ask APP_ACT_OPEN_LIGHT) and opens the window on that folder, at the first track's day; 0, with open_error, when the folder is no light folder. */
void app_set_light_dir(App *a, const char *dir);
int app_open_light(App *a);
int app_open_light_on(App *a, int track);   /* the same on the light folder a->light_dir, at that track (light_ui_track's index) */
/* The game's own light folder: <program's folder>/../Resources/data (a packaged app) or <program's folder>/data, the first that holds a track_light/ folder; 1 and its
 * path in `out`, else 0. A packaged Studio has no source tree to tune, so this is what it offers until a folder is chosen. */
/* 1 when a path runs through a folder named *.app (Save refuses those; no default is taken there) */
int app_light_in_bundle(const char *path);
int app_light_default_dir(const char *game, char *out, size_t cap);

/* Original tracks (dep103; app_original.c). app_set_game_data says where the game's data/ and the art folder are (either may be ""; an empty art folder is looked for beside the
 * source tree). app_original_add_all makes the sixteen CD tracks projects of the workspace (copies, nothing overwritten); 1, or 0 with the reason in status. app_open_original closes what is
 * open (the caller asked first) and opens the Track light window on that original track's folder: its own light files, the track fixed. app_original_copy_ask makes the preview
 * (publish_text) and puts the window up; app_original_copy copies the changed light files into data/. */
void app_set_game_data(App *a, const char *data_dir, const char *art_dir);
/* The game repo's data/ folder: where Copy to game repo writes (cars' mask PNGs, an original track's light files and mask PNGs). "" unless it was given, by --port-data (app_set_repo_arg) or by
 * the kept setting (app_repo_data_set, or "repo data<TAB><path>" in workspace.txt by hand): the copy action is hidden without it, there is no dialog and nothing on the hub. Never in a packaged app's bundle. */
const char *app_repo_data(const App *a);
void app_set_repo_arg(App *a, const char *dir);
int app_repo_data_set(App *a, const char *dir_or_empty);   /* keeps it; 1 when written */
int app_original_add_all(App *a);
int app_original_count(const App *a);   /* how many of the sixteen are in the workspace */
int app_open_original(App *a, const char *dir);
int app_original_can_copy(const App *a);
int app_original_copy_ask(App *a);
int app_original_copy(App *a);
/* Copy to game repo for one of the game's own cars (app_car_copy.c, editor only): the changed and new masks of its kit against data/car_masks/<folder>/ of the repo.
 * app_car_copy_can: the car is one of the game's, has masks and the repo's data/ is set; app_car_copy_ask lists them in publish_text and puts the window up (publish_car);
 * app_car_copy writes them. 1, or 0 with the reason in status. (app_original_copy_ask and app_original_copy do these for a car.) */
int app_car_copy_can(const App *a);
int app_car_copy_ask(App *a);
int app_car_copy(App *a);
/* The Masks panel of an original track (app_track_masks.c, editor only). app_track_masks_load scans the folder and makes the PNGs of the PSDs that need them (NEW and STALE), once
 * per open; app_track_masks_refresh scans again. app_track_mask_select puts row `i`'s picture in tmask_rgba; app_track_mask_regenerate makes the selected one's PNG from its PSD
 * again (after the warning: it replaces a PNG edited by hand). 1, or 0 with the reason in tmask_note. */
/* The migration check: for every original track of the workspace the PNGs are made from the PSDs (what a track does when it opens), then each is compared, as marks, with data/track_masks.
 * `out` gets one line: "originals: 16 tracks, 107 masks: 107 equal, 0 differ, 0 new in the workspace; acceptance: 0 differences outside art-source/masks" (each track made by hand through the same code path and compared: only the mask folders may differ). 1 when none differs. */
int app_original_check_all(App *a, char *out, size_t cap);
int app_track_masks_load(App *a);
int app_track_masks_refresh(App *a);
/* Once a second, with the window's clock: a PSD saved again makes the PNGs again, the rows refresh and the photos are taken again with the new masks. 1 when it did. */
int app_track_masks_poll(App *a);
int app_track_mask_select(App *a, int i);
int app_track_mask_regenerate(App *a);
/* The window on another light folder (the Choose... button): refused, with light.error and the status, while the working copy has unsaved changes or when the folder
 * is no light folder (light_session_open's reason); then nothing changes. 1 when it is on the folder, kept as a->light_dir for the rest of the session. */
int app_light_set_folder(App *a, const char *dir);
/* The track (light_ui_track's index) and time (0..3) of the file: refused, with light.error, while the working copy has unsaved changes (Save or Revert first). */
int app_light_select(App *a, int track, int time);
/* The photo's node, clamped to the file's road. */
void app_light_set_node(App *a, long node);
/* The photo's camera (light_photo_view_name), clamped; the person's choice for the next sessions too (light_photo_view_pref_set). */
void app_light_set_view(App *a, int view);
/* The HUD in the photo (off by default: the photo is for the light); kept like the view. */
void app_light_set_hud(App *a, int hud);
/* The window edited a->light.session.file (light_file_set_*): the photo is due. */
void app_light_touch(App *a);
/* Revert and Save of the file (light_session.h); Save says why it refused in light.error and the status. */
void app_light_revert(App *a);
int app_light_save(App *a);
/* The photo: takes one now of the working copy at the track, time and node (the game headless, light_photo.h); refused, with light.error, with no game, no CD data or no file. */
int app_light_photo(App *a);
/* Every frame, with the clock `now` in seconds: takes the photo job's output and picture, and starts the photo that is due once `now` is LIGHT_UI_SETTLE past the last
 * change. 1 while a job runs. */
int app_light_poll(App *a, double now);
/* part D: what is shown beside the photo, LIGHT_COMPARE_OFF, _BEFORE (the files as they are on disk) or _FOUR (the four times of day); the pictures are made on the poll */
void app_light_set_compare(App *a, int mode);
/* The test drive (light_drive.h): the game in a window of its own on this track, time and node, the working copy in front of the real files; each edit rewrites the copy and the
 * game reads it again without Save. A change of track, time, node or weather restarts it (once it settles), so the game never shows the old one. The photos wait while it runs
 * (one game at a time). app_light_drive starts it (0 with the status saying why: the game, the files, another game running); app_light_drive_stop ends it. */
int app_light_drive(App *a);
void app_light_drive_stop(App *a);
int app_light_drive_running(const App *a);
void app_light_set_weather(App *a, int weather);
int app_light_busy(const App *a);
/* lets go of the window's session and photo (app_free) */
void app_light_free(App *a);
/* what belongs to the window, not to what is open, moves to `to` from `from` when one kind replaces another: the recent list, the game lookup, where the game's files are, the settings */
void app_carry_window(App *to, const App *from);

void app_free(App *a);
/* Why Build would refuse the road as it is now, in Build's words, when no red row of Problems says it already: the road not made, or the
 * generator's refusal of nodes that break no red limit. Problems shows it in red before Build. NULL when Build takes the road, or a red node
 * (which Build names first) stops it. After app_recompute. */
const char *app_road_refusal(const App *a);

/* The window's title: "Spirited Drive Studio", then " \xe2\x80\x94 <project's id>" with a project open and " *" when it has unsaved changes (app_modified). */
void app_title(const App *a, char *out, size_t cap);

/* 1 when the open project has unsaved changes: a track's trace is not what trace.json holds, a car's card is not what mod.ini holds (a point moved, added or removed, or the width changed since the last save or open). */
int app_modified(const App *a);

/* Asks for an action. With nothing unsaved it is a->go at once; else a->confirm waits for app_answer, and a->go stays NONE. An action asked for
 * while another waits is dropped. `path` is the folder of APP_ACT_RECENT, or "" . */
void app_ask(App *a, AppAction act, const char *path);

/* The answer to the waiting confirmation: APP_ANSWER_SAVE saves and goes on (if the save fails it stays waiting, with the status saying why),
 * APP_ANSWER_DISCARD goes on without saving, APP_ANSWER_CANCEL drops the action. */
void app_answer(App *a, int answer);
/* the folder picker was closed without a folder: the answer given before it is dropped (the next open asks again) */
void app_pick_cancelled(App *a);

/* Closes the project and opens the one in `dir` (APP_ACT_RECENT, and the folder the Open and New dialogs gave): 1 when it is open. On a failure the
 * project stays as it was, with the status saying why. app_open_project wants a project.ini in the folder; app_new_project wants none (it makes one). */
int app_open_project(App *a, const char *dir);
int app_new_project(App *a, const char *dir);
/* Opens the car in `dir` (a kit's folder: car_project_detect); on a failure the status says what was looked at and what is missing, and the open
 * project stays as it was. */
int app_open_car(App *a, const char *dir);
/* Opens a recent folder as the kind it was kept as. */
int app_open_recent(App *a, const char *dir);

/* Keeps the recent projects in `file` (loaded now, with the project that is open put first, saved at every change); "" or NULL keeps none. */
void app_keep_recent(App *a, const char *file);
/* the room what is open belongs to */
AppRoom app_room(const App *a);
/* Closes the project and shows the main menu (nothing open); the window's own things stay (recent list, workspace, game lookup). The caller asked about unsaved changes first (app_ask APP_ACT_MAIN). 1 when it did. */
int app_close_project(App *a);
/* The workspace (workspace.h): the file keeps the choice between runs; a folder that is gone is no workspace. */
void app_workspace_keep(App *a, const char *file);
/* `dir` becomes the workspace: its cars/ and tracks/ are made when missing, what they hold is listed, the choice is kept. 1 when it did; 0 with the status saying why. */
int app_workspace_set(App *a, const char *dir);
/* No workspace, and the offer to choose one is put aside (File > Workspace > No workspace) */
void app_workspace_clear(App *a);
/* the start screen's offer to choose a workspace is put aside */
void app_workspace_put_aside(App *a);
/* 1 when the start screen offers to choose a workspace: none is set and the offer was not put aside */
int app_workspace_offer(const App *a);
/* the folder of the I-th car (kind 0) or track (kind 1) the workspace lists into `out`; 0 when there is none */
int app_workspace_item(const App *a, int kind, int i, char *out, size_t cap);
/* New track with a workspace: the track `name` is made as <workspace>/tracks/<id> (`id` NULL or "" takes the one the name makes) and opened. 0, with the reason in the status and
 * new_track_error and nothing written, when there is no workspace, the name or id is not usable, or that folder exists. */
int app_new_track_named(App *a, const char *name, const char *id);
/* New car (dep102; app_new_car.c): a kit <workspace>/cars/<id> made from the car `template_word` (one of the eight you drive, viper, 911 ...) by car_new_kit, opened and listed. 1, or 0 with the
 * reason in new_car_error and the status and nothing written, when there is no workspace, the name or id is not usable, that folder exists, or the game's files are not found. A null `id` is made from the name. */
int app_new_car_named(App *a, const char *name, const char *id, const char *template_word);
/* Export original track (app_export_track.c): the CD track `stem` (TR1..TR7: the tracks the editor can build) becomes the track project <workspace>/tracks/<id>, its road (bends, hills, width, banking) taken from
 * the game's TRI, the art Studio's own; opened and listed. 1, or 0 with the reason in export_track_error and the status and nothing written, when there is no workspace, the name or id is not usable, that folder
 * exists, the game's files are not found or the track is too long. A null `id` is made from the name. */
int app_export_original_track(App *a, const char *stem, const char *name, const char *id);
/* Physics from (dep102; app_physics.c): the physics of `word`'s car (car_physics_words) copied into the open car kit's specs/specs.tsv and specs/pdn.tsv, as car_physics_from does. Refused
 * in the status, and nothing written, when the open car is not a kit in the current layout, is one of the game's own, or the files are not there. 1 when copied (the status says how many rows). Build makes the car with them. */
int app_physics_from(App *a, const char *word);
/* the workspace lists read again (after a project was made in it) */
void app_workspace_refresh(App *a);
/* The start screen's Recent list: entry `index` taken out, or every entry of `kind` (RECENT_CAR / RECENT_TRACK); the list file is written again. */
void app_recent_remove(App *a, int index);
void app_recent_clear(App *a, int kind);
/* the folder dialog for a workspace, from the menu or the start screen: it does not ask about unsaved changes (it changes no project) */
void app_workspace_pick(App *a);
/* where the folder dialog `kind` (APP_ACT_NEW, _OPEN, _OPEN_CAR, _EXPORT_PICK, _WORKSPACE_PICK) starts: the workspace's cars/ or tracks/, else the open project's parent, else "" */
void app_dialog_dir(const App *a, AppAction kind, char *out, size_t cap);

/* Computes the road when the trace changed (a no-op otherwise). */
void app_recompute(App *a);
/* the node nearest control point `i`, by its distance along the road (0 with no road) */
int app_nearest_node(const App *a, int i);

/* The Node window shows a node: the one picked in Problems (app_select_node), else the one nearest the selected control point. app_sync_selection
 * follows a change of the selected point; app_select_node makes `node` the shown node and selects the control point nearest it. */
void app_sync_selection(App *a);
void app_select_node(App *a, int node);

/* The node count the road has, or would have: 4 per segment and one more, from its length. */
int app_node_count(const App *a);

/* Says in the status line, when the game's program is not where `game` points (not there), what to do about it: copy both
 * apps to Applications, or give --game. 1 when the game is there. app_install says it too, instead of a failed exec. */
int app_check_game(App *a);
/* the places the game was looked for and the file that remembers a located game (main.c, game_find); kept for the not-found message */
void app_set_game_lookup(App *a, const char *tried, const char *memo);
/* the user located the game: `picked` is its app or its program (game_find_program_of); 1 when it is a file, it is then the game and is remembered; 0 says why in the status */
int app_set_game(App *a, const char *picked);
/* 1 when the game's program is there */
int app_game_found(const App *a);

int app_save(App *a);

/* The showcase photos (app_photos.c; the card's photos= line is CE_PHOTOS, so each is one undo step). Add copies the picture at `source` into the car's folder under a safe
 * name (size-checked: 100x60 to 1280x960) and names it last; 1, or 0 with the reason in a->status. Remove and Move (by -1 up, +1 down) change the list only. */
int app_add_photo(App *a, const char *source);
int app_photo_remove(App *a, int index);
int app_photo_move(App *a, int index, int by);
/* after a Save of the card: the files Studio made that the card does not name are removed */
void app_photos_swept(App *a);
/* 1 with the reason in `why` when the card lists a photo the car's folder has no file for (Save refuses: the piece would fail its check) */
int app_photos_missing(const App *a, char *why, size_t cap);
/* the folder the showcase pictures are in (car_project_photos_dir) */
void app_photos_dir(const App *a, char *out, size_t cap);
/* 1 when the car's folder has a file called `name` */
int app_photo_file_ok(const App *a, const char *name);
int app_build(App *a);
/* Install starts the game's installer and returns (1 when it started): app_install_poll (every frame) says its end in the status, "installed into ..." or
 * "install failed: ..."; 1 while it runs. One at a time; no test drive and nothing opened while it runs. */
int app_install(App *a);
int app_install_poll(App *a);
/* A kit with no car file is built when it is opened (a thread: Preview says "building..." and the window stays alive). Every frame: 1 while the build runs; when it
 * ends the status says so, and the car is read again (its textures, its masks, its file). A headless run waits for it. */
int app_car_build_poll(App *a);
/* Check: the kit's report (what is wrong before a build: the model, its wheels, its masks) in the build's thread; app_car_build_poll says its end, and a->car_build.list holds
 * the findings when a->car_build.checked. 1 when it started */
int app_check(App *a);
int app_car_building(const App *a);
/* 1 when the open car is a kit in the current layout: the shape Build and Install are for (build_car: the build in a thread, then Install takes build/<id>.zip) */
int app_car_is_kit(const App *a);
int app_install_busy(const App *a);
/* Convert PSD (dep98): app_psd_setup finds the PSD folder (`root`, else TNFS_PSD_ROOT, else
 * ~/workspace/bytes-n-rust/art/tnfs-masks). app_psd_psd_dir gives this car's PSD folder (names match without case; 0 when there is none). app_psd_start converts
 * the PSDs (the C reader, in a thread) into a scratch folder and copies the PNGs into the project only when all were made.
 * app_psd_poll (every frame) tells the result in the status and reads the car's lists again (the open mask is closed: it was just remade). 1 while it runs. */
void app_psd_setup(App *a, const char *base, const char *root);
int app_psd_psd_dir(const App *a, char *out, size_t cap);
int app_psd_start(App *a);
/* The PNGs the masks have that were edited by hand since Studio made them (the manifest's hash is not theirs): how many, and their names ("frnt_mask.png, dash_mask.png"). Convert PSD keeps them. */
int app_psd_edited(const App *a, char *names, size_t cap);
/* Regenerate from PSD: Convert PSD that replaces the PNGs edited by hand too; 1 when it started */
int app_psd_regenerate(App *a);
/* where this car's masks, dash masks and manifest are (car_project.h): a kit's art-generated, else the piece's folders */
void app_psd_dirs(const App *a, char *masks, char *dash, char *manifest, size_t cap);
/* Convert PSD by itself (the stale refresh calls it with what it found): when some PSD is newer than its mask or has none, it starts, once for each state: a state it
 * already tried (a conversion that failed, or left a PSD with no mask) is not tried again until a PSD is saved again, or another is stale, or Convert PSD is pressed.
 * 1 when it started. */
int app_psd_auto(App *a, const StaleReport *r);
int app_psd_poll(App *a);
int app_psd_busy(const App *a);
void app_psd_stop(App *a);
/* the car's lists (project, textures, masks) and its card read again, as after Convert PSD */
void app_car_reread(App *a);
void app_install_stop(App *a);   /* asks the installer to end (a hung one): app_install_poll says "install failed: the install was stopped" */

/* Points: a new point after the last one, along the last segment; one after the selected point, midway to the next (or after the last);
 * the selected point removed (a trace keeps TRACE_POINTS_MIN points). Each sets the status when it refuses. */
void app_add_point(App *a);
void app_insert_point(App *a);
/* a point put in at fraction `frac` (APP_INSERT_MARGIN .. 1 - APP_INSERT_MARGIN) of the way from point `seg` to the next, on the curve (its height there), and selected; 1 when it was, 0
 * with the reason in the status (off the road, too near a point, or no room) */
#define APP_INSERT_MARGIN 0.02
int app_insert_point_at(App *a, int seg, double frac);
/* the strip's click on the profile remembers its place; Insert point here puts a point there (app_insert_point_at) and forgets it */
void app_profile_place(App *a, int seg, double frac);
int app_insert_here(App *a);
void app_remove_point(App *a);
/* A control point's height in metres above the start (the Points table's "h"; dep95): kept to a tenth of a metre and inside +-TRACE_HEIGHT_MAX_M. An unsaved change when it
 * changes anything; a project with a height is written as trace.json version 5. */
void app_set_point_height(App *a, int point, double metres);
/* The road width key of a control point (the Points table's "width (m)"): set it (clamped to the road's range and to a quarter of a metre), or clear it so
 * the point inherits again. Either is an unsaved change when it changes anything. */
void app_set_point_width(App *a, int point, double metres);
void app_clear_point_width(App *a, int point);
/* The shoulder surface of a side (TRACK_GEN_LEFT / _RIGHT) of a control point's road key (the Road key block): verge or tarmac sets it, TRACE_SURFACE_INHERIT
 * lets that side inherit again. An unsaved change when it changes anything. */
void app_set_point_surface(App *a, int point, int side, TraceSurface surface);
/* 1, with the line for Problems in `text`, when a road key makes a shoulder tarmac and the project's art is not Studio's current generic: its asphalt bank is
 * Studio's own (TRACK_GENERIC_SLOT_TARMAC), an art of the author's may show anything there. Nothing is changed in the art */
int app_tarmac_art_note(const App *a, char *text, size_t cap);
/* The road's sides (step 3; app_sides.c): a side value of point `point` (TRACE_SIDE_*, TRACK_GEN_LEFT / _RIGHT): widths kept to a quarter of a metre in
 * 0..TRACE_SIDE_MAX_M, a wall to hard or soft, a fence to on or off. A change makes the project modified; the same value again, or clearing a value the point
 * does not set, does not. A key with nothing left goes. */
/* dep87 part 2: the set_point calls (width, bank, surface, side, lanes, fence) change only what is selected. When `point` is one of several selected points the value is set on all of
 * them (a run: on its first point, ramped values on its last too) and the value that held before comes back right after each run (key_ops.h
 * keys_set_scoped); on a point that is not in a multi-selection it is that one point alone. `value`'s `has` says which values, its fields what. 0 with the
 * status set when memory is out; an unsaved change when it changes any key. */
int app_set_scoped(App *a, int point, const TraceKey *value);
/* «Extend to next change» (`to_end` 0) and «Extend to the end» (1) for the values of `mask` (TRACE_KEY_ bits) set on `point`, or on the run of selected points it
 * is in (from the run's last): key_ops.h keys_extend. An unsaved change when it removes a key. */
void app_extend_point(App *a, int point, unsigned mask, int to_end);
/* Copy profile / Paste profile: the road values (width, bank, shoulders, walls, fences, lanes, surfaces) of the selected point, put on the selected point or points
 * as a scoped edit (app_set_scoped: the value that held comes back after them), one undo step. 1 when it did it; 0 with the reason in the status. */
int app_copy_profile(App *a);
int app_paste_profile(App *a);
void app_set_point_side(App *a, int point, int field, int side, double value);
void app_clear_point_side(App *a, int point, int field, int side);
/* The fence step (app_sides.c): on point `point`'s road key, `bit` TRACE_KEY_FENCE_HEIGHT (0 low, 1 tall), TRACE_KEY_FENCE_ART (a TRACE_FENCE_ picture) or
 * TRACE_KEY_BARRIER_CUSTOM(side) (1: the barrier is that side's Barrier from edge, 0: it follows the fence). A value out of range changes nothing; clearing
 * lets the point inherit again. An unsaved change when it changes anything. */
void app_set_point_fence(App *a, int point, unsigned bit, int value);
void app_clear_point_fence(App *a, int point, unsigned bit);
/* what point `point` has of them as the keys make it; 0 for no such point or out of memory */
int app_point_fence(const App *a, int point, TraceFencePoint *out);
/* 1, with the amber line for Problems in `text`, when a key turns a side's Custom on: the barrier is not where the fence is drawn */
int app_barrier_custom_note(const App *a, char *text, size_t cap);
/* the bank's key on a point: clamped to +-TRACE_BANK_INPUT_MAX_DEG and to a tenth of a degree; clearing it inherits again */
void app_set_point_bank(App *a, int point, double degrees);
void app_clear_point_bank(App *a, int point);
/* The lanes of point `point`'s road key (step 4; app_sides.c): TRACK_GEN_LANES_1_1, _1_2 or _2_1 (anything else changes nothing), or cleared so the point
 * takes the lanes before it again. An unsaved change when it changes anything. */
void app_set_point_lanes(App *a, int point, int lanes);
void app_clear_point_lanes(App *a, int point);
/* the lanes point `point` has as the keys make them (1+1 before any lanes key; 0 for no such point) */
int app_point_lanes(const App *a, int point);
/* 1, with the amber line for Problems in `text`, when a width key sits inside a 1+2 or 2+1 stretch: the lanes set the road's width there, the key is ignored */
int app_lane_width_note(const App *a, char *text, size_t cap);

#endif

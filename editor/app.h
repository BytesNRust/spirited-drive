/* The editor's state and what it does to a project: the trace is edited in place, the road (nodes and limits) is recomputed from it when it
 * changes, and save / build / install run on the project folder. The window (main.c) and the drawing (draw.c) only read and call these. */
#ifndef TNFS_TRACK_EDITOR_APP_H
#define TNFS_TRACK_EDITOR_APP_H

#include "car_edit.h"
#include "mask_edit.h"
#include "mask_tex.h"
#include "night_view.h"
#include "car_export.h"
#include "car_rehacer.h"
#include "car_project.h"
#include "car_race.h"
#include "project.h"
#include "session.h"
#include "view.h"

#include <stdint.h>

enum {
    APP_PATH = 4096,
    APP_MSG = 320,
    APP_NEW_STEP = 40        /* units between a point and the one added after it */
};

/* what the window has open: nothing (the start screen), a track project or a car project. Exactly one; the fields of the other kind are not read. */
typedef enum { APP_KIND_NONE = 0, APP_KIND_TRACK, APP_KIND_CAR } AppKind;

/* what the window was asked to do that ends or changes the project: quit, start a new project, open one, open a recent one */
typedef enum {
    APP_ACT_NONE = 0,
    APP_ACT_QUIT,
    APP_ACT_NEW,       /* a folder for the new project is asked for (main.c's dialog) */
    APP_ACT_OPEN,      /* a folder holding a project is asked for */
    APP_ACT_RECENT,    /* the folder is in the path */
    APP_ACT_NEW_AT,    /* the folder the New dialog gave, in the path: asked through the same question, as the project may have changed meanwhile */
    APP_ACT_OPEN_AT,   /* the same for Open */
    APP_ACT_OPEN_CAR,  /* a car's folder is asked for (its kit or its coche/) */
    APP_ACT_OPEN_CAR_AT, /* the folder that dialog gave, in the path */
    APP_ACT_EXPORT_PICK, /* a folder to make the exported car's project in is asked for (main.c's dialog); the car is a->export_pick */
    APP_ACT_EXPORT_AT    /* the folder that dialog gave, in the path: the project is made in it and opened */
} AppAction;

/* the answers to "unsaved changes" */
enum { APP_ANSWER_CANCEL = 0, APP_ANSWER_DISCARD = 1, APP_ANSWER_SAVE = 2 };

/* the mask editor of the open car (dep59 car editor E3): the textures of its CFM, the mask being edited and the tools */
typedef enum { MASK_TOOL_PENCIL = 0, MASK_TOOL_FILL, MASK_TOOL_PICK } MaskTool;

typedef struct {
    MaskTexInfo tex[MASK_TEX_LIST];   /* the textures of the car file, both banks */
    int tex_count;
    char has_mask[MASK_TEX_LIST];     /* the car's masks folder holds the texture's mask */
    int sel;                          /* the texture being edited (index into tex), -1 for none */
    MaskEdit me;                      /* its mask, when sel >= 0 */
    char note[256];                   /* the size note of the open mask, "" when it matches its texture */
    char error[APP_MSG];         /* why a mask could not be opened or saved, until the next try */
    int tool, brush, cls;             /* MaskTool; the pencil's size 1..8; the legend index of the class the pencil and fill paint */
    int fill_by_texture;              /* 0: the fill takes the texels of the same class, 1: of the same texture colour */
    int show, class_on[ME_CLASSES];   /* the overlay on or off, and each class of it */
    float opacity;                    /* 0..1 */
    float zoom;                       /* screen pixels a mask texel; 0: fit the window (the panel sets it) */
    unsigned generation;              /* counts every mask opened, closed, discarded or saved: a picture made from the mask compares with it */
    int focus;                        /* the Edit mask window has the keyboard: Ctrl+Z is the mask's (set by the panel every frame) */
    int saved_since_race;             /* a mask was saved: Race photo shows the result */
} MaskUi;

typedef struct {
    AppKind kind;                /* what is open: the track fields below are read for APP_KIND_TRACK, `car` for APP_KIND_CAR */
    CarProject car;              /* the car project, when kind is APP_KIND_CAR */
    Rehacer rehacer;             /* the kit's rehacer.command, running or last run (car_rehacer.h) */
    int preview_paint;           /* the card's paint the Preview shows: an index of the card's paints, -1 for the car as its file has it */
    char preview_focus[5];       /* a texture the Preview scrolls to (--preview-tex NAME, for a headless picture); "" none */
    NightSettings night;         /* the Preview's Day / Night and lamp switches (editor/night_view.h) */
    MaskUi mask;                 /* the mask editor (editor/mask_edit.h), when kind is APP_KIND_CAR */
    CarEdit edit;                /* its card as it is being edited (car_edit.h), when kind is APP_KIND_CAR; only mod.ini is ever written */
    char dir[APP_PATH];          /* the project folder: a track's, a car's kit; "" with nothing open */
    char game[APP_PATH];         /* the game program, run for install */
    char art_note[160];          /* what opening the project did to or noticed about its copy of the generic art (project_art_refresh); "" for nothing */
    char game_tried[2048];       /* every place the game was looked for (game_find), for the message when it is not there */
    char game_memo[APP_PATH];    /* the file that remembers the game the user located ("" for none) */
    char user[APP_PATH];         /* the player folder the install goes to */
    ExportSource export_src;     /* where Export original car reads the game's files from (car_export_source) */
    int export_dialog;           /* the Export original car window is up */
    int export_pick;             /* the car chosen in it (car_export_car's index), -1 for none */
    RaceJob race;                /* Test in race / Race photo of the open original car (car_race.h), running or last run */
    int race_night;              /* the preset: 0 Day, 1 Night */
    int race_track;              /* the track chosen (car_race_track's index) */
    RaceLook race_look;          /* the Race window's look switches (T6): the session's, never written to the car's project; all on by default */
    char reveal[APP_PATH];       /* a folder the window is asked to show in the Finder (main.c does it and clears this) */
    Project project;
    Trace trace;
    View view;
    int selected;                /* index of the selected control point, -1 for none */
    int dirty;                   /* the trace changed since the road was computed */

    /* the road, computed from the trace by app_recompute */
    TrackGenNode *nodes;         /* malloc'd when road_made */
    int node_count;
    uint8_t *flags;              /* per node, the TRACE_FLAG_* bits, when road_made */
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
    int node;                    /* the node the Node window shows, -1 for none */
    int node_for;                /* the control point `node` was taken from (app_sync_selection) */
    char road_text[APP_MSG];     /* why the road is not made; "" when it is */

    char saved_edge[8];          /* the verge material as it is in project.ini: app_modified compares with project.edge */
    uint64_t saved_print;        /* session_fingerprint of the trace as it is in trace.json: app_modified compares with it */
    AppAction confirm;           /* an action waiting for the answer to "unsaved changes" */
    char confirm_path[APP_PATH];
    AppAction go;                /* an action to run now (main.c): the confirmation is answered, or there was nothing unsaved */
    char go_path[APP_PATH];
    int fit_pending;             /* the view is fitted to the trace on the next frame (a project was just opened) */
    char generic[APP_PATH];      /* the editor's own GENERIC.FAM, for the projects it creates ("" when there is none) */
    Recent recent;               /* the project folders opened lately, newest first */
    char recent_file[APP_PATH];  /* where they are kept, "" when they are not */

    char zip[APP_PATH];          /* the last package built, "" before one */
    char status[APP_MSG];        /* the last thing done, for the window's bottom line */
    char open_error[APP_MSG];    /* why the last Open, New or recent could not open its folder, until it is acknowledged (the window says it in a message); "" for none */
} App;

/* An app with nothing open: the start screen. `game`, `user` and `generic_art` as for app_open. */
void app_begin(App *a, const char *game, const char *user, const char *generic_art);

/* Opens what `dir` holds, for the folder the command line gave: a track project (it has a project.ini), else a car's kit (a mod.ini in it or in its coche/),
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

/* Where the game's files are, for Export original car; also remembered across the projects opened from this window. */
void app_set_export_source(App *a, const ExportSource *src);
/* Makes the project of the exported car a->export_pick in a folder named for it inside `parent` and opens it (car_export): the window stays as it was, with
 * open_error saying why, when it cannot. 1 when it is open. */
int app_export_car(App *a, const char *parent);

/* The mask editor (E3). app_mask_scan lists the textures of the open car's file and which have a mask in car_masks/<folder>/ (done when a car opens and after Rehacer);
 * app_mask_open edits texture `index` (what is open must not have changes: the panel asks first), app_mask_save writes the mask (same size, the edit only),
 * app_mask_close lets it go. app_mask_open_by_name takes "topv" or "frnt_lo" (the mask file's name without _mask.png). Each sets the status and mask.error when it refuses. */
void app_mask_scan(App *a);
int app_mask_open(App *a, int index);
int app_mask_open_by_name(App *a, const char *name);
int app_mask_save(App *a);
void app_mask_close(App *a);
/* Undo and redo of what has the keyboard: the mask when the Edit mask window does, else the card */
int app_undo(App *a);
int app_redo(App *a);
int app_can_undo(const App *a);
int app_can_redo(const App *a);
int app_mask_modified(const App *a);

void app_free(App *a);

/* 1 when the open project has unsaved changes: a track's trace is not what trace.json holds, a car's card is not what mod.ini holds (a point moved, added or removed, or the width changed since the last save or open). */
int app_modified(const App *a);

/* Asks for an action. With nothing unsaved it is a->go at once; else a->confirm waits for app_answer, and a->go stays NONE. An action asked for
 * while another waits is dropped. `path` is the folder of APP_ACT_RECENT, or "" . */
void app_ask(App *a, AppAction act, const char *path);

/* The answer to the waiting confirmation: APP_ANSWER_SAVE saves and goes on (if the save fails it stays waiting, with the status saying why),
 * APP_ANSWER_DISCARD goes on without saving, APP_ANSWER_CANCEL drops the action. */
void app_answer(App *a, int answer);

/* Closes the project and opens the one in `dir` (APP_ACT_RECENT, and the folder the Open and New dialogs gave): 1 when it is open. On a failure the
 * project stays as it was, with the status saying why. app_open_project wants a project.ini in the folder; app_new_project wants none (it makes one). */
int app_open_project(App *a, const char *dir);
int app_new_project(App *a, const char *dir);
/* Opens the car in `dir` (a kit's root or its coche/: car_project_detect); on a failure the status says what was looked at and what is missing, and the open
 * project stays as it was. */
int app_open_car(App *a, const char *dir);
/* Opens a recent folder as the kind it was kept as. */
int app_open_recent(App *a, const char *dir);

/* Keeps the recent projects in `file` (loaded now, with the project that is open put first, saved at every change); "" or NULL keeps none. */
void app_keep_recent(App *a, const char *file);

/* Rehacer (dep59 car editor 1f): runs the open kit's own rehacer.command. Refused, with the status saying why, when no car is open, a run is going, the kit has no
 * script or the card has unsaved changes (save first). It returns at once (owner test R1): the kit is copied into .studio-backup/<time>/ (the last REHACER_KEEP
 * are kept) in its own thread, and app_rehacer_poll (every frame) starts the script when the copy has ended (a copy that fails is told, and nothing runs), takes
 * its output and, when it ends, tells its exit code in the status and reads the car again (the textures and masks lists, and the card); 1 while it copies or
 * runs. Meanwhile nothing else is opened (app_ask) and the card is not edited. */
int app_rehacer(App *a);
int app_rehacer_poll(App *a);
void app_rehacer_stop(App *a);
int app_rehacer_busy(const App *a);
/* 1 when the open car's kit has a rehacer.command */
int app_rehacer_available(const App *a);
/* Raise version (owner test R2): after a run whose install said the files changed but the version did not, the card's version one step up (car_edit_version_next)
 * into `next`. 1 when it is offered: the run ended (not stopped), the card has no unsaved change and its version is numbers. */
int app_rehacer_raise_offer(const App *a, char *next, size_t cap);
/* The button: sets the card's version to the offered one (one step to undo), saves the card and runs Rehacer again. Nothing is raised without it. 0 with the
 * status saying why (not offered, unsaved changes, the save or the run refused). */
int app_rehacer_raise_and_run(App *a);

/* Computes the road when the trace changed (a no-op otherwise). */
void app_recompute(App *a);

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
int app_build(App *a);
int app_install(App *a);

/* Points: a new point after the last one, along the last segment; one after the selected point, midway to the next (or after the last);
 * the selected point removed (a trace keeps 2 points). Each sets the status when it refuses. */
void app_add_point(App *a);
void app_insert_point(App *a);
void app_remove_point(App *a);
/* The road width key of a control point (the Points table's "width (m)"): set it (clamped to the road's range and to a quarter of a metre), or clear it so
 * the point inherits again. Either is an unsaved change when it changes anything. */
void app_set_point_width(App *a, int point, double metres);
void app_clear_point_width(App *a, int point);
/* The shoulder surface of a side (TRACK_GEN_LEFT / _RIGHT) of a control point's road key (the Road key block): verge or tarmac sets it, TRACE_SURFACE_INHERIT
 * lets that side inherit again. An unsaved change when it changes anything. */
void app_set_point_surface(App *a, int point, int side, TraceSurface surface);

#endif

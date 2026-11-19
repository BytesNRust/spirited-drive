/* The editor's state and what it does to a project: the trace is edited in place, the road (nodes and limits) is recomputed from it when it
 * changes, and save / build / install run on the project folder. The window (main.c) and the drawing (draw.c) only read and call these. */
#ifndef TNFS_TRACK_EDITOR_APP_H
#define TNFS_TRACK_EDITOR_APP_H

#include "project.h"
#include "session.h"
#include "view.h"

#include <stdint.h>

enum {
    APP_PATH = 4096,
    APP_MSG = 320,
    APP_NEW_STEP = 40        /* units between a point and the one added after it */
};

/* what the window was asked to do that ends or changes the project: quit, start a new project, open one, open a recent one */
typedef enum {
    APP_ACT_NONE = 0,
    APP_ACT_QUIT,
    APP_ACT_NEW,       /* a folder for the new project is asked for (main.c's dialog) */
    APP_ACT_OPEN,      /* a folder holding a project is asked for */
    APP_ACT_RECENT,    /* the folder is in the path */
    APP_ACT_NEW_AT,    /* the folder the New dialog gave, in the path: asked through the same question, as the project may have changed meanwhile */
    APP_ACT_OPEN_AT    /* the same for Open */
} AppAction;

/* the answers to "unsaved changes" */
enum { APP_ANSWER_CANCEL = 0, APP_ANSWER_DISCARD = 1, APP_ANSWER_SAVE = 2 };

typedef struct {
    char dir[APP_PATH];          /* the project folder */
    char game[APP_PATH];         /* the game program, run for install */
    char game_tried[2048];       /* every place the game was looked for (game_find), for the message when it is not there */
    char game_memo[APP_PATH];    /* the file that remembers the game the user located ("" for none) */
    char user[APP_PATH];         /* the player folder the install goes to */
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
    double length;               /* the centre line's length, in units (TRACE_METRES gives metres) */
    double *node_dist;           /* metres from node 0 to each node, when road_made */
    double *point_dist;          /* metres along the curve to each control point (point_count values, when road_made) */
    int point_count;             /* the control points the road was computed from: trace.count has moved on when a point was added since */
    double *point_turn;          /* the signed turn, in degrees, at the node nearest each control point */
    int node;                    /* the node the Node window shows, -1 for none */
    int node_for;                /* the control point `node` was taken from (app_sync_selection) */
    char road_text[APP_MSG];     /* why the road is not made; "" when it is */

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
} App;

/* Opens the project in `dir`, creating its files where they are missing (project_create: `generic_art` is the editor's GENERIC.FAM or NULL).
 * 1 with the app ready, 0 with status naming the fault. */
int app_open(App *a, const char *dir, const char *game, const char *user, const char *generic_art);

void app_free(App *a);

/* 1 when the trace is not what trace.json holds (a point moved, added or removed, or the width changed since the last save or open). */
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

/* Keeps the recent projects in `file` (loaded now, with the project that is open put first, saved at every change); "" or NULL keeps none. */
void app_keep_recent(App *a, const char *file);

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

#endif

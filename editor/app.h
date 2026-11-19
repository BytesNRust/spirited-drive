/* The editor's state and what it does to a project: the trace is edited in place, the road (nodes and limits) is recomputed from it when it
 * changes, and save / build / install run on the project folder. The window (main.c) and the drawing (draw.c) only read and call these. */
#ifndef TNFS_TRACK_EDITOR_APP_H
#define TNFS_TRACK_EDITOR_APP_H

#include "project.h"
#include "view.h"

#include <stdint.h>

enum {
    APP_PATH = 4096,
    APP_MSG = 320,
    APP_NEW_STEP = 40        /* units between a point and the one added after it */
};

typedef struct {
    char dir[APP_PATH];          /* the project folder */
    char game[APP_PATH];         /* the game program, run for install */
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
    int turn_nodes, radius_nodes;   /* the nodes that break a red limit (the road is not built) */
    int warn_turn_nodes, warn_radius_nodes;   /* the nodes over an amber one (a warning: it can be built) */
    double length;               /* the centre line's length, in units */
    char road_text[APP_MSG];     /* why the road is not made, or the first red limit it breaks; "" when there is none */
    char warn_text[APP_MSG];     /* the amber warning when a node is over one and none is red, else "" */

    char zip[APP_PATH];          /* the last package built, "" before one */
    char status[APP_MSG];        /* the last thing done, for the window's bottom line */
} App;

/* Opens the project in `dir`, creating its files where they are missing (project_create: `generic_art` is the editor's GENERIC.FAM or NULL).
 * 1 with the app ready, 0 with status naming the fault. */
int app_open(App *a, const char *dir, const char *game, const char *user, const char *generic_art);

void app_free(App *a);

/* Computes the road when the trace changed (a no-op otherwise). */
void app_recompute(App *a);

/* The node count the road has, or would have: 4 per segment and one more, from its length. */
int app_node_count(const App *a);

/* Says in the status line, when the game's program is not where `game` points (not there), what to do about it: copy both
 * apps to Applications, or give --game. 1 when the game is there. app_install says it too, instead of a failed exec. */
int app_check_game(App *a);

int app_save(App *a);
int app_build(App *a);
int app_install(App *a);

/* Points: a new point after the last one, along the last segment; one after the selected point, midway to the next (or after the last);
 * the selected point removed (a trace keeps 2 points). Each sets the status when it refuses. */
void app_add_point(App *a);
void app_insert_point(App *a);
void app_remove_point(App *a);

#endif

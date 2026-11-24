/* The Track light panel's "Copy to..." (notes/diseno-copiar-entre-horas-2026-11-24): a Brightness, Tone or Lamp line of one time copied to other times, never over anything unasked.
 * Pure C over light_track.h: no window, no disk. A plan says what each destination time would do, and apply does it, all or nothing. */
#ifndef TNFS_EDITOR_LIGHT_TRACK_COPY_H
#define TNFS_EDITOR_LIGHT_TRACK_COPY_H

#include "light_track.h"

#ifdef __cplusplus
extern "C" {
#endif

/* what a destination time does with the line. The line's key is brightness / tone: from, to and strip; lamp: node and strip (none is a key of its own); the rest is its value
 * (percent, a tone's colour, a lamp's colour and radius). */
enum {
    LT_COPY_NONE = 0,   /* not asked for (the source's own time, or a time left out) */
    LT_COPY_ADD,        /* no line with that key: one is added */
    LT_COPY_SAME,       /* the same key and value: nothing to do */
    LT_COPY_CLASH       /* the same key, another value: replaced only when asked */
};

typedef struct {
    int action;     /* LT_COPY_* */
    int clash;      /* LT_COPY_CLASH: the index of that line among the kind's lines of the time */
    int overlaps;   /* LT_COPY_ADD: the lines of that time (other keys) the new one shares nodes with, and points for a brightness: the file's order will matter for them */
} LightTrackCopyTime;

typedef struct {
    int kind;        /* LT_BRIGHTNESS, LT_TONE or LT_LAMP */
    int from_time;   /* the source line's time and its index among the kind's lines there */
    int index;
    LightTrackCopyTime to[LT_TIMES];
} LightTrackCopyPlan;

typedef struct { int added, replaced, same, skipped; } LightTrackCopyResult;

/* The plan for copying the `index`-th `kind` line of `from_time` to the times in `times` (bit t; the source's own time is left out). 1, or 0 with `why` (no such line, a bad kind).
 * Reads only: the file is not changed. */
int light_track_copy_plan(const LightTrack *f, int kind, int from_time, int index, unsigned times, LightTrackCopyPlan *plan, char *why, size_t cap);

/* Does the plan: ADD adds the line (in its place by node), CLASH replaces the line when `replace` is 1 and leaves it when 0, SAME does nothing. All or nothing: when the game's rules would
 * refuse any time (64 lines, 16 distinct tones, 2048 lamps) the file stays as it was and `why` says "Not copied to Dusk: ...". `result` (may be NULL) counts what was done. */
int light_track_copy_apply(LightTrack *f, const LightTrackCopyPlan *plan, int replace, LightTrackCopyResult *result, char *why, size_t cap);

/* The line's key as three numbers (brightness and tone: from, to, strip mask; lamp: node, has a strip, strip mask), to find it again after the copy has sorted the lists. 0 when there is no such line. */
int light_track_copy_key(const LightTrack *f, int kind, int time, int index, unsigned key[3]);

/* The words for the menus and the clash window: the line's key (`852-943 left`, a lamp's `node 130 left`) and the value of the line `index` of `time` (`60 %`, a tone's `ffe080 50 %`, a lamp's
 * `80 % ff2000 radius 8`). 1, or 0 when there is no such line (`out` is then ""). */
int light_track_copy_key_text(const LightTrack *f, int kind, int time, int index, char *out, size_t cap);
int light_track_copy_value_text(const LightTrack *f, int kind, int time, int index, char *out, size_t cap);

/* ---- the Tunnels table: one row for each tunnel (and each look over nodes inside one), a look in each of the four times -------------------------------------------------------------- */

typedef struct {
    int tunnel;            /* the `tunnel` fact's index, -1 for a row that is only a look over some nodes (inside a tunnel) */
    unsigned from, to;     /* the row's nodes (the key: the fact's, or the look's) */
    int lit;               /* a hall (1) takes only Lamps and Reach */
    int look[LT_TIMES];    /* the time's `tunnel_look` line over exactly these nodes (its index among the time's looks), -1 for none */
} LightTrackLookRow;

/* The rows: the facts in the file's order, then the other looks' nodes in the order the times (Dawn to Night) have them. Returns how many rows (at most `cap`). */
int light_track_look_rows(const LightTrack *f, LightTrackLookRow *rows, int cap);
/* The default row (a look with no nodes): its index in each time, -1 for none. */
void light_track_default_looks(const LightTrack *f, int look[LT_TIMES]);
/* The tunnel `i` with new nodes or kind, and the looks over its old nodes at all four times moved with it: all or nothing (`why`: the game's rule that refused). */
int light_track_move_tunnel(LightTrack *f, int i, const LightTrackTunnel *t, char *why, size_t cap);
/* The tunnel `i` (the fact) and its looks at all four times away: all or nothing. A look over only part of it that is left makes the game refuse, and says so. */
int light_track_remove_tunnel(LightTrack *f, int i, char *why, size_t cap);
/* The looks over exactly [from, to] at all four times away (a row that is only looks): all or nothing. */
int light_track_remove_looks(LightTrack *f, unsigned from, unsigned to, char *why, size_t cap);

#ifdef __cplusplus
}
#endif

#endif

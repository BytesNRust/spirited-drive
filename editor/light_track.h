/* A track's light file in the new format as the Track light panel edits it: data/track_light/<TRACK>.txt, one file for the track, the facts first and one block for each time of day
 * (game/race/track_light_track.h, notes/track-light-format-2026-11-22.md). The file is held as its LINES. The panel owns the lines it lists (the facts `tunnel`, `snow`; and in a time's
 * block `sun`, `tunnel_look`, `lamp`, `brightness`, `tone`, `rows`); every other line (comments, blank lines, the `track` and `time` lines, `snow_map`) is kept byte for byte, so a file that
 * was not changed is written back as the same bytes and a change moves no line it did not make. A new line goes at the end of its time's block (the facts: after the last fact; a time with
 * no block gets one at the end of the file).
 *
 * The rules are the game's (the whole file is checked after each change, as the game's parser checks it: a time once, facts before the first `time`, a `tunnel_look` over nodes that lie inside a
 * `tunnel` fact, the limits per time). A change that would make a line the game refuses is not made: the file stays as it was and `why` says what is wrong. A line the file already has that the
 * game would refuse (a word it does not know, a value out of range) is kept as it is, listed (light_track_first_bad), never rewritten. Pure C, no window, no file access. */
#ifndef TNFS_EDITOR_LIGHT_TRACK_H
#define TNFS_EDITOR_LIGHT_TRACK_H

#include <stddef.h>

enum {
    LT_TIME_FACTS = -1,   /* `time` argument of the facts */
    LT_TIME_A = 0, LT_TIME_B, LT_TIME_C, LT_TIME_N, LT_TIMES
};

enum {
    LT_OTHER = 0,   /* blank, comment, `track`, `time`, `snow_map` */
    LT_TUNNEL,      /* fact `tunnel FROM TO [lit]` */
    LT_SNOW,        /* fact `snow 0|1` */
    LT_SUN,         /* `sun AZ EL SHADOW [REACH]` */
    LT_LOOK,        /* `tunnel_look [FROM TO] [LIGHT] [COLOUR] [lamps S [COLOUR]] [reach R]` */
    LT_LAMP,        /* `lamp NODE PERCENT [COLOUR[:S]] [RADIUS] [STRIP]` */
    LT_BRIGHTNESS,  /* `brightness FROM TO PERCENT [STRIP]` */
    LT_TONE,        /* `tone FROM TO [STRIP] RRGGBB:PERCENT` */
    LT_ROWS,        /* `rows FROM TO p0..p10` */
    LT_OWNED,       /* how many kinds the panel edits */
    LT_BAD = LT_OWNED   /* a line the game would refuse: kept as it is, listed, never rewritten */
};

enum {   /* the game's limits (game/race/track_light_file.h) */
    LT_MAX_PER_TIME = 64,      /* tunnel_look, brightness, tone lines of a time; halls of the track; and the halls and the `tunnel_look A B` lines of a time together (the game's zones[]) */
    LT_MAX_LAMPS = 2048,
    LT_MAX_LAMP_PERCENT = 1000,
    LT_MAX_LAMP_RADIUS = 64,
    LT_MAX_BRIGHTNESS_PERCENT = 1000,
    LT_MAX_DISTINCT_TONES = 16,
    LT_SNOW_SLOTS = 144,       /* the texture slots a `snow_map` names */
    LT_POINTS = 11,            /* the points of a node's row: p0 centre, p1..p5 right, p6..p10 left */
    LT_ALL_POINTS = (1 << LT_POINTS) - 1,
    LT_WHY = 160,
    LT_MAX_FILE_BYTES = 4 * 1024 * 1024   /* a bigger file is not opened (a 5000-node track's is about 0.3 MB) */
};
#define LT_MAX_LOOK_LIGHT 10000.0
#define LT_MAX_LOOK_LAMPS 1000.0
#define LT_MAX_SUN_REACH 2000.0

/* a strip across the row as a mask of its points: bit p for p0..p10. The writable ones are `all`, `left` (p6..p10), `right` (p1..p5), `road` (p0 p1 p6) and any run pA..pB. */
enum { LT_STRIP_LEFT = 0x7c0, LT_STRIP_RIGHT = 0x03e, LT_STRIP_ROAD = 0x043 };

typedef struct {
    char *text;   /* the line without its '\n' (a "\r" before it stays in the text) */
    int kind;     /* LT_* */
    int time;     /* LT_TIME_FACTS, or the time of the block the line is in */
    const char *why;   /* LT_BAD: what is wrong (static text) */
} LightTrackLine;

typedef struct {
    LightTrackLine *lines;
    int count, cap;
    int final_newline;   /* the file ends with a '\n' */
    int crlf;            /* the first line ends in "\r": new lines are written so too */
    char track[16];      /* the stem of the `track` line, "CY1" */
    unsigned nodes;      /* the track's node count (the file does not say it) */
} LightTrack;

typedef struct { unsigned from, to; int lit; int has_gradient, gradient; } LightTrackTunnel;   /* gradient: the nodes inside each mouth the light falls over (0 a straight cut, LT_MAX_GRADIENT at most); the game's default (LT_DEFAULT_GRADIENT) when the line has none */
enum { LT_MAX_GRADIENT = 8, LT_DEFAULT_GRADIENT = 3 };
typedef struct { int has_azimuth, has_elevation, has_shadow; double azimuth, elevation, shadow; int has_reach; double reach; } LightTrackSun;   /* degrees, degrees (-90..90), percent 0..100; a part not set is `-`; the reach in units 1..2000 (the terrain that can shade a point), no reach: the generator's */
typedef struct {
    int has_nodes; unsigned from, to;   /* no nodes: the default look of the time */
    int has_light; double light;        /* percent of full light inside, 0..10000 */
    int has_colour; char colour[48];    /* RRGGBB[:strength] or r/g/b[:strength], as written */
    int has_lamps; double lamps;        /* the lamps' strength in percent, 0..1000 */
    int has_lamp_colour; char lamp_colour[48];
    int has_reach; double reach;        /* the pools' reach in nodes, 0.5..4 */
} LightTrackLook;
typedef struct { unsigned node, percent; int has_colour; char colour[16]; int has_radius; unsigned radius; int has_strip; unsigned mask; } LightTrackLamp;   /* has_strip: the strip (the last word) the lamp stands over, mask its points (LT_ALL_POINTS is `all`); none: as before the strip existed */
typedef struct { unsigned from, to, percent, mask; } LightTrackBrightness;   /* mask: the strip's points (LT_ALL_POINTS for the whole row) */
typedef struct { unsigned from, to, mask, percent; char colour[8]; } LightTrackTone;   /* colour: six lower-case hex digits, percent 0..100 */
typedef struct { unsigned from, to; unsigned char value[LT_POINTS]; } LightTrackRows;

/* Reads a file's text for a track of `nodes` nodes. 1, or 0 with `why` (no `track STEM` first, no nodes). A line the game would refuse does not fail the read: it is listed. */
int light_track_parse(const char *text, size_t size, unsigned nodes, LightTrack *out, char *why, size_t cap);
void light_track_free(LightTrack *f);

/* The file as bytes (malloc'd, `*size` long, also NUL-terminated); NULL when out of memory. A file that was not edited gives the bytes it was parsed from. */
char *light_track_format(const LightTrack *f, size_t *size);

/* How many lines of `kind` (LT_*) a time's block has (`time` LT_TIME_FACTS for the facts), and the 1-based number of the first line the game would refuse (0 for none). */
int light_track_count(const LightTrack *f, int time, int kind);
int light_track_first_bad(const LightTrack *f);
/* which times have a block (`time A` ... `time N`) */
int light_track_has_time(const LightTrack *f, int time);

/* The i-th (from 0) line of a kind as values: 1, or 0 when there is no such line. The facts take no time. */
int light_track_get_tunnel(const LightTrack *f, int i, LightTrackTunnel *out);
int light_track_get_snow(const LightTrack *f, int *on);
int light_track_get_sun(const LightTrack *f, int time, LightTrackSun *out);
int light_track_get_look(const LightTrack *f, int time, int i, LightTrackLook *out);
int light_track_get_lamp(const LightTrack *f, int time, int i, LightTrackLamp *out);
int light_track_get_brightness(const LightTrack *f, int time, int i, LightTrackBrightness *out);
int light_track_get_tone(const LightTrack *f, int time, int i, LightTrackTone *out);
int light_track_get_rows(const LightTrack *f, int time, int i, LightTrackRows *out);

/* Set the i-th line of a kind, or add a new one with i = -1 (a `sun` or a `snow` has one line: i is 0 or -1 for both). 1; or 0 with `why` when the value breaks a rule of the game or the
 * file would hold a line the game refuses (a `tunnel_look` outside the tunnels, a tunnel fact that a look still needs, a 65th line of a kind, a 17th tone, a strip that is not a run of
 * points); the file is then as it was. */
int light_track_set_tunnel(LightTrack *f, int i, const LightTrackTunnel *v, char *why, size_t cap);
int light_track_set_snow(LightTrack *f, int on, char *why, size_t cap);
int light_track_set_sun(LightTrack *f, int time, const LightTrackSun *v, char *why, size_t cap);
int light_track_set_look(LightTrack *f, int time, int i, const LightTrackLook *v, char *why, size_t cap);
int light_track_set_lamp(LightTrack *f, int time, int i, const LightTrackLamp *v, char *why, size_t cap);
int light_track_set_brightness(LightTrack *f, int time, int i, const LightTrackBrightness *v, char *why, size_t cap);
int light_track_set_tone(LightTrack *f, int time, int i, const LightTrackTone *v, char *why, size_t cap);
int light_track_set_rows(LightTrack *f, int time, int i, const LightTrackRows *v, char *why, size_t cap);

/* The sun a part left "auto" uses (the generator's own, game/race/terrain_light.c terrain_light_sun_shadowed: dawn 90 degrees, day 180, dusk 270, night 0; height 15, 50, 12, 12 degrees; shadow strength
 * 55 percent, 100 at night), so the panel can say it and a ticked part starts from it. Every has_* is 1; 0 for a time that does not exist. A test keeps it equal to the game's. */
int light_track_sun_auto(int time, LightTrackSun *out);

/* Drops the i-th line of an owned kind (nothing else moves). 1, or 0 when there is no such line or dropping it would leave a look outside the tunnels (`why`). */
int light_track_remove(LightTrack *f, int time, int kind, int i, char *why, size_t cap);

/* Puts the lines of a kind (LT_TUNNEL, LT_LAMP, LT_BRIGHTNESS, LT_TONE) of a time in the order of their first node (stable). The file's order is the order the game applies them in, so a brightness
 * never passes one that shares a node and a point with it, nor a tone one that shares a node; the lines that cannot change places stay where they are: what the game makes of the file does not change. A line that
 * is added with an index of -1 is sorted in the same way. Returns the places changed (0 when sorted), -1 on a kind that has no order or no memory. */
int light_track_sort(LightTrack *f, int time, int kind);

#endif

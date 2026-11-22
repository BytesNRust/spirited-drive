/* A track's light file as the Track light window edits it (dep98 step 2, part A): data/track_light/<STEM><A|B|C|N>.txt held as its LINES. The window owns only the
 * retouch lines it lists (`brightness`, `lamp`, `tunnel_tone`); every other line (the header, the `rows`, comments, the Spanish words of older files, words this editor
 * does not know, the places lines) is kept byte for byte, so a file that was not changed is written back as the same bytes and a change moves no line it did not make.
 * A new line goes after the file's last line. The values are checked by the rules of the game's parser (game/race/track_light_file.c; the Studio shares only the core with
 * the game, so the limits are repeated here and the test pins them). Pure C, no window, no file access. */
#ifndef TNFS_EDITOR_LIGHT_FILE_H
#define TNFS_EDITOR_LIGHT_FILE_H

#include <stddef.h>

enum {
    LIGHT_KIND_BRIGHTNESS = 0,   /* `brightness FROM TO PERCENT` */
    LIGHT_KIND_LAMP,             /* `lamp NODE PERCENT [RRGGBB[:strength]] [radius]` (a night file's) */
    LIGHT_KIND_TUNNEL_TONE,      /* `tunnel_tone LIGHT [colour]` */
    LIGHT_KIND_TONE,             /* `tone FROM TO [p0..p10 ...] RRGGBB:percent` (part D) */
    LIGHT_KIND_CAR_TONE,         /* `car_tone [FROM TO] RRGGBB:percent` */
    LIGHT_KIND_ZONE,             /* `tunnel FROM TO [light] [colour | lit]` of a time's file */
    LIGHT_KIND_PLACE,            /* `tunnel|canyon|car_dim FROM TO` of a track's places file (letter 0) */
    LIGHT_KIND_TUNNEL_LAMP,      /* `tunnel_lamp [FROM TO] STRENGTH [colour] [reach]` of a time's file */
    LIGHT_KIND_OWNED,            /* how many kinds the window edits */
    LIGHT_KIND_BAD = LIGHT_KIND_OWNED,   /* a line that names one of them and is not well-formed: kept as it is, listed, never rewritten */
    LIGHT_KIND_OTHER                     /* every other line */
};

enum {   /* the game's limits (game/race/track_light_file.h) */
    LIGHT_MAX_BRIGHTNESS = 64,
    LIGHT_MAX_LAMPS = 2048,
    LIGHT_MAX_LAMP_PERCENT = 1000,
    LIGHT_MAX_LAMP_RADIUS = 64,
    LIGHT_MAX_BRIGHTNESS_PERCENT = 1000,
    LIGHT_MAX_TONES = 64,          /* `tone` lines; also car_tone and zone lines, 64 each */
    LIGHT_MAX_TUNNEL_LAMPS = 64,   /* `tunnel_lamp` lines */
    LIGHT_MAX_TUNNEL_LAMP_STRENGTH = 1000,
    LIGHT_MAX_DISTINCT_TONES = 16, /* different (colour, percent) among the `tone` lines */
    LIGHT_POINTS = 11,             /* the points of a node's row: p0..p10 */
    LIGHT_WHY = 160
};
#define LIGHT_MAX_TUNNEL_LIGHT 10000.0

typedef struct {
    char *text;   /* the line without its '\n' (a "\r" before it stays in the text) */
    int kind;     /* LIGHT_KIND_* */
} LightLine;

typedef struct {
    LightLine *lines;
    int count, cap;
    int final_newline;   /* the file ends with a '\n' */
    int crlf;            /* the first line ends in "\r": new lines are written so too */
    char track[8];       /* "CY1" */
    char letter;         /* 'A' dawn, 'B' day, 'C' dusk, 'N' night; 0 for a track's places file (data/track/<STEM>.txt) */
    unsigned nodes;      /* the header's node count; 0 for a places file */
} LightFile;

/* `brightness FROM TO PERCENT [left|right] [shade]`: the side of the road's rows it acts on (the middle point is on both) and Only shadows (`shade`) */
enum { LIGHT_SIDE_BOTH, LIGHT_SIDE_LEFT, LIGHT_SIDE_RIGHT };
typedef struct { unsigned from, to, percent; int side, shade; } LightBrightness;
typedef struct {
    unsigned node, percent;
    int has_colour;
    char colour[16];   /* "RRGGBB" or "RRGGBB:strength" */
    int has_radius;
    unsigned radius;   /* in nodes */
} LightLamp;
typedef struct {
    double light;      /* percent of full light */
    int has_colour;
    char colour[48];   /* "RRGGBB", "RRGGBB:strength" or "r/g/b[:strength]" as it is written */
} LightTunnelTone;

/* part D. A tone is seen over nodes FROM..TO (`mask`: bit p for the row points it covers, 0 for all of them) as a colour at a percent 0..100. A car tone is the same for the car, over
 * the whole track when `has_nodes` is 0. A zone is a tunnel over FROM..TO with this time's light (percent, `has_light`) and colour, or a hall (`lit`, then neither). A place is a
 * range of a track's tunnel, canyon or car_dim nodes. A tone's colour is six lower-case hex digits; a zone's is RRGGBB, RRGGBB:strength or r/g/b[:strength]. */
typedef struct { unsigned from, to, mask; char colour[8]; unsigned percent; } LightTone;
typedef struct { int has_nodes; unsigned from, to; char colour[8]; unsigned percent; } LightCarTone;
typedef struct { unsigned from, to; int lit, has_light; double light; int has_colour; char colour[48]; } LightZone;
typedef struct { char what[8]; unsigned from, to; } LightPlace;
/* A tunnel_lamp line: the tunnels' lamps at this time, over every tunnel (`has_nodes` 0) or over FROM..TO: strength in percent (0..1000, as written at any time of day), a colour (as a zone's, kept as
 * written) and a reach in nodes (0.5..4); what is not written comes from the general line, and then from the game's own values. */
typedef struct { int has_nodes; unsigned from, to; double strength; int has_colour; char colour[48]; int has_reach; double reach; } LightTunnelLamp;

/* Reads a file's text. 1, or 0 with `why` (header missing or not a light file, a time that is not A, B, C or N, no node count). Nothing is refused for its other lines. */
int light_file_parse(const char *text, size_t size, LightFile *out, char *why, size_t cap);
void light_file_free(LightFile *f);

/* The file as bytes (malloc'd, `*size` long, also NUL-terminated); NULL when out of memory. A file that was not edited gives the bytes it was parsed from. */
char *light_file_format(const LightFile *f, size_t *size);

/* How many lines of `kind` there are (LIGHT_KIND_*), and the 1-based number of the first malformed owned line (0 for none). */
int light_file_count(const LightFile *f, int kind);
int light_file_first_bad(const LightFile *f);

/* The i-th (from 0) line of a kind as values: 1, or 0 when there is no such line. */
int light_file_get_brightness(const LightFile *f, int i, LightBrightness *out);
int light_file_get_lamp(const LightFile *f, int i, LightLamp *out);
int light_file_get_tunnel_tone(const LightFile *f, int i, LightTunnelTone *out);
int light_file_get_tone(const LightFile *f, int i, LightTone *out);
int light_file_get_car_tone(const LightFile *f, int i, LightCarTone *out);
int light_file_get_zone(const LightFile *f, int i, LightZone *out);
int light_file_get_place(const LightFile *f, int i, LightPlace *out);
int light_file_get_tunnel_lamp(const LightFile *f, int i, LightTunnelLamp *out);

/* Set the i-th line of a kind, or add a new one at the end of the file with i = -1. 1; or 0 with `why` when the value breaks a rule of the game (an unordered or out-of-range
 * run of nodes, a percent over its limit, a 65th brightness or a 2049th lamp, a lamp in a file that is not a night's, a bad colour, a second tunnel_tone) or memory is out; the
 * file is then as it was. */
int light_file_set_brightness(LightFile *f, int i, const LightBrightness *v, char *why, size_t cap);
int light_file_set_lamp(LightFile *f, int i, const LightLamp *v, char *why, size_t cap);
int light_file_set_tunnel_tone(LightFile *f, int i, const LightTunnelTone *v, char *why, size_t cap);

/* part D. Tone, car tone and zone lines belong to a time's file, places to a places file; the same rules (the run inside the road, percents, 16 distinct tones, 64 lines of a kind). A place
 * is checked against `nodes`, the node count of the track's time files (a places file has none of its own). */
int light_file_set_tone(LightFile *f, int i, const LightTone *v, char *why, size_t cap);
int light_file_set_car_tone(LightFile *f, int i, const LightCarTone *v, char *why, size_t cap);
int light_file_set_zone(LightFile *f, int i, const LightZone *v, char *why, size_t cap);
int light_file_set_place(LightFile *f, int i, const LightPlace *v, unsigned nodes, char *why, size_t cap);
int light_file_set_tunnel_lamp(LightFile *f, int i, const LightTunnelLamp *v, char *why, size_t cap);   /* a time's file: the same rules as the game's parser (64 lines, strength 0..1000, reach 0.5..4) */

/* Drops the i-th line of an owned kind (nothing else moves). 1, or 0 when there is no such line. */
int light_file_remove(LightFile *f, int kind, int i);

#endif

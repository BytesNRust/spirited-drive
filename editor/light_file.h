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

typedef struct { unsigned from, to, percent; } LightBrightness;
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

/* Set the i-th line of a kind, or add a new one at the end of the file with i = -1. 1; or 0 with `why` when the value breaks a rule of the game (an unordered or out-of-range
 * run of nodes, a percent over its limit, a 65th brightness or a 2049th lamp, a lamp in a file that is not a night's, a bad colour, a second tunnel_tone) or memory is out; the
 * file is then as it was. */
int light_file_set_brightness(LightFile *f, int i, const LightBrightness *v, char *why, size_t cap);
int light_file_set_lamp(LightFile *f, int i, const LightLamp *v, char *why, size_t cap);
int light_file_set_tunnel_tone(LightFile *f, int i, const LightTunnelTone *v, char *why, size_t cap);

/* Drops the i-th line of an owned kind (nothing else moves). 1, or 0 when there is no such line. */
int light_file_remove(LightFile *f, int kind, int i);

#endif

/* port (track_light_file): what a track light file's lines of retouches are once read (TrackLightTweaks: brightness, tone, lamps, the tunnels' look, `rows` overrides) and how they go over a made light
 * (track_light_apply). The file itself, one for a track (data/track_light/<TRACK>.txt), is read by track_light_track.h; its format is in docs/track-light.md. Stock and Restored do not read it. */
#ifndef TNFS_TRACK_LIGHT_FILE_H
#define TNFS_TRACK_LIGHT_FILE_H

#include "game/race/track_node_light.h"

#include <stddef.h>
#include <stdint.h>

enum { TRACK_LIGHT_MAX_TUNNEL_LAMPS = 64 };
/* a `tunnel_lamp [FROM TO] <strength> [colour] [reach]` line: the tunnels' lamps at this time (`general`: every tunnel; else nodes from..to), their strength in percent (0..1000, as written at any time of
 * day), their colour as `tunnel_tone`'s (RRGGBB[:strength] or r/g/b: the factor on each channel, 0..1), and their reach across the road in nodes (0.5..4). What a line leaves out comes from the general line, and
 * what that leaves out from the values the code has (track_light_tunnel_lamp_at). */
typedef struct { uint32_t from, to; int general; double strength; int has_colour, has_reach; float cast[3]; double reach; } TrackLightTunnelLamp;
enum { TRACK_LIGHT_TUNNEL_LAMP_STRENGTH = 1, TRACK_LIGHT_TUNNEL_LAMP_COLOUR = 2, TRACK_LIGHT_TUNNEL_LAMP_REACH = 4 };
enum { TRACK_LIGHT_MAX_BRIGHT = 64, TRACK_LIGHT_MAX_LAMPS = 2048, TRACK_LIGHT_MAX_LAMP_RADIUS = 64, TRACK_LIGHT_MAX_LAMP_PERCENT = 1000 };
/* a `brightness FROM TO PERCENT [left|right] [shade]` line: which side of the road's rows it acts on (the middle point counts for both: the right is points 1..5, the left 6..10) and, with `shade` (Only shadows),
 * whether it acts only on the darkness the sun left (see track_light_apply). A line with neither word is the line of before. */
enum { TRACK_LIGHT_SIDE_BOTH, TRACK_LIGHT_SIDE_LEFT, TRACK_LIGHT_SIDE_RIGHT };
/* `mask`: the points of the row the line is on (bit p, p0..p10), set by the track light file's STRIP (track_light_track.h); 0 for the lines of the old format, which `side` and `shade` rule */
typedef struct { uint32_t from, to; int percent; int side; int shade; uint16_t mask; } TrackLightBrightness;
/* a `tone` line: nodes from..to, the points of their rows it is on (bit p; 0x7ff all), and the tone's number in the file's list of distinct tones */
typedef struct { uint32_t from, to; uint16_t mask; uint8_t tone; } TrackLightToneRun;
/* a `tunnel a b [light] [colour | lit]` line: lit, a hall (a tunnel with a lot of light, lit by its own lamps); else the zone's own look when the line gives a light and / or a colour (what it leaves out is the
 * time's tunnel_tone's), which is a tone slot (`tone`: its number, or -1 when the look is the time's) once the file is read. `line` is the line's number, for the slot that does not fit. */
typedef struct { uint32_t from, to; int lit; int has_light, has_colour; double light; float cast[3]; int line; int tone; int blend; int own; int share; } TrackLightZone;   /* blend 1..99: a step of a tunnel mouth's gradient (made at the end of a time's lines, not read), `light` and `cast` the tunnel's; 0 a zone; share > 0: a step on the open road beside the mouth (look 3), the percent of the way the road's light goes toward the tunnel road surface's */
/* `tunnel_gradient A B N` (the track file's `tunnel A B gradient N`, a line of every block): the tunnel A..B's mouths fall off over N nodes, 1..TRACK_LIGHT_MAX_GRADIENT */
typedef struct { uint32_t from, to; int nodes; } TrackLightGradient;
enum { TRACK_LIGHT_MAX_GRADIENT = 8, TRACK_LIGHT_DEFAULT_GRADIENT = 3, TRACK_LIGHT_MAX_STEPS = 512 };
/* a `lamp` line: the lamp's node, its light in percent (1..TRACK_LIGHT_MAX_LAMP_PERCENT), its radius in nodes, and its colour (the warm street lamp's when `coloured` is 0; else rgb mixed toward the warm by 100 - strength percent), and its STRIP (`mask`: bit k = row point k; 0 none: the lamp stands at a scenery post, else the node's centre) */
typedef struct { uint32_t node; uint16_t percent; uint8_t radius; uint8_t coloured, strength; uint8_t rgb[3]; uint16_t mask; } TrackLightLamp;
typedef struct {
    TrackLightBrightness brightness[TRACK_LIGHT_MAX_BRIGHT];
    int brightness_count;
    TrackNodeLightTone tones[TRACK_NODE_LIGHT_TONES];   /* the distinct tones the `tone` lines use, and then the zones' own looks (at most TRACK_NODE_LIGHT_TONES in all: a line past that is refused) */
    int tone_count;
    TrackLightToneRun tone_runs[TRACK_LIGHT_MAX_BRIGHT];
    int tone_run_count;
    int look_has_light, look_has_colour;   /* the tunnels' look this time: `tunnel_tone`'s (one default a time: a second that differs is a bad line; a zone line may give its own) */
    double look_light;                     /* percent of full light (100 is row 255 at any time of day) */
    float look_cast[3];                    /* the colour as the factor on each channel, 0..1 (the file's RRGGBB[:strength]) */
    TrackLightZone zones[TRACK_LIGHT_MAX_BRIGHT];   /* the `tunnel a b ...` lines: they add to the places' tunnels, and `lit` makes the nodes a hall */
    int zone_count;
    TrackLightGradient gradients[TRACK_LIGHT_MAX_BRIGHT];   /* tunnel_gradient: the tunnels whose mouths take a gradient; zones for it are added at the end (track_light_tweaks_finish) */
    int gradient_count;
    TrackLightZone steps[TRACK_LIGHT_MAX_STEPS];   /* the gradients' steps, one a node (blend > 0), made by track_light_tweaks_finish and laid over the zones */
    int step_count;
    TrackLightLamp lamps[TRACK_LIGHT_MAX_LAMPS];   /* lamp: in file order */
    int lamp_count;
    TrackLightTunnelLamp tunnel_lamps[TRACK_LIGHT_MAX_TUNNEL_LAMPS];   /* tunnel_lamp: in file order */
    int tunnel_lamp_count;
    int sun_set;                           /* the `sun AZIMUTH ELEVATION SHADOW` line (one a time; a word may be `-`, the generator's own): bit 0 the azimuth, 1 the elevation, 2 the shadow are given */
    float sun_azimuth, sun_elevation;      /* degrees: the azimuth in the world's axes (0 toward +x of the track's units, 90 toward +z), the elevation over the horizon */
    float sun_reach;                       /* units, the fourth word (bit 3): how far the terrain shades a point from the sun; the generator's own when not given */
    float sun_shadow;                      /* percent of light left in full shadow (100: no shadow) */
} TrackLightTweaks;
enum { TRACK_LIGHT_SUN_AZIMUTH = 1, TRACK_LIGHT_SUN_ELEVATION = 2, TRACK_LIGHT_SUN_SHADOW = 4, TRACK_LIGHT_SUN_REACH = 8 };

/* What the file says about the lamps of the tunnel at `node`: the bits of TRACK_LIGHT_TUNNEL_LAMP_* it gives, with *strength (percent), cast[3] and *reach (nodes) set for those bits only. The last
 * general line is the base, the last line over `node` replaces it for what it gives; 0 (nothing set) with no line: the caller keeps the values the code has. */
int track_light_tunnel_lamp_at(const TrackLightTweaks *tweaks, uint32_t node, double *strength, float cast[3], double *reach);

/* a time block's `sun AZ EL SHADOW`: degrees, degrees, percent of light left in full shadow; a part written `-` is not `has_*` (the generator's default) */
typedef struct { int has, has_azimuth, has_elevation, has_shadow; double azimuth, elevation, shadow; int has_reach; double reach; } TrackLightSun;   /* reach: the units the terrain can shade from (a fourth word, 1..2000; the generator's 400 when not given) */
typedef struct {
    int base;                   /* 1: the file's flags are the ones asked and `light` holds its rows (owned); 0: only the tweaks count */
    uint32_t flags;             /* the flags the file was made with */
    TrackNodeLight light;
    TrackLightTweaks tweaks;
    char place_path[512];       /* the places file the light's tunnels, canyon and car_dim came from ("" none) */
    uint8_t *row_set;           /* the track file's `rows` overrides (track_light_track.h): per node (owned), 1 where `light.data` holds the file's own row; NULL none */
    TrackLightSun sun;          /* the time's `sun` line (track_light_track.h), `has` 0 when there is none */
} TrackLightFile;

/* `flags` for track_light_parse that takes the file's rows whatever settings its header says it was made with (the header's flags are a record: TrackLightFile.flags) */
#define TRACK_LIGHT_ANY_FLAGS 0xffffffffu

void track_light_file_free(TrackLightFile *file);
/* for track_light_track.c: one line of a time's retouches (brightness, tone, lamp, tunnel_tone, tunnel, tunnel_lamp, of the old format's grammar) into `tweaks`, and the end of the lines (the zones' tone slots;
 * 0 and the zone's line in *bad_line when a slot does not fit) */
int track_light_tweak_line(const char *line, uint32_t nodes, TrackLightTweaks *tweaks);
int track_light_tweaks_finish(TrackLightTweaks *tweaks, int *bad_line);

/* The tweaks over a made light: the brightness runs scale its rows, the tunnel looks are its warm_cast / warm_percent. With `lamps` (the port's track_light_lamp) the `lamp` lines are laid too, as
 * the light's lamp list (sorted by node; their world place is set by track_node_light_lamps_place): the old pool classes the code made are dropped, a lamp is light. `ring` wraps a lamp's reach past the ends. */
void track_light_apply(const TrackLightTweaks *tweaks, TrackNodeLight *light, int lamps, int ring);

#endif

/* port (track_light_file): the light the port makes for a track, as a text file the game reads in place of making it, one per track and time of day
 * (data/track_light/<TRACK><A|B|C|N>.txt, A dawn, B day, C dusk, N night), by runs of nodes so it can be edited by hand. The file holds what make_light (drive_light.c)
 * makes from the terrain, the road's tunnels and the street lamps: the rows of 11 points, the tunnel, hall, canyon and dim-car nodes, the lamps' pools, the car's
 * lamp alternation and the tunnel casts. A line a run, '#' starts a comment:
 *
 *   # tnfs track light 1
 *   pista CY1 hora N nodos 1024 banderas 3fff   (the header: the file is read only for this track, time, node count and the port flags it was made with)
 *   filas 0 63 255 255 255 255 255 255 255 255 255 255 255    (nodes 0..63 have this row of 11 points)
 *   tunel 76 107          (road tunnel nodes; sala: a lit hall's; coche_tenue: the car keeps its dim tone; canon: TR6's canyon)
 *   pozo 130 136 3        (the street lamps' pool class 1..4 of the nodes, for the road's warm tint; pozo_coche: the same under a car)
 *   coche_disco 1 <even r g b> <odd r g b>   (the CD's own car ratios in a tunnel's lamp alternation, as C hex floats)
 *   reparto neutro 0 ciudad 1                (the tunnel's cast: neutral_cast, city_tunnel)
 *
 * and the lines added by hand (all lines count the same):
 *   tono_tunel noche 1 0.35 0.05 174   (a warm tunnel's cast, red green blue, and its light in percent; dia is the day's. The file starts with the values the code has)
 *   brillo 120 180 70                  (nodes 120..180 at 70 percent of the light they have)
 *   tono_coche ff8020:60               (the car's tone on the whole track: a colour in hex and its intensity 0..100, as the palette allows; tono_coche 120 180 ff8020:60 for nodes 120..180 only)
 *
 * The header's flags are a record of which of the port's settings the rows were generated with. In Restomod '26 the file rules whatever they say (a difference is one log line, track_light_diff names it);
 * Stock and Restored do not read the files. See docs/track-light.md.
 *
 * track_light_find looks for the file first in a mod's folder (mods/<mod>/track_light/<file>, track_light_set_mod_dir), then in the game's data. */
#ifndef TNFS_TRACK_LIGHT_FILE_H
#define TNFS_TRACK_LIGHT_FILE_H

#include "game/race/psx_light.h"

#include <stddef.h>
#include <stdint.h>

/* The file's text for `light` (a made light): malloc'ed in *text, its length in *size. 1 on success. `flags` is the port flags' fingerprint the light was made with. */
int track_light_format(const PsxLight *light, const char *track, char letter, uint32_t flags, char **text, size_t *size);

/* the port's settings the rows are made with, one bit each, in this order (drive_light.c track_light_flags) */
enum {
    TLF_TRANS_SHADOWS, TLF_MADE_TUNNELS, TLF_CAR_TUNNEL_LIGHT, TLF_HALL_LIGHT, TLF_PARKING_DARK, TLF_CAVERN_DARK, TLF_CANYON_SHADE, TLF_LAMP_POOL_TINT, TLF_LAMP_CAR_SHADE,
    TLF_LAMP_POOL_BRIGHT, TLF_CITY_LIFT3, TLF_STREET_LAMPS, TLF_REVERSE, TLF_COUNT
};
/* The names of the bits of `file` and `now` that differ, comma separated in `out` ("" when they are the same) */
void track_light_diff(uint32_t file, uint32_t now, char *out, size_t out_size);

enum { TRACK_LIGHT_MAX_BRIGHT = 64 };
typedef struct { uint32_t from, to; int percent; } TrackLightBrightness;
typedef struct { uint32_t from, to; uint8_t rgb[3]; int percent; } TrackLightCarTone;
typedef struct {
    TrackLightBrightness brightness[TRACK_LIGHT_MAX_BRIGHT];
    int brightness_count;
    TrackLightCarTone car_tone[TRACK_LIGHT_MAX_BRIGHT];   /* tono_coche: in file order, later lines over earlier ones */
    int car_tone_count;
    int warm_set[2];            /* [0] the day's, [1] the night's: tono_tunel given */
    float warm_cast[2][3];
    int warm_percent[2];
} TrackLightTweaks;

typedef struct {
    int base;                   /* 1: the file's flags are the ones asked and `light` holds its rows (owned); 0: only the tweaks count */
    uint32_t flags;             /* the flags the file was made with */
    PsxLight light;
    TrackLightTweaks tweaks;
} TrackLightFile;

/* `flags` for track_light_parse that takes the file's rows whatever settings its header says it was made with (the header's flags are a record: TrackLightFile.flags) */
#define TRACK_LIGHT_ANY_FLAGS 0xffffffffu

/* The file in `text`: its header must name `track`, `letter` and `expected_nodes` (else 0: it is another file), and every line must be well-formed (else 0, with the line number in
 * *bad_line when it is not NULL). `out` is untouched unless 1; with other flags than `flags` (unless TRACK_LIGHT_ANY_FLAGS) its base is 0 and only the tweaks are read. */
int track_light_parse(const char *text, size_t size, const char *track, char letter, uint32_t expected_nodes, uint32_t flags, TrackLightFile *out, int *bad_line);
void track_light_file_free(TrackLightFile *file);

/* The tweaks over a made light: the brightness runs scale its rows, the tunnel looks are its warm_cast / warm_percent. */
void track_light_apply(const TrackLightTweaks *tweaks, PsxLight *light);

/* The folder a mod keeps its own copy of the files in (<mod dir>/track_light/), NULL or "" for none. */
void track_light_set_mod_dir(const char *dir);

/* Where the file `<TRACK><letter>.txt` is: `out` takes the path of the first that exists (the mod's, then the data folders: data_path.h), 1 when there is one. */
int track_light_find(const char *track, char letter, char *out, size_t out_size);

/* The file of `track` at `letter`, parsed (see track_light_parse), 0 when there is none or it is not for this light. Its rows are always the base: `flags` (the settings now) is only compared with the
 * header's record, and a difference is one line in the log and in `why` (NULL for none; "" when they are the same). `path` (NULL for none) takes the file's path. */
int track_light_load(const char *track, char letter, uint32_t expected_nodes, uint32_t flags, TrackLightFile *out, char *path, size_t path_size, char *why, size_t why_size);

#endif

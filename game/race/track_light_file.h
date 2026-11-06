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
 * track_light_find looks for the file first in a mod's folder (mods/<mod>/track_light/<file>, track_light_set_mod_dir), then in the game's data. */
#ifndef TNFS_TRACK_LIGHT_FILE_H
#define TNFS_TRACK_LIGHT_FILE_H

#include "game/race/psx_light.h"

#include <stddef.h>
#include <stdint.h>

/* The file's text for `light` (a made light): malloc'ed in *text, its length in *size. 1 on success. `flags` is the port flags' fingerprint the light was made with. */
int track_light_format(const PsxLight *light, const char *track, char letter, uint32_t flags, char **text, size_t *size);

/* The light of `text`: its header must name `track`, `letter`, `expected_nodes` and `flags` (else 0: the caller makes the light itself), and every line must be well-formed (else 0,
 * with the line number in *bad_line when it is not NULL). `out` is untouched unless 1; its arrays are owned (psx_light_free). */
int track_light_parse(const char *text, size_t size, const char *track, char letter, uint32_t expected_nodes, uint32_t flags, PsxLight *out, int *bad_line);

/* The folder a mod keeps its own copy of the files in (<mod dir>/track_light/), NULL or "" for none. */
void track_light_set_mod_dir(const char *dir);

/* Where the file `<TRACK><letter>.txt` is: `out` takes the path of the first that exists (the mod's, then the data folders: data_path.h), 1 when there is one. */
int track_light_find(const char *track, char letter, char *out, size_t out_size);

/* The file of `track` at `letter`, parsed (see track_light_parse), 0 when there is none or it is not for this light. */
int track_light_load(const char *track, char letter, uint32_t expected_nodes, uint32_t flags, PsxLight *out);

#endif

/* port: see track_light_file.h. The lookup and the read, apart from the format so that its tests need no platform. */
#include "game/race/track_light_file.h"
#include "game/race/track_light_track.h"

#include "game/platform/platform.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the first file of data folder-relative name `relative` that exists, a mod's own copy first (data_path.h) */
static int find_relative(const char *relative, char *out, size_t out_size)
{
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(relative, found);
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) {   /* a mod's own copy first (data_path.h) */
        if (found[i][0] == '\0' || !data_path_exists(found[i])) continue;   /* a folder without it is passed over, no "cannot read" line (i191) */
        size_t size = 0;
        uint8_t *file = platform_read_file(found[i], &size);
        if (file == NULL) continue;
        platform_free_file(file);
        snprintf(out, out_size, "%s", found[i]);
        return 1;
    }
    return 0;
}

int track_light_track_find(const char *track, char *out, size_t out_size)
{
    if (track == NULL || out == NULL || out_size == 0) return 0;
    char relative[128];
    snprintf(relative, sizeof relative, "track_light/%s.txt", track);
    return find_relative(relative, out, out_size);
}

int track_light_track_load(const char *track, char letter, uint32_t nodes, TrackLightFile *out, char *path, size_t path_size)
{
    char found[DATA_PATH_MAX];
    if (!track_light_track_find(track, found, sizeof found)) return 0;
    size_t size = 0;
    uint8_t *file = platform_read_file(found, &size);
    if (file == NULL) return 0;
    int bad = 0;
    const int ok = track_light_track_parse((const char *)file, size, track, letter, nodes, out, &bad);
    platform_free_file(file);
    if (path != NULL && path_size > 0) snprintf(path, path_size, "%s", found);
    if (!ok) {
        if (bad != 0) port_log("track light: %s is not well-formed (line %d), the light is made as before\n", found, bad);
        return 0;
    }
    port_log("track light: %s\n", found);
    out->place_path[0] = '\0';
    return 1;
}

int track_light_track_places(const char *track, uint32_t nodes, TrackNodeLight *out)
{
    TrackLightFile file = { 0 };
    char path[DATA_PATH_MAX];
    if (out == NULL || !track_light_track_load(track, 'B', nodes, &file, path, sizeof path)) return 0;   /* the facts are the same at every time: any block's read has them */
    free(out->tunnel);
    out->tunnel = file.light.tunnel, file.light.tunnel = NULL;
    out->snow = file.light.snow;
    memcpy(out->snow_map, file.light.snow_map, sizeof out->snow_map);
    out->from_places = 1;
    track_light_file_free(&file);
    return 1;
}

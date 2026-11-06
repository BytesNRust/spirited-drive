/* port: see track_light_file.h. The lookup and the read, apart from the format so that its tests need no platform. */
#include "game/race/track_light_file.h"

#include "game/platform/platform.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <string.h>

int track_light_find(const char *track, char letter, char *out, size_t out_size)
{
    if (track == NULL || out == NULL || out_size == 0) return 0;
    char relative[128];
    snprintf(relative, sizeof relative, "track_light/%s%c.txt", track, letter);
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(relative, found);
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) {   /* a mod's own copy first (data_path.h) */
        if (found[i][0] == '\0') continue;
        size_t size = 0;
        uint8_t *file = platform_read_file(found[i], &size);
        if (file == NULL) continue;
        platform_free_file(file);
        snprintf(out, out_size, "%s", found[i]);
        return 1;
    }
    return 0;
}

int track_light_load(const char *track, char letter, uint32_t expected_nodes, uint32_t flags, TrackLightFile *out, char *path, size_t path_size, char *why, size_t why_size)
{
    char found[DATA_PATH_MAX];
    if (why != NULL && why_size > 0) why[0] = '\0';
    if (!track_light_find(track, letter, found, sizeof found)) return 0;
    size_t size = 0;
    uint8_t *file = platform_read_file(found, &size);
    if (file == NULL) return 0;
    int bad = 0;
    const int ok = track_light_parse((const char *)file, size, track, letter, expected_nodes, TRACK_LIGHT_ANY_FLAGS, out, &bad);   /* the file rules: its header's flags are a record only */
    platform_free_file(file);
    if (path != NULL && path_size > 0) snprintf(path, path_size, "%s", found);
    if (!ok) {
        if (bad != 0) port_log("track light: %s is not well-formed (line %d), the light is made as before\n", found, bad);
        return 0;
    }
    port_log("track light: %s\n", found);
    if (out->flags != flags) {   /* one line of warning: the file is used all the same */
        char names[160];
        track_light_diff(out->flags, flags, names, sizeof names);
        port_log("track light: %s was made with other settings (%s)\n", found, names);
        if (why != NULL && why_size > 0) snprintf(why, why_size, "made with other settings: %s", names);
    }
    return 1;
}

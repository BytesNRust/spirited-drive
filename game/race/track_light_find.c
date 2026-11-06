/* port: see track_light_file.h. The lookup and the read, apart from the format so that its tests need no platform. */
#include "game/race/track_light_file.h"

#include "game/platform/platform.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <string.h>

static char mod_dir[DATA_PATH_MAX];

void track_light_set_mod_dir(const char *dir)
{
    snprintf(mod_dir, sizeof mod_dir, "%s", dir != NULL ? dir : "");
}

int track_light_find(const char *track, char letter, char *out, size_t out_size)
{
    if (track == NULL || out == NULL || out_size == 0) return 0;
    char relative[128];
    snprintf(relative, sizeof relative, "track_light/%s%c.txt", track, letter);
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(relative, found);
    char candidates[1 + DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    snprintf(candidates[0], sizeof candidates[0], "%s%s%s", mod_dir, mod_dir[0] != '\0' && mod_dir[strlen(mod_dir) - 1] != '/' ? "/" : "", mod_dir[0] != '\0' ? relative : "");   /* a mod's own copy first */
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) snprintf(candidates[1 + i], sizeof candidates[1 + i], "%s", found[i]);
    for (int i = 0; i < 1 + DATA_PATH_CANDIDATES; i++) {
        if (candidates[i][0] == '\0') continue;
        size_t size = 0;
        uint8_t *file = platform_read_file(candidates[i], &size);
        if (file == NULL) continue;
        platform_free_file(file);
        snprintf(out, out_size, "%s", candidates[i]);
        return 1;
    }
    return 0;
}

int track_light_load(const char *track, char letter, uint32_t expected_nodes, uint32_t flags, PsxLight *out)
{
    char path[DATA_PATH_MAX];
    if (!track_light_find(track, letter, path, sizeof path)) return 0;
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) return 0;
    int bad = 0;
    const int ok = track_light_parse((const char *)file, size, track, letter, expected_nodes, flags, out, &bad);
    platform_free_file(file);
    if (ok) port_log("track light: %s\n", path);
    else if (bad != 0) port_log("track light: %s is not well-formed (line %d), the light is made as before\n", path, bad);
    return ok;
}

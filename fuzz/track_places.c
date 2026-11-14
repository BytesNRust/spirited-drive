/* A track's places file (data/track/<TRK>.txt, or a mod's): tunnels, canyons, car_dim zones */
#include "fuzz/fuzz.h"
#include "game/race/track_light_file.h"

#include <stdio.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char head[128], track[8] = "AL1";
    const size_t n = size < sizeof head - 1 ? size : sizeof head - 1;
    memcpy(head, data, n), head[n] = '\0';
    const char *line = strstr(head, "track ");
    if (line != NULL) (void)sscanf(line, "track %7s", track);
    TrackNodeLight light;
    memset(&light, 0, sizeof light);
    int bad = 0;
    if (track_light_place_parse((const char *)data, size, track, 2080, &light, &bad)) track_node_light_free(&light);
    return 0;
}

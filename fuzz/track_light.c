/* A track's light file (track_light/<TRK>.txt, data/ or a mod): the track is taken from its first line, the time and the node count from the input's first bytes so every line is reached at every time */
#include "fuzz/fuzz.h"
#include "game/race/track_light_track.h"

#include <stdio.h>
#include <string.h>

enum { MOST_NODES = 8192 };

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char head[256], track[8] = "AL1";
    const size_t n = size < sizeof head - 1 ? size : sizeof head - 1;
    memcpy(head, data, n), head[n] = '\0';
    const char *line = strstr(head, "track ");
    if (line != NULL) (void)sscanf(line, "track %7s", track);
    const unsigned nodes = size > 0 ? 1u + (unsigned)((data[0] << 5) % MOST_NODES) : 2080u;
    for (int t = 0; t < 4; t++) {
        TrackLightFile file;
        memset(&file, 0, sizeof file);
        int bad = 0;
        if (track_light_track_parse((const char *)data, size, track, "ABCN"[t], nodes, &file, &bad)) track_light_file_free(&file);
    }
    return 0;
}

/* A track's light file (track_light/<TRK><L>.txt, data/ or a mod): its header names the track, the time and the nodes, taken from the input
 * itself so every line past it is reached */
#include "fuzz/fuzz.h"
#include "game/race/track_light_file.h"

#include <stdio.h>
#include <string.h>

enum { MOST_NODES = 8192 };

static TrackLightFile file;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char head[256], track[8] = "AL1";
    char letter = 'A';
    unsigned nodes = 2080;
    const size_t n = size < sizeof head - 1 ? size : sizeof head - 1;
    memcpy(head, data, n), head[n] = '\0';
    const char *line = strstr(head, "track ");
    if (line != NULL) (void)sscanf(line, "track %7s time %c nodes %u", track, &letter, &nodes);
    if (nodes > MOST_NODES) nodes = MOST_NODES;
    int bad = 0;
    memset(&file, 0, sizeof file);
    if (track_light_parse((const char *)data, size, track, letter, nodes, TRACK_LIGHT_ANY_FLAGS, &file, &bad)) track_light_file_free(&file);
    return 0;
}

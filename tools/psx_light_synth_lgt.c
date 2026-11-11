/* psx_light_synth_lgt <data_dir> <track> <A|B|C> <out.lgt>: writes our own made light for a track as an LGT file (8-byte header, rows), a test input for psx_light_mae and
 * psx_light_fit when there is no CD: both tools must then find our own model exactly (MAE near 0). */
#include "game/race/track_node_light.h"
#include "game/race/terrain_light.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 5) return fprintf(stderr, "usage: psx_light_synth_lgt <data_dir> <track> <A|B|C> <out.lgt>\n"), 2;
    const int time = toupper((unsigned char)argv[3][0]) == 'A' ? LIGHT_SYNTH_DAWN : toupper((unsigned char)argv[3][0]) == 'C' ? LIGHT_SYNTH_DUSK : LIGHT_SYNTH_DAY;
    char path[512];
    snprintf(path, sizeof path, "%s/SIMDATA/MISC/%s.TRI", argv[1], argv[2]);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return fprintf(stderr, "cannot read %s\n", path), 1;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (fread(data, 1, (size_t)size, f) != (size_t)size) return 1;
    fclose(f);
    static Tri tri;
    if (tri_open(&tri, data, (size_t)size) != TRI_OK) return fprintf(stderr, "not a TRI\n"), 1;
    TrackNodeLight light;
    if (!terrain_light(&tri, time, &light)) return fprintf(stderr, "no light\n"), 1;
    FILE *out = fopen(argv[4], "wb");
    const uint32_t header[2] = { light.nodes, 0 };
    fwrite(header, sizeof header, 1, out);
    fwrite(light.data, 1, (size_t)light.nodes * TRACK_NODE_LIGHT_POINTS, out);
    fclose(out);
    printf("%u nodes\n", light.nodes);
    return 0;
}

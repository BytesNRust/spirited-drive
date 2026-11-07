/* port (tunnel_dry): the places file read for wet_zones_for_track (wet_zones.h); apart from wet_zones.c so that its tests need no platform */
#include "game/race/wet_zones.h"
#include "game/platform/platform.h"
#include "game/race/track_light_file.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <string.h>

uint8_t *wet_zones_for_track(const Tri *tri, const char *name, int reversed)
{
    const uint32_t count = tri != NULL ? tri->ring_nodes : 0;
    uint8_t *property = count != 0 ? malloc(count) : NULL;
    if (property == NULL) return NULL;
    for (uint32_t i = 0; i < count; i++) {
        TriNode n;
        property[i] = tri_node(tri, i, &n) == TRI_OK ? n.property : 0;
    }
    PsxLight places = { 0 };
    int have = 0;
    char path[DATA_PATH_MAX];
    if (tri->node_count != 0 && track_light_place_find(name, path, sizeof path)) {
        size_t size = 0;
        uint8_t *text = platform_read_file(path, &size);
        if (text != NULL) {
            places.nodes = tri->node_count;
            places.reversed = reversed != 0;
            have = track_light_place_parse((const char *)text, size, name, tri->node_count, &places, NULL);
            platform_free_file(text);
        }
    }
    uint8_t *open = wet_zones_make(have ? &places : NULL, property, count, 1);
    if (have && open != NULL) {   /* the places file is the only source: say which runs of nodes the road roofs and it does not close (the rain now falls there) */
        uint8_t *road = wet_zones_make(NULL, property, count, 1);
        uint32_t roofed = 0;
        char runs[160] = "";
        for (uint32_t i = 0, from = 0; road != NULL && i <= count; i++) {
            const int only = i < count && road[i] == 0 && open[i] != 0;
            if (only) { if (roofed == 0 || i == 0 || !(road[i - 1] == 0 && open[i - 1] != 0)) from = i; roofed++; }
            if (!only && i > 0 && road[i - 1] == 0 && open[i - 1] != 0) {
                char one[32];
                snprintf(one, sizeof one, " %u..%u", (unsigned)from, (unsigned)(i - 1));
                if (strlen(runs) + strlen(one) < sizeof runs) strcat(runs, one);
            }
        }
        if (roofed != 0) port_log("wet zones: %s: the road's own roof covers %u nodes the places file does not close:%s\n", name, (unsigned)roofed, runs);
        free(road);
    }
    psx_light_free(&places);
    free(property);
    return open;
}

int track_snow_for(const Tri *tri, const char *name, uint8_t snow_map[PSX_LIGHT_SNOW_SLOTS])
{
    if (snow_map != NULL) memset(snow_map, 0, PSX_LIGHT_SNOW_SLOTS);
    char path[DATA_PATH_MAX];
    if (tri == NULL || tri->node_count == 0 || !track_light_place_find(name, path, sizeof path)) return 0;
    size_t size = 0;
    uint8_t *text = platform_read_file(path, &size);
    if (text == NULL) return 0;
    PsxLight places = { 0 };
    places.nodes = tri->node_count;
    const int have = track_light_place_parse((const char *)text, size, name, tri->node_count, &places, NULL);
    platform_free_file(text);
    const int snow = have && places.snow != 0;
    if (snow && snow_map != NULL) memcpy(snow_map, places.snow_map, PSX_LIGHT_SNOW_SLOTS);
    psx_light_free(&places);
    return snow;
}

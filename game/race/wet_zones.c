#include "game/race/wet_zones.h"
#include "game/race/light_synth.h"
#include "game/race/weather.h"

uint8_t *wet_zones_make(const TrackNodeLight *places, const uint8_t *property, uint32_t count, int ring)
{
    if (property == NULL || count == 0) return NULL;
    uint8_t *closed = malloc(count), *cover = malloc(count), *open = malloc(count);
    if (closed == NULL || cover == NULL || open == NULL) { free(closed); free(cover); free(open); return NULL; }
    if (places != NULL) {
        for (uint32_t i = 0; i < count; i++) closed[i] = (uint8_t)(track_node_light_weather_closed(places, i) != 0);   /* a places file is the only source: its galleries are `tunnel` lines */
    } else {
        light_synth_rain_cover(property, count, ring, cover);   /* no places file: the road's own roofed runs, tunnels and galleries */
        light_synth_tunnels(property, count, ring, closed);
        for (uint32_t i = 0; i < count; i++) closed[i] = (uint8_t)(closed[i] == LIGHT_SYNTH_FLAG_TUNNEL || cover[i] != 0);
    }
    weather_open_nodes(closed, count, open);
    free(closed);
    free(cover);
    return open;
}

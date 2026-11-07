#include "game/race/wet_zones.h"
#include "game/race/psx_light_synth.h"
#include "game/race/weather.h"

uint8_t *wet_zones_make(const uint8_t *property, uint32_t count, int ring)
{
    if (property == NULL || count == 0) return NULL;
    uint8_t *tunnel = malloc(count), *cover = malloc(count), *open = malloc(count);
    if (tunnel == NULL || cover == NULL || open == NULL) { free(tunnel); free(cover); free(open); return NULL; }
    psx_light_synth_tunnels(property, count, ring, tunnel);
    psx_light_synth_rain_cover(property, count, ring, cover);
    for (uint32_t i = 0; i < count; i++) tunnel[i] = (uint8_t)(tunnel[i] == PSX_SYNTH_FLAG_TUNNEL || cover[i] != 0);
    weather_open_nodes(tunnel, count, open);
    free(tunnel);
    free(cover);
    return open;
}

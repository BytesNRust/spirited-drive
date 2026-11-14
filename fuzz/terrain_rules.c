/* The per-track terrain light rules (a player's text file) */
#include "fuzz/fuzz.h"
#include "game/race/terrain_light.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static TerrainLightRule rules[256];
    (void)terrain_light_rules_parse((const char *)data, size, rules, 256);
    (void)terrain_light_rules_parse((const char *)data, size, rules, 1);
    return 0;
}

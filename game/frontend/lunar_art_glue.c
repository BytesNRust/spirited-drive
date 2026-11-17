/* port (dep59, Lunar Springs' entry, L4): Lunar Springs' bank made from the CD's and kept in the player's cache (lunar_art.h) */
#include "game/frontend/lunar_art.h"
#include "game/sys/art_cache.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"

#include <stdio.h>
#include <stdlib.h>

uint8_t *lunar_art_cached(LunarArtKind kind, size_t *size, char *name, size_t name_size)
{
    const char *rust_name = kind == LUNAR_ART_BEST ? "tracksel/rustbst.qfs" : "tracksel/rustsel.qfs";
    size_t rn = 0, ln = 0;
    uint8_t *rust = resource_load_art(rust_name, &rn), *lost = resource_load_art("tracksel/lostsel.qfs", &ln);
    ArtCacheKey key;
    const uint8_t how = (uint8_t)kind;
    art_cache_key_init(&key, "lunar_art", LUNAR_ART_VERSION);
    art_cache_key_add(&key, &how, 1);
    art_cache_key_add(&key, rust, rn), art_cache_key_add(&key, lost, ln);
    snprintf(name, name_size, "lunar_%s_%s.bin", kind == LUNAR_ART_BEST ? "bst" : "sel", resource_german() ? "de" : "en");
    uint8_t *out = rust != NULL ? malloc(rn) : NULL;   /* the made bank is as long as Rusty's (lunar_art.h) */
    size_t on = rn;
    if (out == NULL || !art_cache_read(name, &key, out, on)) {
        free(out), out = NULL, on = 0;
        char why[128] = "";
        if (lunar_art_make(rust, rn, lost, ln, kind, &out, &on, why, sizeof why)) (void)art_cache_write(name, &key, out, on);
        else port_log("lunar art: %s is not made (%s): Rusty Springs' art instead\n", name, why);
    }
    resource_free(rust), resource_free(lost);
    *size = out != NULL ? on : 0;
    return out;
}

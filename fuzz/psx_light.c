/* The PlayStation light and sky files a player points the port at: LGS (ambient), LGT (node light), INF (sky) and the PSH panorama */
#include "fuzz/fuzz.h"
#include "game/race/psx_lgs.h"
#include "game/race/psx_lgt.h"
#include "game/race/psx_sky.h"
#include "game/race/track_node_light.h"

#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0) return 0;
    const uint8_t *file = data + 1;
    const size_t n = size - 1;
    switch (data[0] % 4) {
    case 0: { PsxLgs lgs; (void)psx_lgs_parse(file, n, &lgs); break; }
    case 1: {
        TrackNodeLight light;
        memset(&light, 0, sizeof light);
        if (psx_lgt_parse(file, n, 2080, &light)) track_node_light_free(&light);
        break;
    }
    case 2: { PsxSkyInf inf; (void)psx_sky_inf_parse(file, n, &inf); break; }
    default: {
        PsxSky sky;
        if (psx_sky_panorama(file, n, &sky)) fuzz_touch(sky.rgb, (size_t)sky.width * (size_t)sky.height * 3), psx_sky_free(&sky);
        break;
    }
    }
    return 0;
}

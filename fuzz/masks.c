/* The mask PNGs (data/ or a mod): car, track and dash masks and the dash's colours, read at the size the game asks; the first byte
 * picks the reader and a size */
#include "fuzz/fuzz.h"
#include "game/race/car_mask.h"
#include "game/race/dash_color.h"
#include "game/race/dash_mask.h"
#include "game/race/track_mask.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0) return 0;
    static const int SIZES[][2] = { { 256, 256 }, { 64, 32 }, { 1, 1 }, { DASH_MASK_W, DASH_MASK_H } };
    const int *wh = SIZES[(data[0] >> 2) % 4];
    const uint8_t *png = data + 1;
    const size_t n = size - 1;
    uint8_t *out = malloc((size_t)wh[0] * (size_t)wh[1]);
    if (out == NULL) return 0;
    switch (data[0] % 4) {
    case 0: (void)car_mask_parse(png, n, wh[0], wh[1], out); break;
    case 1: (void)track_mask_parse(png, n, wh[0], wh[1], out); break;
    case 2: (void)dash_mask_parse(png, n, wh[0], wh[1], out); break;
    default: {
        DashColour c = { 0 };
        if (dash_color_parse(png, n, wh[0], wh[1], &c)) dash_color_free(&c);
        break;
    }
    }
    free(out);
    return 0;
}

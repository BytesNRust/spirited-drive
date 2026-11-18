/* port (dep59 step 5, T7e2): Best Times' bank for an added track (added_best_art.h). */
#include "game/frontend/added_best_art.h"
#include "formats/shpi_build.h"

#include <stdlib.h>
#include <string.h>

enum { ENTRIES_MAX = 48 };

int added_best_art_make(const uint8_t *coast, size_t size, uint8_t **out, size_t *out_size, char *why, size_t why_size)
{
    ShpiDescEntry e[ENTRIES_MAX];
    ExchangeError err;
    size_t n = 0;
    *out = NULL, *out_size = 0;
    if (coast == NULL || !shpi_describe(coast, size, e, ENTRIES_MAX, &n, &err)) {
        if (why != NULL && why_size > 0) strncpy(why, "not a bank", why_size - 1), why[why_size - 1] = '\0';
        return 0;
    }
    int bgnd = -1;
    for (size_t i = 0; i < n; i++)
        if (memcmp(e[i].name, "bgnd", 4) == 0 && e[i].type == 0x7b && e[i].width == ADDED_BEST_W && e[i].height == ADDED_BEST_H) bgnd = (int)i;
    uint8_t *px = bgnd >= 0 ? malloc((size_t)ADDED_BEST_W * ADDED_BEST_H) : NULL;
    if (px == NULL) {
        if (why != NULL && why_size > 0) strncpy(why, bgnd < 0 ? "no 640 x 480 bgnd" : "no room", why_size - 1), why[why_size - 1] = '\0';
        return 0;
    }
    memcpy(px, e[bgnd].data, (size_t)ADDED_BEST_W * ADDED_BEST_H);
    for (int y = ADDED_BEST_BOX_Y0; y <= ADDED_BEST_BOX_Y1; y++)   /* the page's own texture, from the rows below the column heads, same columns */
        memcpy(px + y * ADDED_BEST_W + ADDED_BEST_BOX_X0, px + (y + ADDED_BEST_SOURCE_SHIFT) * ADDED_BEST_W + ADDED_BEST_BOX_X0,
               (size_t)(ADDED_BEST_BOX_X1 - ADDED_BEST_BOX_X0 + 1));
    e[bgnd].data = px;
    const int ok = shpi_build(e, n, out, out_size, &err);
    if (!ok && why != NULL && why_size > 0) strncpy(why, err.message, why_size - 1), why[why_size - 1] = '\0';
    free(px);
    return ok;
}

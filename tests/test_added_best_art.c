/* An added track's Best Times bank (added_best_art.c): Coastal's with its title's box filled again from the texture above it, the rest of the
 * bank as it was. The bank here is made by hand: a 640 x 480 bgnd whose rows are distinct, a palette, one more picture. */
#include "game/frontend/added_best_art.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { PIXELS = ADDED_BEST_W * ADDED_BEST_H };

static uint8_t *bank_make(size_t *size, int with_bgnd)
{
    static uint8_t bgnd[PIXELS], pal[SHPI_PALETTE_BYTES], dim[16 * 8];
    for (int y = 0; y < ADDED_BEST_H; y++)
        for (int x = 0; x < ADDED_BEST_W; x++) bgnd[y * ADDED_BEST_W + x] = (uint8_t)(y % 200 + (x >= 20 && x <= 100 && y >= 14 && y <= 44 ? 50 : 0));   /* a "title" in the box */
    memset(dim, 7, sizeof dim);
    ShpiDescEntry e[3];
    memset(e, 0, sizeof e);
    memcpy(e[0].name, "bgnd", 4), e[0].type = 0x7b, e[0].width = ADDED_BEST_W, e[0].height = ADDED_BEST_H, e[0].data = bgnd;
    memcpy(e[1].name, "!pal", 4), e[1].type = 0x22, e[1].width = 256, e[1].height = 1, e[1].data = pal;
    memcpy(e[2].name, "dim1", 4), e[2].type = 0x7b, e[2].width = 16, e[2].height = 8, e[2].data = dim;
    for (size_t i = 0; i < 3; i++) e[i].block_size = SHPI_BLOCK_AUTO, e[i].dir_index = (uint32_t)i;
    if (!with_bgnd) memcpy(e[0].name, "xxxx", 4);
    ExchangeError err;
    uint8_t *out = NULL;
    CHECK(shpi_build(e, 3, &out, size, &err));
    return out;
}

static void test_the_title_box_is_filled_again(void)
{
    size_t size = 0, made_size = 0;
    uint8_t *bank = bank_make(&size, 1), *made = NULL;
    char why[64];
    CHECK(added_best_art_make(bank, size, &made, &made_size, why, sizeof why));
    ShpiDescEntry in[3], out[3];
    ExchangeError err;
    size_t n = 0, m = 0;
    CHECK(shpi_describe(bank, size, in, 3, &n, &err) && shpi_describe(made, made_size, out, 3, &m, &err) && n == 3 && m == 3);
    int title_left = 0, box_changed = 0, outside_same = 1;
    for (int y = 0; y < ADDED_BEST_H; y++)
        for (int x = 0; x < ADDED_BEST_W; x++) {
            const int inside = x >= ADDED_BEST_BOX_X0 && x <= ADDED_BEST_BOX_X1 && y >= ADDED_BEST_BOX_Y0 && y <= ADDED_BEST_BOX_Y1;
            const uint8_t a = in[0].data[y * ADDED_BEST_W + x], b = out[0].data[y * ADDED_BEST_W + x];
            if (inside) box_changed += a != b, title_left += b != (uint8_t)((y + ADDED_BEST_SOURCE_SHIFT) % 200);   /* the rows below's texture, with no lift of 50 */
            else outside_same &= a == b;
        }
    CHECK(outside_same);                              /* not a pixel past the box */
    CHECK(box_changed > 0 && title_left == 0);       /* the title is gone */
    CHECK(memcmp(in[1].data, out[1].data, SHPI_PALETTE_BYTES) == 0);   /* the palette */
    CHECK(memcmp(in[2].data, out[2].data, 16 * 8) == 0 && memcmp(in[2].name, out[2].name, 4) == 0);   /* the other pictures */
    free(bank), free(made);
}

static void test_a_bank_without_the_picture_is_refused(void)
{
    size_t size = 0, made_size = 0;
    uint8_t *bank = bank_make(&size, 0), *made = NULL;
    char why[64] = "";
    CHECK(!added_best_art_make(bank, size, &made, &made_size, why, sizeof why));
    CHECK(made == NULL && why[0] != '\0');
    CHECK(!added_best_art_make(NULL, 0, &made, &made_size, why, sizeof why));
    free(bank);
}

int main(void)
{
    test_the_title_box_is_filled_again();
    test_a_bank_without_the_picture_is_refused();
    return TEST_REPORT();
}

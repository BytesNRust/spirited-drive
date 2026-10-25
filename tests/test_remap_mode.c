/* MAP_REMAP and MAP_REMAP_TRANSPARENT (quad.h): the same quad or triangle drawn with the plain mode and with the remap
 * mode differs only by the table: each pixel the plain draw painted holds the table's entry for it. Synthetic textures. */
#include "game/gfx/quad.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 96, H = 80, BACK = 0x7F, TEX = 16 };

static uint8_t tex_pixels[TEX * TEX];
static FshEntry texture(int transparent_type)
{
    for (int i = 0; i < TEX * TEX; i++) tex_pixels[i] = (uint8_t)(i % 11 == 0 ? 0xFF : 1 + i % 120);
    FshEntry e = { .type = FSH_TYPE_BITMAP, .width = TEX, .height = TEX, .payload = tex_pixels, .payload_size = TEX * TEX };
    (void)transparent_type;
    return e;
}

static uint8_t table[256];

static void fill_table(void)
{
    for (int i = 0; i < 256; i++) table[i] = (uint8_t)(i == 0xFF ? 0xFF : (i * 3 + 5) % 126);   /* never BACK */
}

static int check_pair(const uint8_t *plain, const uint8_t *remapped)
{
    long painted = 0;
    for (int i = 0; i < W * H; i++) {
        if (plain[i] == BACK) {
            if (remapped[i] != BACK) return -1;
        } else {
            if (remapped[i] != table[plain[i]]) return -1;
            painted++;
        }
    }
    return (int)painted;
}

int main(void)
{
    fill_table();
    const FshEntry t = texture(0);
    uint8_t a[W * H], b[W * H];
    for (int transparent = 0; transparent <= 1; transparent++) {
        const MapMode plain = transparent ? MAP_TRANSPARENT : MAP_OPAQUE, remap = transparent ? MAP_REMAP_TRANSPARENT : MAP_REMAP;
        for (int k = 0; k < 40; k++) {
            memset(a, BACK, sizeof a), memset(b, BACK, sizeof b);
            const QuadTarget ta = { a, W, 0, 0, W, H }, tb = { b, W, 0, 0, W, H };
            const int32_t o = 4 * k % 20;
            const int32_t uv[8] = { 0, 0, (TEX << 16) - 1, 0, (TEX << 16) - 1, (TEX << 16) - 1, 0, (TEX << 16) - 1 };
            const int32_t screen[8] = { 8 + o, 6, 70 + o / 2, 10 + k % 7, 74, 66 - o / 3, 10, 60 };
            quad_draw_mode(&ta, &t, uv, screen, plain, NULL);
            quad_draw_mode(&tb, &t, uv, screen, remap, table);
            const int painted = check_pair(a, b);
            CHECK(painted > 300);
            const int32_t tuv[6] = { 1, 1, 14, 1, 7, 14 };
            const int32_t tscreen[6] = { 5 + k, 70, 80, 60 - k % 9, 40, 4 };
            memset(a, BACK, sizeof a), memset(b, BACK, sizeof b);
            tri_draw_mode(&ta, &t, tuv, tscreen, plain, NULL);
            tri_draw_mode(&tb, &t, tuv, tscreen, remap, table);
            CHECK(check_pair(a, b) > 100);
        }
    }
    /* a remap without a table draws nothing */
    memset(a, BACK, sizeof a);
    const QuadTarget ta = { a, W, 0, 0, W, H };
    const int32_t uv[8] = { 0, 0, 1 << 20, 0, 1 << 20, 1 << 20, 0, 1 << 20 }, screen[8] = { 5, 5, 50, 5, 50, 50, 5, 50 };
    quad_draw_mode(&ta, &t, uv, screen, MAP_REMAP, NULL);
    for (int i = 0; i < W * H; i++) CHECK(a[i] == BACK);
    return TEST_REPORT();
}

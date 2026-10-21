/* The road's near plane (track_render_near): the exe's 004c52c0 >> shift, and the port's scale for a wider field. */
#include "game/race/track_render.h"
#include "test.h"

int main(void)
{
    DrawContext ctx = { 0 };
    ctx.world_shift = 3;
    CHECK_EQ_INT(track_render_near(&ctx), 62259 >> 3);
    ctx.port_near_scale = 65536;
    CHECK_EQ_INT(track_render_near(&ctx), 62259 >> 3);
    ctx.port_near_scale = 32768;
    CHECK_EQ_INT(track_render_near(&ctx), 31129 >> 3);
    ctx.port_near_scale = 45889;
    CHECK_EQ_INT(track_render_near(&ctx), (int32_t)((int64_t)62259 * 45889 >> 16) >> 3);
    return TEST_REPORT();
}

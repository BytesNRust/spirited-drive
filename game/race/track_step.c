/* The track's streaming each frame (0043f540). */
#include "game/race/track_step.h"

#include "game/gfx/x86_int.h"

enum { MIRROR_BEHIND = 4, MIRROR_ROWS = 5, NODE_ROUND = 3 };

void track_stream_step(const TrackStepOps *ops, const TrackStepInputs *in)
{
    const int32_t segment = in->node / 4;
    ops->stream(ops->ctx, segment);
    if (*in->unknown_4c53a4 != 0) *in->unknown_4c53a4 = 0;
    if (in->stream_node / 4 != segment || in->stream_node < 0) ops->build(ops->ctx, in->node, segment, in->draw_segments);
    if (*in->rows_again == 0) return;
    int32_t first = (x_add(in->node, NODE_ROUND) / 4 - MIRROR_BEHIND) & in->segment_mask;
    if (first != *in->mirror_segment) {
        *in->mirror_segment = first;
        *in->mirror_count = 0;
        for (int i = 0; i < MIRROR_ROWS; i++) {
            ops->mirror_row(ops->ctx, in->circuit == 0 && first < 0 ? 0 : first);
            first = (first + 1) & in->segment_mask;
        }
    }
    *in->rows_again = 0;
}

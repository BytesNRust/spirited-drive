/* The distance strip's runs: where each road value starts and ends along the control points (track_trace.h). */
#include "formats/track_trace.h"

int track_trace_strip_runs(const TraceKey *keys, int key_count, int point_count, unsigned bit, TraceStripRun *out, int cap)
{
    if (point_count <= 0 || bit == 0 || (bit & (bit - 1)) != 0) return 0;
    int runs = 0;
    int start = 0, set = 0;   // the open run: from `start`, the template's until a key sets the value
    for (int i = 0; i < key_count; ++i) {
        const TraceKey *k = &keys[i];
        if ((k->has & bit) == 0 || k->point < 0 || k->point >= point_count) continue;
        if (k->point < start) continue;   // out of order: the list is ascending, so none is
        if (k->point > start) {   // close the open run here (the template's is empty when the key is on the first point)
            if (runs < cap && out != NULL) out[runs] = (TraceStripRun){ start, k->point, set };
            ++runs;
        }
        start = k->point;
        set = 1;
    }
    if (runs < cap && out != NULL) out[runs] = (TraceStripRun){ start, point_count - 1, set };
    return runs + 1;
}

#include "value_help.h"

#include "formats/track_trace.h"

const char *road_value_help(unsigned bit)
{
    if (bit == TRACE_KEY_WIDTH) return "How wide the road is here, edge to edge. It changes smoothly from one set point to the next.";
    if (bit == TRACE_KEY_BANK) return "How much the road tilts across its width here, in degrees. Too much and Build refuses the road.";
    if (bit == TRACE_KEY_LANES) return "How many lanes each way, 5 m each. 1+2 or 2+1 open a third lane and make the road 15 m wide.";
    if (bit == TRACE_KEY_SHOULDER_SURFACE) return "What the strip beside the road is: Asphalt grips like the road, Off-road loses grip (the track's Off-road ground).";
    for (int side = 0; side < 2; ++side) {
        if (bit == TRACE_KEY_SIDE(TRACE_SIDE_SHOULDER, side)) return "How far the shoulder reaches past the road's edge on this side. The fence stands at its end.";
        if (bit == TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, side)) return "How far past the road's edge the barrier stops the car. The barrier is not drawn; it counts only with Custom barrier.";
        if (bit == TRACE_KEY_SIDE(TRACE_SIDE_WALL, side)) return "Hard or soft barrier: a hard hit makes the crash sound and is recorded in the race; a soft one is silent and turns the car gently.";
        if (bit == TRACE_KEY_SIDE(TRACE_SIDE_FENCE, side)) return "Draws the game's fence on this side, at the end of the shoulder. Off draws none; the barrier still stops the car.";
        if (bit == TRACE_KEY_BARRIER_CUSTOM(side)) return "On: the barrier stops the car at Barrier from edge instead of where the fence stands. Off: it follows the fence.";
    }
    if (bit == TRACE_KEY_FENCE_HEIGHT) return "How tall the fence is: Low is 0.9 m, Tall is 2 m. The game keeps one height for both sides.";
    if (bit == TRACE_KEY_FENCE_ART) return "The fence's picture: Planks and Rail see through, Parapet a solid stone wall. One picture for both sides.";
    return 0;
}

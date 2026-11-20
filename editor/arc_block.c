/* Add curve and Add straight's points (arc_block.h). */
#include "arc_block.h"

#include <math.h>

int arc_block(int right, double degrees, double radius_m, int count, TracePoint *out)
{
    if (!(degrees > 0 && degrees <= ARC_DEGREES_MAX) || !(radius_m > 0) || count < 1 || count > ARC_POINTS_MAX) return 0;
    const double r = radius_m / TRACE_METRES_PER_UNIT, side = right ? 1 : -1;
    for (int i = 0; i <= count; ++i) {   /* about the centre (side * r, 0): angle t from the start, going +z first */
        const double t = degrees * M_PI / 180.0 * i / count;
        out[i] = (TracePoint){ side * (r - r * cos(t)), r * sin(t) };
    }
    return 1;
}

int straight_block(double length_m, int count, TracePoint *out)
{
    if (!(length_m > 0 && length_m <= STRAIGHT_LENGTH_MAX_M) || count < 1 || count > ARC_POINTS_MAX) return 0;
    for (int i = 0; i <= count; ++i) out[i] = (TracePoint){ 0, length_m / TRACE_METRES_PER_UNIT * i / count };
    return 1;
}

double arc_length_m(double degrees, double radius_m) { return radius_m * degrees * M_PI / 180.0; }

static int clamp_count(double n)
{
    const int c = (int)ceil(n - 1e-9);
    return c < 1 ? 1 : c > ARC_POINTS_MAX ? ARC_POINTS_MAX : c;
}

int arc_default_count(double degrees, double radius_m)
{
    const double by_length = arc_length_m(degrees, radius_m) / ARC_CHORD_MAX_M, by_turn = degrees / ARC_CHORD_TURN_MAX;
    return clamp_count(by_length > by_turn ? by_length : by_turn);
}

int straight_default_count(double length_m) { return clamp_count(length_m / STRAIGHT_CHORD_MAX_M); }

TracePoint arc_end_heading(int right, double degrees)
{
    const double t = degrees * M_PI / 180.0;
    return (TracePoint){ (right ? 1 : -1) * sin(t), cos(t) };
}

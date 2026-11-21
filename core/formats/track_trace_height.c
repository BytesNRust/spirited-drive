/* The control points' heights (dep95): see track_trace.h. */
#include "formats/track_trace.h"

#include <math.h>

/* the chord length from point i to point i+1 (never 0: a zero chord is a point on top of another, and the run is then taken as 1 unit so the slopes stay finite) */
static double chord(const TracePoint *p, int i)
{
    const double d = hypot(p[i + 1].x - p[i].x, p[i + 1].z - p[i].z);
    return d > 1e-9 ? d : 1.0;
}

static int sign(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }

/* the slope of the height at point i along the chords (a monotone cubic: Fritsch-Carlson in the middle, Fritsch-Butland at the ends): 0 at a local extremum, never a slope
 * that makes the cubic leave the range of the two heights it joins */
static double slope_at(const TracePoint *p, int n, int i)
{
    if (n < 2) return 0;
    const double d_prev = i > 0 ? (p[i].h - p[i - 1].h) / chord(p, i - 1) : 0, d_next = i + 1 < n ? (p[i + 1].h - p[i].h) / chord(p, i) : 0;
    if (i > 0 && i + 1 < n) {
        if (d_prev * d_next <= 0) return 0;
        const double h_prev = chord(p, i - 1), h_next = chord(p, i), w1 = 2 * h_next + h_prev, w2 = h_next + 2 * h_prev;
        return (w1 + w2) / (w1 / d_prev + w2 / d_next);
    }
    if (n == 2) return i == 0 ? d_next : d_prev;
    // an end: the three-point slope, held to the sign of the chord beside it and to three times its slope when the next chord turns the other way
    const int first = i == 0;
    const double h0 = first ? chord(p, 0) : chord(p, n - 2), h1 = first ? chord(p, 1) : chord(p, n - 3), d0 = first ? d_next : d_prev;
    const double d1 = first ? (p[2].h - p[1].h) / h1 : (p[n - 2].h - p[n - 3].h) / h1;
    double m = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (sign(m) != sign(d0)) m = 0;
    else if (sign(d0) != sign(d1) && fabs(m) > 3 * fabs(d0)) m = 3 * d0;
    return m;
}

double track_trace_height_at(const TracePoint *p, int count, int seg, double frac)
{
    if (p == NULL || count < 2 || seg < 0 || seg + 1 >= count) return 0;
    const double t = frac < 0 ? 0 : frac > 1 ? 1 : frac, h = chord(p, seg), dy = p[seg + 1].h - p[seg].h;
    const double m0 = slope_at(p, count, seg), m1 = slope_at(p, count, seg + 1);
    const double t2 = t * t, t3 = t2 * t;
    // the Hermite form on the first height, so equal heights with no slope give exactly that height and the ends give exactly the points
    return p[seg].h + dy * (3 * t2 - 2 * t3) + h * m0 * (t3 - 2 * t2 + t) + h * m1 * (t3 - t2);
}

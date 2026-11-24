/* port (track light, STRIP): see light_points.h. */
#include "formats/light_points.h"

#include <stdio.h>
#include <string.h>

enum { POINTS = 11 };

int light_points_parse(const char *word, unsigned *mask)
{
    if (word == NULL || strchr(word, ',') == NULL) return 0;
    unsigned m = 0;
    for (const char *c = word;;) {
        if (c[0] != 'p' || c[1] < '0' || c[1] > '9') return -1;
        unsigned v = (unsigned)(c[1] - '0');
        c += 2;
        if (*c >= '0' && *c <= '9') {   /* two digits: p10 only (no p03) */
            if (v == 0) return -1;
            v = v * 10u + (unsigned)(*c++ - '0');
        }
        if (v >= POINTS || (*c >= '0' && *c <= '9')) return -1;
        m |= 1u << v;
        if (*c == '\0') break;
        if (*c++ != ',') return -1;
    }
    *mask = m;
    return 1;
}

int light_points_text(unsigned mask, char *out, size_t cap)
{
    if (cap > 0) out[0] = '\0';
    if (mask == 0 || (mask >> POINTS) != 0) return 0;
    size_t len = 0;
    for (int p = 0; p < POINTS; p++) {
        if (!((mask >> p) & 1u)) continue;
        const int n = snprintf(out + len, cap - len, "%sp%d", len > 0 ? "," : "", p);
        if (n < 0 || (size_t)n >= cap - len) return out[0] = '\0', 0;
        len += (size_t)n;
    }
    return 1;
}

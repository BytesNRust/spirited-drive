#include "light_strip.h"

#include <stdio.h>
#include <string.h>

static const int CELLS[LIGHT_STRIP_POINTS] = { 10, 9, 8, 7, 6, 0, 1, 2, 3, 4, 5 };

static int point_of(const char *word)
{
    if (word == NULL || word[0] != 'p' || word[1] < '0' || word[1] > '9') return -1;
    int v = word[1] - '0';
    if (word[2] != '\0') {
        if (word[2] < '0' || word[2] > '9' || word[3] != '\0') return -1;
        v = v * 10 + (word[2] - '0');
    }
    return v < LIGHT_STRIP_POINTS && !(word[1] == '0' && word[2] != '\0') ? v : -1;
}

int light_strip_parse(const char *const *words, int n, unsigned *mask)
{
    if (n < 1 || words[0] == NULL) return 0;
    if (strcmp(words[0], "all") == 0) return *mask = LIGHT_STRIP_ALL, 1;
    if (strcmp(words[0], "right") == 0) return *mask = LIGHT_STRIP_RIGHT, 1;
    if (strcmp(words[0], "left") == 0) return *mask = LIGHT_STRIP_LEFT, 1;
    if (strcmp(words[0], "road") == 0) return *mask = LIGHT_STRIP_ROAD, 1;
    const int a = point_of(words[0]);
    if (a < 0) return 0;
    const int b = n > 1 ? point_of(words[1]) : -1;
    if (b < 0) return *mask = 1u << a, 1;
    if (b < a) return 0;
    unsigned m = 0;
    for (int k = a; k <= b; k++) m |= 1u << k;
    return *mask = m, 2;
}

int light_strip_text(unsigned mask, char *out, size_t cap)
{
    out[0] = '\0';
    if (mask == LIGHT_STRIP_ALL) return snprintf(out, cap, "all"), 1;
    if (mask == LIGHT_STRIP_RIGHT) return snprintf(out, cap, "right"), 1;
    if (mask == LIGHT_STRIP_LEFT) return snprintf(out, cap, "left"), 1;
    if (mask == LIGHT_STRIP_ROAD) return snprintf(out, cap, "road"), 1;
    if (mask == 0 || (mask & ~(unsigned)LIGHT_STRIP_ALL) != 0) return 0;
    int lo = 0;
    while (!(mask >> lo & 1u)) lo++;
    int hi = lo;
    while (hi + 1 < LIGHT_STRIP_POINTS && (mask >> (hi + 1) & 1u)) hi++;
    if (mask >> (hi + 1) != 0) return 0;   // a gap: not one line
    if (lo == hi) snprintf(out, cap, "p%d", lo);
    else snprintf(out, cap, "p%d p%d", lo, hi);
    return 1;
}

int light_strip_cell_point(int cell)
{
    return cell >= 0 && cell < LIGHT_STRIP_POINTS ? CELLS[cell] : -1;
}

unsigned light_strip_span(int a, int b)
{
    if (a > b) { const int t = a; a = b, b = t; }
    unsigned m = 0;
    for (int c = a < 0 ? 0 : a; c <= b && c < LIGHT_STRIP_POINTS; c++) m |= 1u << CELLS[c];
    return m;
}

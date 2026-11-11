/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see boot_quant.h. */
#include "game/frontend/boot_quant.h"

#include <stdlib.h>
#include <string.h>

enum { BINS = 32 * 32 * 32 };

typedef struct {
    int lo[3], hi[3];   /* bin coordinates, inclusive */
    long count;
} Box;

static int bin_of(const uint8_t *c) { return (c[0] >> 3) << 10 | (c[1] >> 3) << 5 | c[2] >> 3; }

static long box_count(const long *hist, Box *b)
{
    long n = 0;
    int lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 };
    for (int r = b->lo[0]; r <= b->hi[0]; r++)
        for (int g = b->lo[1]; g <= b->hi[1]; g++)
            for (int bl = b->lo[2]; bl <= b->hi[2]; bl++) {
                const long h = hist[r << 10 | g << 5 | bl];
                if (h == 0) continue;
                n += h;
                const int v[3] = { r, g, bl };
                for (int c = 0; c < 3; c++) lo[c] = v[c] < lo[c] ? v[c] : lo[c], hi[c] = v[c] > hi[c] ? v[c] : hi[c];
            }
    if (n > 0) memcpy(b->lo, lo, sizeof lo), memcpy(b->hi, hi, sizeof hi);   /* shrunk to what it holds */
    b->count = n;
    return n;
}

/* splits b at the median of its widest channel; 0 when it is a single bin */
static int split(const long *hist, Box *b, Box *out)
{
    int axis = 0;
    for (int c = 1; c < 3; c++)
        if (b->hi[c] - b->lo[c] > b->hi[axis] - b->lo[axis]) axis = c;
    if (b->hi[axis] == b->lo[axis]) return 0;
    long acc = 0;
    int cut = b->lo[axis];
    for (; cut < b->hi[axis]; cut++) {
        Box slab = *b;
        slab.lo[axis] = slab.hi[axis] = cut;
        acc += box_count(hist, &slab);
        if (2 * acc >= b->count) break;
    }
    *out = *b;
    b->hi[axis] = cut, out->lo[axis] = cut + 1;
    box_count(hist, b);
    box_count(hist, out);
    return 1;
}

static uint8_t nearest(const uint8_t *palette, int colours, const uint8_t *c)
{
    int best = 0;
    long best_d = -1;
    for (int i = 0; i < colours; i++) {
        long d = 0;
        for (int k = 0; k < 3; k++) d += (long)(palette[3 * i + k] - c[k]) * (palette[3 * i + k] - c[k]);
        if (best_d < 0 || d < best_d) best = i, best_d = d;
    }
    return (uint8_t)best;
}

int boot_quant(const uint8_t *rgb, int n, int colours, uint8_t palette[768], uint8_t *indices)
{
    long *hist = calloc(BINS, sizeof(long)), *sum = calloc((size_t)BINS * 3, sizeof(long));
    Box *boxes = malloc(sizeof(Box) * (size_t)colours);
    if (hist == NULL || sum == NULL || boxes == NULL || colours < 1 || colours > 256) {
        free(hist), free(sum), free(boxes);
        return 0;
    }
    for (int i = 0; i < n; i++) {
        const int b = bin_of(rgb + 3 * i);
        hist[b]++;
        for (int c = 0; c < 3; c++) sum[3 * b + c] += rgb[3 * i + c];
    }
    int made = 1;
    boxes[0] = (Box){ { 0, 0, 0 }, { 31, 31, 31 }, 0 };
    box_count(hist, &boxes[0]);
    while (made < colours) {
        int pick = -1;
        for (int i = 0; i < made; i++)
            if ((boxes[i].hi[0] > boxes[i].lo[0] || boxes[i].hi[1] > boxes[i].lo[1] || boxes[i].hi[2] > boxes[i].lo[2]) &&
                (pick < 0 || boxes[i].count > boxes[pick].count)) pick = i;
        if (pick < 0 || !split(hist, &boxes[pick], &boxes[made])) break;
        made++;
    }
    memset(palette, 0, 3 * (size_t)colours);
    for (int i = 0; i < made; i++) {
        long s[3] = { 0, 0, 0 }, cnt = 0;
        for (int r = boxes[i].lo[0]; r <= boxes[i].hi[0]; r++)
            for (int g = boxes[i].lo[1]; g <= boxes[i].hi[1]; g++)
                for (int bl = boxes[i].lo[2]; bl <= boxes[i].hi[2]; bl++) {
                    const int b = r << 10 | g << 5 | bl;
                    cnt += hist[b];
                    for (int c = 0; c < 3; c++) s[c] += sum[3 * b + c];
                }
        for (int c = 0; c < 3; c++) palette[3 * i + c] = (uint8_t)(cnt > 0 ? (s[c] + cnt / 2) / cnt : 0);
    }
    uint8_t last[3] = { 0, 0, 0 }, last_index = nearest(palette, made, last);   /* neighbours repeat: remember the last match */
    for (int i = 0; i < n; i++) {
        if (memcmp(rgb + 3 * i, last, 3) != 0) memcpy(last, rgb + 3 * i, 3), last_index = nearest(palette, made, last);
        indices[i] = last_index;
    }
    free(hist), free(sum), free(boxes);
    return made;
}

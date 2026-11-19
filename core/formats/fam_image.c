/* See fam_image.h. The median cut runs over a histogram of 64 x 64 x 64 cells (the 6-bit colours), so a split is a scan of a box of
 * cells, not a sort of the pixels. */
#include "formats/fam_image.h"

#include <stdlib.h>
#include <string.h>

enum { CELL_LEVELS = 64, CELLS = CELL_LEVELS * CELL_LEVELS * CELL_LEVELS, CHANNELS = 3 };

typedef struct {
    int lo[CHANNELS], hi[CHANNELS];   /* a box of cells: lo inclusive, hi exclusive */
    uint64_t population;
} Box;

static size_t cell_of(uint8_t r, uint8_t g, uint8_t b)
{
    return ((size_t)(r >> 2) * CELL_LEVELS + (g >> 2)) * CELL_LEVELS + (b >> 2);
}

/* the histogram of the colours of `pics` (CELLS counts, calloc'd); NULL when memory runs out */
static uint32_t *histogram_of(const FamPicture *pics, size_t n)
{
    uint32_t *hist = calloc(CELLS, sizeof *hist);
    if (hist == NULL) return NULL;
    for (size_t p = 0; p < n; p++) {
        const size_t count = (size_t)pics[p].width * pics[p].height;
        for (size_t i = 0; i < count; i++) {
            const uint8_t *px = pics[p].rgba + i * 4;
            hist[cell_of(px[0], px[1], px[2])]++;
        }
    }
    return hist;
}

/* the cell counts of a box along `axis`, slice by slice; `slices` gets the population of each */
static void slices_along(const uint32_t *hist, const Box *box, int axis, uint64_t slices[CELL_LEVELS])
{
    for (int s = box->lo[axis]; s < box->hi[axis]; s++) slices[s] = 0;
    for (int r = box->lo[0]; r < box->hi[0]; r++)
        for (int g = box->lo[1]; g < box->hi[1]; g++)
            for (int b = box->lo[2]; b < box->hi[2]; b++) {
                const int at[CHANNELS] = { r, g, b };
                slices[at[axis]] += hist[((size_t)r * CELL_LEVELS + g) * CELL_LEVELS + b];
            }
}

/* the axis to cut: the longest side with more than one cell; -1 when no side has one */
static int longest_axis(const Box *box)
{
    int best = -1, best_len = 1;
    for (int axis = 0; axis < CHANNELS; axis++)
        if (box->hi[axis] - box->lo[axis] > best_len) best_len = box->hi[axis] - box->lo[axis], best = axis;
    return best;
}

/* the cut of `box` along `axis` at its population median; the two halves are never empty */
static int median_cut(const uint32_t *hist, const Box *box, int axis, int *cut)
{
    uint64_t slices[CELL_LEVELS] = { 0 };
    slices_along(hist, box, axis, slices);
    uint64_t seen = 0;
    int k = box->lo[axis];
    for (; k < box->hi[axis]; k++) {
        seen += slices[k];
        if (seen * 2 >= box->population) break;
    }
    if (k <= box->lo[axis]) k = box->lo[axis] + 1;
    if (k >= box->hi[axis]) k = box->hi[axis] - 1;
    *cut = k;
    return k > box->lo[axis] && k < box->hi[axis];
}

static uint64_t population_of(const uint32_t *hist, const Box *box)
{
    uint64_t total = 0;
    for (int r = box->lo[0]; r < box->hi[0]; r++)
        for (int g = box->lo[1]; g < box->hi[1]; g++)
            for (int b = box->lo[2]; b < box->hi[2]; b++) total += hist[((size_t)r * CELL_LEVELS + g) * CELL_LEVELS + b];
    return total;
}

/* the box with the most pixels that has an axis to cut; -1 when none has */
static int busiest_splittable(const Box *boxes, int count)
{
    int best = -1;
    for (int i = 0; i < count; i++) {
        if (longest_axis(&boxes[i]) < 0 || boxes[i].population < 2) continue;
        if (best < 0 || boxes[i].population > boxes[best].population) best = i;
    }
    return best;
}

/* the box `boxes[i]` split in two, the second half appended at boxes[count]; 0 when it cannot be split */
static int split_box(const uint32_t *hist, Box *boxes, int i, int count)
{
    const int axis = longest_axis(&boxes[i]);
    int cut = 0;
    if (axis < 0 || !median_cut(hist, &boxes[i], axis, &cut)) return 0;
    Box upper = boxes[i];
    upper.lo[axis] = cut;
    boxes[i].hi[axis] = cut;
    boxes[i].population = population_of(hist, &boxes[i]);
    upper.population = population_of(hist, &upper);
    boxes[count] = upper;
    return 1;
}

/* the mean colour of the cells of a box, 6-bit */
static void box_mean(const uint32_t *hist, const Box *box, uint8_t out[CHANNELS])
{
    uint64_t sum[CHANNELS] = { 0, 0, 0 };
    for (int r = box->lo[0]; r < box->hi[0]; r++)
        for (int g = box->lo[1]; g < box->hi[1]; g++)
            for (int b = box->lo[2]; b < box->hi[2]; b++) {
                const uint64_t n = hist[((size_t)r * CELL_LEVELS + g) * CELL_LEVELS + b];
                sum[0] += n * (uint64_t)r, sum[1] += n * (uint64_t)g, sum[2] += n * (uint64_t)b;
            }
    for (int c = 0; c < CHANNELS; c++) out[c] = (uint8_t)((sum[c] + box->population / 2) / (box->population ? box->population : 1));
}

/* the entries kept back from the median cut for the shades of the colours that have none (reserve_shades) */
enum { SHADE_SLOTS = 8, MAX_BOXES = FAM_PIXEL_INDICES - SHADE_SLOTS };

/* the boxes of the median cut: returns how many there are, at most MAX_BOXES (one per palette entry 0..254) */
static int median_cut_boxes(const uint32_t *hist, Box *boxes)
{
    Box whole = { { 0, 0, 0 }, { CELL_LEVELS, CELL_LEVELS, CELL_LEVELS }, 0 };
    whole.population = population_of(hist, &whole);
    boxes[0] = whole;
    int count = 1;
    while (count < MAX_BOXES) {
        const int i = busiest_splittable(boxes, count);
        if (i < 0 || !split_box(hist, boxes, i, count)) break;
        count++;
    }
    return count;
}

/* the shade of a 6-bit colour as the race makes it (game/sys/palette.c shade_channel, scale FAM_SHADE_SCALE): the colour the shadow quads and skid marks
 * remap it to is the palette's nearest to this one */
static void shade_of(const uint8_t c[CHANNELS], uint8_t out[CHANNELS])
{
    for (int i = 0; i < CHANNELS; i++) {
        const long product = (long)c[i] * FAM_SHADE_SCALE;
        const int v = (int)(product >> 16) + (int)((product >> 15) & 1);
        out[i] = (uint8_t)(v > 0x3F ? 0x3F : v);
    }
}

/* the squared distance of two 6-bit colours in 8-bit units */
static long distance_of(const uint8_t a[CHANNELS], const uint8_t b[CHANNELS])
{
    long d = 0;
    for (int i = 0; i < CHANNELS; i++) d += (long)((a[i] - b[i]) * 4) * ((a[i] - b[i]) * 4);
    return d;
}

/* The shadow of a car is the road's pixels each turned into the palette's nearest to the colour darkened by 0.6, and a palette cut for the pictures alone
 * has no dark yellow: a yellow line under a car went green. So the entries kept back take, one at a time, the shade of the entry whose own shade is
 * farthest from every entry, while it is farther than FAM_SHADE_BAR: the same hue, darker, exactly (a CD stage's palette has them). `count` entries are
 * in use; the reserved ones follow them. */
static void reserve_shades(uint8_t palette6[FAM_PALETTE_BYTES], int count)
{
    for (int slot = 0; slot < SHADE_SLOTS && count < FAM_PIXEL_INDICES; slot++) {
        long worst = FAM_SHADE_BAR;
        int pick = -1;
        uint8_t shade[CHANNELS], pick_shade[CHANNELS];
        for (int i = 0; i < count; i++) {
            shade_of(palette6 + 3 * i, shade);
            long nearest = -1;
            for (int j = 0; j < count; j++) {
                const long d = distance_of(shade, palette6 + 3 * j);
                if (nearest < 0 || d < nearest) nearest = d;
            }
            if (nearest > worst) worst = nearest, pick = i, memcpy(pick_shade, shade, sizeof pick_shade);
        }
        if (pick < 0) return;
        memcpy(palette6 + 3 * count, pick_shade, CHANNELS);
        count++;
    }
}

int fam_palette_make(const FamPicture *pics, size_t n, uint8_t palette6[FAM_PALETTE_BYTES])
{
    memset(palette6, 0, FAM_PALETTE_BYTES);
    uint32_t *hist = histogram_of(pics, n);
    Box *boxes = malloc(sizeof *boxes * MAX_BOXES);
    if (hist == NULL || boxes == NULL) { free(hist); free(boxes); return 0; }
    const int count = median_cut_boxes(hist, boxes);
    for (int i = 0; i < count; i++) box_mean(hist, &boxes[i], palette6 + 3 * i);
    reserve_shades(palette6, count);
    free(boxes);
    free(hist);
    return 1;
}

uint8_t fam_palette_index(const uint8_t palette6[FAM_PALETTE_BYTES], uint8_t r, uint8_t g, uint8_t b)
{
    int best = 0;
    long best_d = -1;
    for (int i = 0; i < FAM_PIXEL_INDICES; i++) {
        const long dr = (long)(palette6[3 * i] << 2) - r, dg = (long)(palette6[3 * i + 1] << 2) - g, db = (long)(palette6[3 * i + 2] << 2) - b;
        const long d = dr * dr + dg * dg + db * db;
        if (best_d < 0 || d < best_d) best_d = d, best = i;
    }
    return (uint8_t)best;
}

uint8_t *fam_indices(const FamPicture *pic, const uint8_t palette6[FAM_PALETTE_BYTES])
{
    const size_t count = (size_t)pic->width * pic->height;
    uint8_t *out = malloc(count ? count : 1);
    if (out == NULL) return NULL;
    for (size_t i = 0; i < count; i++) {
        const uint8_t *px = pic->rgba + i * 4;
        out[i] = fam_palette_index(palette6, px[0], px[1], px[2]);
    }
    return out;
}

uint8_t *fam_resample(const FamPicture *src, uint32_t width, uint32_t height)
{
    uint8_t *out = malloc((size_t)width * height * 4);
    if (out == NULL) return NULL;
    for (uint32_t y = 0; y < height; y++) {
        uint32_t y0 = (uint32_t)((uint64_t)y * src->height / height), y1 = (uint32_t)((uint64_t)(y + 1) * src->height / height);
        if (y1 <= y0) y1 = y0 + 1;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t x0 = (uint32_t)((uint64_t)x * src->width / width), x1 = (uint32_t)((uint64_t)(x + 1) * src->width / width);
            if (x1 <= x0) x1 = x0 + 1;
            uint64_t sum[4] = { 0, 0, 0, 0 };
            for (uint32_t sy = y0; sy < y1; sy++)
                for (uint32_t sx = x0; sx < x1; sx++)
                    for (int c = 0; c < 4; c++) sum[c] += src->rgba[((size_t)sy * src->width + sx) * 4 + c];
            const uint64_t count = (uint64_t)(x1 - x0) * (y1 - y0);
            for (int c = 0; c < 4; c++) out[((size_t)y * width + x) * 4 + c] = (uint8_t)((sum[c] + count / 2) / count);
        }
    }
    return out;
}

/* A pure-C reader of Photoshop (.psd) files, for what the mask PSDs use: version 1, 8 bits per channel, RGB, layers stored raw or PackBits (RLE),
 * layer records with names (Pascal, or the unicode "luni" one when there is), bounds of their own and layer groups ("lsct"). Everything else is
 * refused in words (PSB, 16 or 32 bits, other colour modes, ZIP, no layers, truncated).
 *
 * The layers come in the file's order (the bottom one first). A group is two records round its layers: the divider (PSD_LAYER_DIVIDER) before them and the
 * folder (kind PSD_LAYER_OPEN_FOLDER or CLOSED_FOLDER, with the group's name) after; reading the records from the last to the first is the top-down
 * depth-first walk of the layer tree.
 * Only the layers whose name (trimmed, lower case) is one of `want` get their pixels; the others keep rgba NULL, so a big texture layer costs nothing.
 * A wanted layer with no area (nothing painted) has rgba NULL too. Pure: no globals, safe to call from any thread. */
#ifndef TNFS_PSD_READ_H
#define TNFS_PSD_READ_H

#include <stddef.h>
#include <stdint.h>

enum { PSD_LAYER_NORMAL = 0, PSD_LAYER_OPEN_FOLDER = 1, PSD_LAYER_CLOSED_FOLDER = 2, PSD_LAYER_DIVIDER = 3 };

typedef struct {
    char *name;       /* UTF-8, as the file has it (not trimmed) */
    int kind;         /* PSD_LAYER_* */
    int left, top, right, bottom;   /* the layer's own bounds on the canvas; they may lie outside it */
    uint8_t *rgba;    /* (right - left) x (bottom - top) pixels, straight alpha (255 when the layer has no alpha channel); NULL when not wanted or empty */
} PsdLayer;

typedef struct {
    uint32_t width, height;   /* the canvas */
    PsdLayer *layers;
    int count;
} PsdFile;

/* Reads `bytes` (a whole .psd) into `out`. 1 when it reads; 0 with `why` set (English, one line) otherwise, and `out` holds nothing to free. */
int psd_parse(const uint8_t *bytes, size_t len, const char *const *want, int want_count, PsdFile *out, char *why, size_t why_cap);

void psd_free(PsdFile *psd);

/* The first layer in top-down depth-first order (the file's last record first) that is not a group and whose trimmed, lower-case name is one of `names`;
 * NULL when there is none. */
const PsdLayer *psd_find_layer(const PsdFile *psd, const char *const *names, int name_count);

#endif

#include "formats/dash_png.h"

#include "formats/png.h"

#include <stdlib.h>

enum { MAX_SIDE = 4096 };

int dash_png_write(const uint8_t *classes, int width, int height, uint8_t **out, size_t *len)
{
    if (classes == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *indices = malloc(count);
    if (indices == NULL) return 0;
    for (size_t i = 0; i < count; i++)
        indices[i] = classes[i] == 255 ? 3 : classes[i] == 64 ? 1 : classes[i] == 128 ? 2 : classes[i] == 192 ? 4 : classes[i] == 160 ? 5 : 0;
    uint8_t palette[768] = { 0 };
    palette[5] = 255;                                  /* 1 blue */
    palette[7] = 255;                                  /* 2 green */
    palette[9] = palette[10] = palette[11] = 255;      /* 3 white */
    palette[12] = 255;                                 /* 4 red */
    palette[16] = palette[17] = 255;                   /* 5 cyan */
    const PngResult r = png_write_indexed(indices, (uint32_t)width, (uint32_t)height, palette, out, len);
    free(indices);
    return r == PNG_OK;
}

/* See added_gallery_setup.h. */
#include "game/frontend/added_gallery_setup.h"

#include "formats/fsh.h"
#include "formats/qfs.h"
#include "game/sys/palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BEVEL = 8, INTERIOR = 205 - 2 * BEVEL };   /* the slab's border left out: its 189 interior columns are the panel's ground */

/* the panel's column x from the slab's: its middle is the slab's interior, each side the interior's edge mirrored */
static int source_column(int x)
{
    const int pad = (GL_SETUP_W - INTERIOR) / 2;
    int c = x - pad;
    if (c < 0) c = -c - 1;
    if (c >= INTERIOR) c = 2 * INTERIOR - 1 - c;
    if (c < 0) c = 0;
    if (c >= INTERIOR) c = INTERIOR - 1;
    return BEVEL + c;
}

/* the entry's pixel (x, y) of a 205x262 bitmap: its row from the middle of the slab */
static int source_row(int y, int h) { return (h - GL_SETUP_H) / 2 + y; }

static const FshEntry *shape(const Fsh *fsh, const char tag[4], FshEntry *e)
{
    uint32_t index;
    if (fsh_find(fsh, tag, &index) != FSH_OK || fsh_entry(fsh, index, e) != FSH_OK || e->type != FSH_TYPE_BITMAP ||
        e->payload_size < (size_t)e->width * e->height || e->width < 205 || e->height < GL_SETUP_H)
        return NULL;
    return e;
}

int added_gallery_setup_art(SetupArt *art, const uint8_t *file, size_t size, const uint8_t pal[768], const uint8_t usable[256])
{
    memset(art, 0, sizeof *art);
    uint8_t *data = NULL;
    size_t data_size = size;
    if (qfs_is_compressed(file, size)) {
        if (qfs_decompressed_size(file, size, &data_size) != QFS_OK || (data = malloc(data_size)) == NULL ||
            qfs_decompress(file, size, data, data_size) != QFS_OK) {
            free(data);
            return 0;
        }
    }
    Fsh fsh;
    uint32_t index;
    FshEntry e, p;
    uint8_t own[768];
    int ok = fsh_open(&fsh, data != NULL ? data : file, data_size) == FSH_OK && fsh_find(&fsh, "!pal", &index) == FSH_OK &&
             fsh_entry(&fsh, index, &p) == FSH_OK && fsh_palette_rgb(&p, own) == FSH_OK && shape(&fsh, "opn1", &e) != NULL;
    if (ok) {
        for (int y = 0; y < GL_SETUP_H; y++)
            for (int x = 0; x < GL_SETUP_W; x++) {
                const uint8_t *c = own + 3 * e.payload[(size_t)source_row(y, e.height) * e.width + source_column(x)];
                art->slab[y * GL_SETUP_W + x] = (uint8_t)palette_nearest(pal, usable, c[0], c[1], c[2]);
                art->body[y * GL_SETUP_W + x] = (uint8_t)palette_nearest(pal, usable, c[0] * 45 / 100, c[1] * 45 / 100, c[2] * 45 / 100);
            }
    }
    for (int k = 0; ok && k < SETUP_FRAMES; k++) {
        char tag[5];
        snprintf(tag, sizeof tag, "opn%d", k + 1);
        FshEntry f;
        ok = shape(&fsh, tag, &f) != NULL;
        for (int y = 0; ok && y < GL_SETUP_H; y++)
            for (int x = 0; x < GL_SETUP_W; x++)
                art->stone[k][y * GL_SETUP_W + x] = f.payload[(size_t)source_row(y, f.height) * f.width + source_column(x)] != 255;   /* 255 is the hole */
    }
    free(data);
    art->ok = ok;
    return ok;
}

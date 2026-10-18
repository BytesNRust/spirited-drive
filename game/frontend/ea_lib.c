/* EA's picture and shape library over game/sys (ea_lib.h) */
#include "game/frontend/ea_lib.h"

#include "formats/fsh.h"
#include "game/gfx/flat.h"
#include "game/sys/palette.h"
#include "game/sys/resource.h"
#include "game/sys/shape_bank.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BANKS = 32, PICTURES = 16, ENTRY_HEADER = 16, TYPE_BITMAP = 0x7b };
/* pictures are numbered from here, banks from 1, so one free (00475260) can take either */
enum { PICTURE_BASE = 0x80 };
/* a picture's shape handle: this bit and its slot; real shapes are pointers into a bank */
enum { PICTURE_SHAPE = 0x40000000 };

static uint8_t *banks[BANKS];
static uint8_t *pictures[PICTURES];  /* each a raw shape: the 16-byte header, then w x h pixels */
static int32_t screen;
static int32_t selected;  /* the picture drawing goes to, 0 the back buffer */

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static uint8_t *bank_of(int32_t bank) { return bank >= 1 && bank <= BANKS ? banks[bank - 1] : NULL; }
static uint8_t *picture_of(int32_t picture)
{
    return picture > PICTURE_BASE && picture <= PICTURE_BASE + PICTURES ? pictures[picture - PICTURE_BASE - 1] : NULL;
}

int32_t ea_bank_load(const char *name)
{
    for (int i = 0; i < BANKS; i++) {
        if (banks[i] != NULL) continue;
        size_t size;
        banks[i] = resource_load_art(name, &size);
        if (banks[i] == NULL) fprintf(stderr, "ea_bank_load: cannot read %s\n", name);
        return banks[i] != NULL ? i + 1 : 0;
    }
    fprintf(stderr, "ea_bank_load: no room for %s\n", name);
    return 0;
}

void ea_bank_free(int32_t bank)
{
    uint8_t *const b = bank_of(bank);
    if (b == NULL) return;
    resource_free(b);
    banks[bank - 1] = NULL;
}

const uint8_t *ea_shape(int32_t bank, const char *tag)
{
    uint8_t *const b = bank_of(bank);
    const uint8_t *const s = b != NULL ? shape_bank_find(b, (const uint8_t *)tag) : NULL;
    if (s == NULL) fprintf(stderr, "ea_shape: no %.4s\n", tag);
    return s;
}

int32_t ea_picture_new(int32_t w, int32_t h)
{
    if (w <= 0 || h <= 0) return 0;
    for (int i = 0; i < PICTURES; i++) {
        if (pictures[i] != NULL) continue;
        uint8_t *const p = calloc(ENTRY_HEADER + (size_t)w * (size_t)h, 1);
        if (p == NULL) return 0;
        p[0] = TYPE_BITMAP;
        p[4] = (uint8_t)w, p[5] = (uint8_t)(w >> 8), p[6] = (uint8_t)h, p[7] = (uint8_t)(h >> 8);
        pictures[i] = p;
        return PICTURE_BASE + i + 1;
    }
    fprintf(stderr, "ea_picture_new: no room\n");
    return 0;
}

void ea_picture_free(int32_t picture)
{
    uint8_t *const p = picture_of(picture);
    if (p == NULL) return;
    if (picture == screen) screen = 0;
    if (picture == selected) ea_select_default();
    free(p);
    pictures[picture - PICTURE_BASE - 1] = NULL;
}

void ea_free(int32_t handle)
{
    if (handle > PICTURE_BASE) ea_picture_free(handle);
    else ea_bank_free(handle);
}

int32_t ea_picture_shape(int32_t picture)
{
    return picture_of(picture) != NULL ? PICTURE_SHAPE | picture : 0;
}

int32_t ea_screen(void)
{
    return *ea_screen_slot();
}

int32_t *ea_screen_slot(void)
{
    if (screen == 0) screen = ea_picture_new(VIDEO_WIDTH, VIDEO_HEIGHT);
    return &screen;
}

void ea_select(int32_t picture)
{
    uint8_t *const p = picture_of(picture);
    if (p == NULL) {
        ea_select_default();
        return;
    }
    video_select_target(p + ENTRY_HEADER, le16(p + 4), le16(p + 6));
    selected = picture;
}

void ea_select_default(void)
{
    video_select_target(NULL, 0, 0);
    selected = 0;
}

/* a raw shape (a bank's entry or a picture's) as an FshEntry; 0 when it is none */
static int entry_of(const uint8_t *shape, FshEntry *e)
{
    const uintptr_t v = (uintptr_t)shape;
    if ((v & ~(uintptr_t)0xff) == PICTURE_SHAPE) shape = picture_of((int32_t)(v & 0xff));
    if (shape == NULL) return 0;
    memset(e, 0, sizeof *e);
    e->type = shape[0];
    e->width = le16(shape + 4);
    e->height = le16(shape + 6);
    for (int i = 0; i < 4; i++) e->misc[i] = (int16_t)le16(shape + 8 + 2 * i);
    e->payload = shape + ENTRY_HEADER;
    e->payload_size = (size_t)e->width * e->height;
    return e->type == TYPE_BITMAP;
}

void ea_blit_own(const uint8_t *shape)
{
    FshEntry e;
    if (entry_of(shape, &e)) video_blit_at_own_position(&e);
}

void ea_blit_masked(const uint8_t *shape)
{
    FshEntry e;
    if (entry_of(shape, &e)) video_blit_transparent_at_own_position(&e);
}

void ea_restore(int32_t shape, int32_t x, int32_t y)
{
    FshEntry e;
    if (entry_of((const uint8_t *)(intptr_t)shape, &e)) video_blit(&e, x, y);
}

void ea_blit_at(const uint8_t *shape, int32_t x, int32_t y)
{
    FshEntry e;
    if (entry_of(shape, &e)) video_blit(&e, x, y);
}

void ea_save(int32_t shape, int32_t x, int32_t y)
{
    FshEntry e;
    if (!entry_of((const uint8_t *)(intptr_t)shape, &e)) return;
    video_read_rect(x, y, e.width, e.height, (uint8_t *)(uintptr_t)e.payload);
}

static uint8_t *shade;  /* 004826f0's table */
static int32_t clip_x0, clip_y0, clip_x1 = VIDEO_WIDTH, clip_y1 = VIDEO_HEIGHT;

/* the colours a menu may draw with: all but 0xfe and 0xff (the pointer's and the see-through one) */
static void usable_colours(uint8_t usable[256])
{
    memset(usable, 1, 256);
    usable[254] = usable[255] = 0;
}

void ea_shade_table(int32_t r, const uint8_t *pal, uint8_t *table)
{
    uint8_t usable[256];
    usable_colours(usable);
    palette_shade_table(pal, usable, r, table);
}

void ea_shade_set(uint8_t *table)
{
    shade = table;
}

/* the drawing's target, clipped to 0047723c's box */
static QuadTarget quad_target(void)
{
    int w, h;
    uint8_t *const p = video_target(&w, &h);
    return (QuadTarget){ p, w, clip_x0 > 0 ? clip_x0 : 0, clip_y0 > 0 ? clip_y0 : 0, clip_x1 < w ? clip_x1 : w,
                         clip_y1 < h ? clip_y1 : h };
}

void ea_shade_polygon(int32_t count, const int32_t *xy)
{
    if (shade == NULL) return;
    const QuadTarget t = quad_target();
    flat_polygon_shade(&t, shade, count, xy, 0);
}

int32_t ea_colour(int32_t rgb)
{
    uint8_t pal[PALETTE_BYTES], usable[256];
    video_get_palette(pal);
    usable_colours(usable);
    return palette_nearest(pal, usable, rgb >> 16 & 0xff, rgb >> 8 & 0xff, rgb & 0xff);
}

void ea_frame(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t colour)
{
    const QuadTarget t = quad_target();
    flat_frame_rect(&t, x0, y0, x1, y1, (uint32_t)colour);
}

/* TODO(0047723c): the blits (video.c) do not keep to the box yet, only the polygons and frames */
void ea_clip(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    clip_x0 = x0, clip_y0 = y0, clip_x1 = x1, clip_y1 = y1;
}

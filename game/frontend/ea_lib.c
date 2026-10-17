/* EA's picture and shape library over game/sys (ea_lib.h) */
#include "game/frontend/ea_lib.h"

#include "formats/fsh.h"
#include "game/sys/resource.h"
#include "game/sys/shape_bank.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BANKS = 32, PICTURES = 16, ENTRY_HEADER = 16, TYPE_BITMAP = 0x7b };
/* a picture's shape handle: this bit and its slot; real shapes are pointers into a bank */
enum { PICTURE_SHAPE = 0x40000000 };

static uint8_t *banks[BANKS];
static uint8_t *pictures[PICTURES];  /* each a raw shape: the 16-byte header, then w x h pixels */
static int32_t screen;

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static uint8_t *bank_of(int32_t bank) { return bank >= 1 && bank <= BANKS ? banks[bank - 1] : NULL; }
static uint8_t *picture_of(int32_t picture) { return picture >= 1 && picture <= PICTURES ? pictures[picture - 1] : NULL; }

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
        return i + 1;
    }
    fprintf(stderr, "ea_picture_new: no room\n");
    return 0;
}

void ea_picture_free(int32_t picture)
{
    uint8_t *const p = picture_of(picture);
    if (p == NULL) return;
    if (picture == screen) screen = 0;
    free(p);
    pictures[picture - 1] = NULL;
}

int32_t ea_picture_shape(int32_t picture)
{
    return picture_of(picture) != NULL ? PICTURE_SHAPE | picture : 0;
}

int32_t ea_screen(void)
{
    if (screen == 0) screen = ea_picture_new(VIDEO_WIDTH, VIDEO_HEIGHT);
    return screen;
}

void ea_select(int32_t picture)
{
    uint8_t *const p = picture_of(picture);
    if (p == NULL) {
        ea_select_default();
        return;
    }
    video_select_target(p + ENTRY_HEADER, le16(p + 4), le16(p + 6));
}

void ea_select_default(void)
{
    video_select_target(NULL, 0, 0);
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

void ea_save(int32_t shape, int32_t x, int32_t y)
{
    FshEntry e;
    if (!entry_of((const uint8_t *)(intptr_t)shape, &e)) return;
    video_read_rect(x, y, e.width, e.height, (uint8_t *)(uintptr_t)e.payload);
}

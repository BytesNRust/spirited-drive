/* The loading pictures (drive_slide.h) */
#include "game/sys/port_log.h"
#include "game/viewer/drive_slide.h"

#include "game/sys/resource.h"
#include "game/sys/video.h"

#include <stdio.h>

enum { SLIDE_CARS = 12, ENTRY_HEADER = 0x10, PALETTE_BYTES = 768 };

/* 004c5660 */
static const char *const SLIDE_CAR[SLIDE_CARS] = { "TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER", "ANSX",
                                                   "MRX7", "MRX7", "MRX7", "MRX7", "Traffc" };

void drive_slide_name(char *out, size_t size, const char *dir, int32_t car, int32_t picture)
{
    if (car < 0 || car >= SLIDE_CARS) {  /* port guard: the original reads past its table */
        if (size > 0) out[0] = '\0';
        return;
    }
    snprintf(out, size, "%s%s%s%d.qfs", dir, SLIDE_CAR[car], "s", (int)picture);
}

int drive_slide_parts(const uint8_t *data, size_t size, FshEntry *shape, const uint8_t **rgb)
{
    Fsh fsh;
    uint32_t index;
    if (fsh_open(&fsh, data, size) != FSH_OK || fsh_find(&fsh, "SCRN", &index) != FSH_OK
        || fsh_entry(&fsh, index, shape) != FSH_OK || shape->type != FSH_TYPE_BITMAP)
        return 0;
    const uint8_t *start = shape->payload - ENTRY_HEADER;
    const size_t at = (size_t)(start - data) + shape->block_size + ENTRY_HEADER;
    if (shape->block_size == 0 || at > size || size - at < PALETTE_BYTES) return 0;  /* port guard */
    *rgb = data + at;
    return 1;
}

void drive_slide_palette(const uint8_t *six, uint8_t rgb[768])
{
    for (int i = 0; i < PALETTE_BYTES; i++) rgb[i] = (uint8_t)(six[i] << 2 | six[i] >> 4);
}

void drive_slide_show(int32_t car, int32_t picture)
{
    char name[512];
    drive_slide_name(name, sizeof name, resource_dir(PATH_SLIDES), car, picture);
    size_t size = 0;
    uint8_t *data = name[0] != '\0' ? resource_load(name, &size) : NULL;
    FshEntry shape;
    const uint8_t *rgb;
    if (data == NULL || !drive_slide_parts(data, size, &shape, &rgb)) {
        port_log("drive_slide: cannot show %s\n", name);
        resource_free(data);
        return;
    }
    uint8_t colours[PALETTE_BYTES];
    drive_slide_palette(rgb, colours);
    video_set_palette(0, 256, colours);
    video_blit_at_own_position(&shape);
    video_flip();
    resource_free(data);
}

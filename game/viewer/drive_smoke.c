/* --drive's tyre smoke (drive_smoke.h). */
#include "viewer/drive_smoke.h"

#include "game/gfx/palette_match.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void drive_smoke_setup(DriveSmoke *s, const char *data_root, CrashTrail *trail)
{
    memset(s, 0, sizeof *s);
    snprintf(s->misc_dir, sizeof s->misc_dir, "SIMDATA/MISC/");
    snprintf(s->root, sizeof s->root, "%s", data_root);
    s->trail = trail;
}

/* 00476268, with the name upper case when the mixed-case one is missing (the CD's files are). */
static uint8_t *load(void *ctx, const char *path)
{
    const DriveSmoke *s = ctx;
    size_t size = 0;
    char full[sizeof s->root + 0x50];  /* the root, then 00456a7c's 80 bytes */
    snprintf(full, sizeof full, "%s/%s", s->root, path);
    uint8_t *file = resource_load(full, &size);
    if (file != NULL) return file;
    for (size_t i = strlen(s->root) + 1 + strlen(s->misc_dir); full[i] != '\0'; i++) full[i] = (char)toupper((unsigned char)full[i]);
    return resource_load(full, &size);
}

/* 00473140 on the race's screen palette */
static uint8_t nearest(void *ctx, uint32_t rgb)
{
    const RacePalette *p = ctx;
    const PaletteMatch m = { p->inverse, p->screen, p->enabled };
    return (uint8_t)palette_match_rgb(&m, rgb);
}

void drive_smoke_palette(void *ctx, RacePalette *p)
{
    DriveSmoke *s = ctx;
    puff_art_start(&s->art, s->trail, p, s->misc_dir, load, s);
    if (s->art.bank == NULL || s->art.palette == NULL) {
        port_log("drive_smoke: no %s/%ssmoke.qfs\n", s->root, s->misc_dir);
        return;
    }
    s->ready = 1;
    for (int k = 0; k < PUFF_SHAPES; k++)
        if (s->art.smoke[k] == NULL || s->art.dirt[k] == NULL) s->ready = 0;
}

void drive_smoke_blend(void *ctx, RacePalette *p)
{
    DriveSmoke *s = ctx;
    if (s->art.bank != NULL && s->art.palette != NULL) puff_blend_init(&s->blend, s->table, p, nearest, p);
}

void drive_smoke_free(DriveSmoke *s)
{
    resource_free(s->art.bank);
    memset(&s->art, 0, sizeof s->art);
    s->ready = 0;
}

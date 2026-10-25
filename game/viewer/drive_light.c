#include "viewer/drive_light.h"
#include "viewer/ported_view.h"
#include "game/platform/platform.h"
#include "game/race/psx_light.h"
#include "game/sys/port_log.h"

#include <ctype.h>
#include <stdio.h>

enum { LIGHT_PATH = 320, LIGHT_TRACK = 16, LIGHT_DAY = 'B' };

static PsxLight light;

void drive_light_load(int enabled, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    ported_view_set_light(NULL);
    psx_light_free(&light);
    if (!enabled || psx_dir == NULL || psx_dir[0] == '\0') return;
    char upper[LIGHT_TRACK], path[LIGHT_PATH];
    size_t i = 0;
    for (; track[i] != '\0' && i < LIGHT_TRACK - 1; i++) upper[i] = (char)toupper((unsigned char)track[i]);
    upper[i] = '\0';
    snprintf(path, sizeof path, "%s/Z%s%c.LGT", psx_dir, upper, LIGHT_DAY);
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("psx_light: no %s\n", path);
        return;
    }
    PsxLight parsed = { 0 };
    const int ok = psx_light_parse(file, size, ring_nodes, &parsed);
    platform_free_file(file);
    if (!ok) {
        port_log("psx_light: %s does not fit the track (%u nodes)\n", path, ring_nodes);
        return;
    }
    light = parsed;
    ported_view_set_light(&light);
    port_log("psx_light: %s, %u nodes\n", path, light.nodes);
}

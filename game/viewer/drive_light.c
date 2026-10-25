#include "viewer/drive_light.h"
#include "viewer/ported_view.h"
#include "game/platform/platform.h"
#include "game/race/psx_lgs.h"
#include "game/race/psx_light.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"

#include <ctype.h>
#include <stdio.h>

enum { LIGHT_PATH = 320, LIGHT_TRACK = 16 };

static const char LETTERS[] = { 'B', 'A', 'C' };   /* by PSX_TIME_* */

static PsxLight light;
static float tint[3] = { 1, 1, 1 };

/* the files' path: <psx_dir>/Z<TRACK><letter>.<ext> */
static void file_path(char *out, size_t size, const char *psx_dir, const char *track, char letter, const char *ext)
{
    char upper[LIGHT_TRACK];
    size_t i = 0;
    for (; track[i] != '\0' && i < LIGHT_TRACK - 1; i++) upper[i] = (char)toupper((unsigned char)track[i]);
    upper[i] = '\0';
    snprintf(out, size, "%s/Z%s%c.%s", psx_dir, upper, letter, ext);
}

/* the LGS's ambient colour as a tint, neutral when the file is missing or does not read */
static void load_tint(const char *psx_dir, const char *track, char letter)
{
    tint[0] = tint[1] = tint[2] = 1.0f;
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGS");
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("psx_light: no %s, the ambient stays neutral\n", path);
        return;
    }
    PsxLgs lgs;
    if (psx_lgs_parse(file, size, &lgs)) psx_lgs_tint(&lgs, tint);
    else port_log("psx_light: %s does not read, the ambient stays neutral\n", path);
    platform_free_file(file);
}

void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    ported_view_set_light(NULL, NULL);
    psx_light_free(&light);
    if (!enabled || psx_dir == NULL || psx_dir[0] == '\0') return;
    const char letter = LETTERS[time >= PSX_TIME_DAY && time <= PSX_TIME_C ? time : PSX_TIME_DAY];
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGT");
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
    load_tint(psx_dir, track, letter);
    ported_view_set_light(&light, tint);
    port_log("psx_light: %s, %u nodes, tint %.2f %.2f %.2f\n", path, light.nodes, tint[0], tint[1], tint[2]);
}

#include "game/race/car_mask.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include "formats/png.h"
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MASK_MAX_SIDE = 2048 };   /* a mask is its texture's size or near it: a larger header is never decoded */

/* the mask at `path` for a width x height texture; NULL when it does not read (said) */
static uint8_t *mask_read(const char *path, int width, int height)
{
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) return NULL;
    uint32_t w = 0, h = 0;
    const int sized = png_size(file, size, &w, &h) == PNG_OK && w > 0 && h > 0 && w <= MASK_MAX_SIDE && h <= MASK_MAX_SIDE;
    uint8_t *mask = sized ? malloc((size_t)width * (size_t)height) : NULL;
    const int ok = mask != NULL && car_mask_parse(file, size, width, height, mask);
    platform_free_file(file);
    if (ok) return mask;
    port_log("car mask: cannot read %s: not a %dx%d mask PNG\n", path, width, height);
    free(mask);
    return NULL;
}

uint8_t *car_mask_load_dir(const char *dir, const char name[4], int level, int width, int height)
{
    if (dir == NULL || dir[0] == '\0' || width <= 0 || height <= 0) return NULL;
    for (int i = 0; i < 4; i++)   /* the texture's name names the file: letters and digits alone */
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= '0' && name[i] <= '9'))) return NULL;
    char path[DATA_PATH_MAX];
    snprintf(path, sizeof path, "%s/%.4s%s_mask.png", dir, name, level != 0 ? "_lo" : "");
    if (!data_path_exists(path)) return NULL;   /* masks are optional */
    uint8_t *const mask = mask_read(path, width, height);
    if (mask != NULL) port_log_detail("car mask: %s\n", path);
    return mask;
}

uint8_t *car_mask_load(const char *stem, const char name[4], int level, int width, int height)
{
    if (stem == NULL || width <= 0 || height <= 0) return NULL;
    const char *const suffix = level != 0 ? "_lo" : "";   /* the low detail bank's files are <tex>_lo_mask.png */
    const char *env = getenv("TNFS_CAR_MASKS");
    char file_name[256], found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    snprintf(file_name, sizeof file_name, "car_masks/%s/%.4s%s_mask.png", stem, name, suffix);
    const int dev = env != NULL && env[0] != '\0';   /* a dev override first: TNFS_CAR_MASKS=<dir of <car>/<tex>_mask.png> */
    (void)data_path_candidates(file_name, found);     /* then the program's own data (the bundle's), the working directory, a development build's source tree */
    for (int r = 0; r < 1 + DATA_PATH_CANDIDATES; r++) {
        char path[DATA_PATH_MAX];
        if (r == 0) {
            if (!dev) continue;
            snprintf(path, sizeof path, "%s/%s/%.4s%s_mask.png", env, stem, name, suffix);
        } else {
            if (found[r - 1][0] == '\0') continue;
            snprintf(path, sizeof path, "%s", found[r - 1]);
        }
        if (r > 0 && !data_path_exists(path)) continue;   /* masks are optional: a data folder without this one is passed over, no "cannot read" line (i191); the dev override is read as named */
        uint8_t *const mask = mask_read(path, width, height);   /* there, but not a mask: said, then the next folder */
        if (mask == NULL) continue;
        if (r <= 1) port_log("car mask: %s\n", path);   /* a dev override's or a mod's: what a bug report needs */
        else port_log_detail("car mask: %s\n", path);   /* the port's own data (the bundle's, the working directory, ...): a detail */
        return mask;
    }
    port_log_detail("car mask: none for %s/%.4s%s\n", stem, name, suffix);
    return NULL;
}

#include "game/sys/resource.h"

#include "formats/qfs.h"
#include "platform/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PATH_MAX_LEN = 512 };

static char dirs[PATH_COUNT][PATH_MAX_LEN];
static char root[PATH_MAX_LEN];
static int is_german;

int resource_german(void)
{
    return is_german;
}

void resource_init(const char *data_root, int german)
{
    static const char *const defaults[PATH_COUNT][2] = {
        [PATH_ART] = { "frontend/art/", "frontend/gart/" },
        [PATH_MOVIE] = { "frontend/movie/", "frontend/movie/" },
        [PATH_MUSIC] = { "frontend/music/", "frontend/music/" },
        [PATH_REPLAY] = { "gamedata/replay/", "gamedata/replay/" },
        [PATH_CONFIG] = { "gamedata/config/", "gamedata/config/" },
    };
    is_german = german;
    snprintf(root, sizeof root, "%s", data_root);
    for (int i = 0; i < PATH_COUNT; i++) {
        snprintf(dirs[i], sizeof dirs[i], "%s/%s", data_root, defaults[i][german ? 1 : 0]);
    }
}

const char *resource_root(void)
{
    return root;
}

const char *resource_dir(GamePath which)
{
    return dirs[which];
}

/* The CD is ISO 9660 (upper case) while the game asks for lower-case names;
 * case-sensitive file systems need the upper-cased fallback. */
static uint8_t *read_any_case(const char *path, size_t *size)
{
    uint8_t *data = platform_read_file(path, size);
    if (data != NULL) return data;
    char upper[PATH_MAX_LEN];
    snprintf(upper, sizeof upper, "%s", path);
    for (char *c = upper; *c; c++) *c = (char)toupper((unsigned char)*c);
    return platform_read_file(upper, size);
}

uint8_t *resource_load(const char *path, size_t *size)
{
    size_t raw_size;
    uint8_t *raw = read_any_case(path, &raw_size);
    if (raw == NULL) return NULL;

    uint8_t *out = malloc(raw_size ? raw_size : 1);
    QfsResult r = QFS_OK;
    if (out != NULL && qfs_is_compressed(raw, raw_size)) {
        free(out);
        r = qfs_decompressed_size(raw, raw_size, size);
        out = r == QFS_OK ? malloc(*size ? *size : 1) : NULL;
        if (out != NULL && (r = qfs_decompress(raw, raw_size, out, *size)) != QFS_OK) {
            free(out);
            out = NULL;
        }
    } else if (out != NULL) {
        memcpy(out, raw, raw_size);
        *size = raw_size;
    }
    platform_free_file(raw);
    if (out == NULL) fprintf(stderr, "resource: %s: %s\n", path, r != QFS_OK ? qfs_result_str(r) : "out of memory");
    return out;
}

uint8_t *resource_load_art(const char *name, size_t *size)
{
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof path, "%s%s", dirs[PATH_ART], name);
    return resource_load(path, size);
}

void resource_free(uint8_t *data)
{
    free(data);
}

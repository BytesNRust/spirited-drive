/* the install check of a mod's zip (dep59): names, kinds and every file's content through the game's readers; on a pass, every entry it
 * accepted is one of the zip's, with a path that stays below the mod's folder */
#include "fuzz/fuzz.h"
#include "formats/mod_check.h"

#include <string.h>

/* whether `path` has a "." or ".." component, or an empty one */
static int odd_component(const char *path)
{
    for (const char *p = path;;) {
        const char *end = strchr(p, '/');
        const size_t n = end != NULL ? (size_t)(end - p) : strlen(p);
        if (n == 0 || (n == 1 && p[0] == '.') || (n == 2 && p[0] == '.' && p[1] == '.')) return 1;
        if (end == NULL) return 0;
        p = end + 1;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ModCheck c;
    char why[256];
    if (!mod_check_zip(data, size, &c, why, sizeof why)) {
        fuzz_touch((const uint8_t *)why, strlen(why));
        return 0;
    }
    ZipArchive z;
    if (zip_open(data, size, &z) != ZIP_OK) __builtin_trap();   /* the check opened it: it opens again */
    for (size_t i = 0; i < c.entry_count; i++) {
        const ModEntry *e = &c.entries[i];
        if (e->index >= z.count || e->path[0] == '/' || odd_component(e->path)) __builtin_trap();
        fuzz_touch((const uint8_t *)e->path, strlen(e->path));
    }
    zip_close(&z);
    mod_check_free(&c);
    return 0;
}

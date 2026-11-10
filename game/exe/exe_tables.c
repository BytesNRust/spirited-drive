#include "game/exe/exe_tables.h"

#include "game/sys/data_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*Extractor)(const ExeImage *img, ExeTables *t, char *err, size_t errsz);

static const struct { const char *name; Extractor run; } FAMILIES[] = {
    { "ai", exe_tables_ai }, { "split", exe_tables_split }, { "view", exe_tables_view }, { "track", exe_tables_track },
    { "menu", exe_tables_menu }, { "config", exe_tables_config },
};

int exe_tables_extract(const ExeImage *img, ExeTables *out, char *err, size_t errsz)
{
    ExeTables *t = calloc(1, sizeof *t);
    if (!t) {
        if (err && errsz) snprintf(err, errsz, "out of memory reading the exe's tables");
        return -1;
    }
    for (size_t i = 0; i < sizeof FAMILIES / sizeof FAMILIES[0]; i++) {
        char why[256] = "";
        if (FAMILIES[i].run(img, t, why, sizeof why) != 0) {
            if (err && errsz) snprintf(err, errsz, "NFS_WIN.EXE: the %s tables: %s", FAMILIES[i].name, why);
            free(t);
            return -1;
        }
    }
    *out = *t;
    free(t);
    return 0;
}

int exe_tables_load(const uint8_t *exe, size_t len, const char *where, ExeTables *out, char *err, size_t errsz)
{
    ExeImage img;
    if (exe_image_verify(exe, len, where, err, errsz) != 0) return -1;
    if (exe_image_open(&img, exe, len, err, errsz) != 0) return -1;
    return exe_tables_extract(&img, out, err, errsz);
}

int exe_tables_load_dir(const char *data_root, ExeTables *out, char *err, size_t errsz)
{
    char path[1024];
    if (!data_root || !data_find_file(data_root, "NFS_WIN.EXE", path, sizeof path)) {
        if (err && errsz)
            snprintf(err, errsz, "NFS_WIN.EXE not found in %s. The port reads its tables from the PC Special Edition's own file: "
                     "choose the folder of that CD.", data_root ? data_root : "the data folder");
        return -1;
    }
    FILE *f = fopen(path, "rb");
    long n = -1;
    if (f && fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (!f || n <= 0 || fseek(f, 0, SEEK_SET) != 0) {
        if (f) fclose(f);
        if (err && errsz) snprintf(err, errsz, "cannot read %s", path);
        return -1;
    }
    uint8_t *buf = malloc((size_t)n);
    size_t got = buf ? fread(buf, 1, (size_t)n, f) : 0;
    fclose(f);
    if (!buf || got != (size_t)n) {
        free(buf);
        if (err && errsz) snprintf(err, errsz, "cannot read %s", path);
        return -1;
    }
    int rc = exe_tables_load(buf, (size_t)n, data_root, out, err, errsz);
    free(buf);
    return rc;
}

int exe_tables_install(const char *data_root, char *err, size_t errsz)
{
    ExeTables *t = calloc(1, sizeof *t);
    if (!t) {
        if (err && errsz) snprintf(err, errsz, "out of memory reading the exe's tables");
        return -1;
    }
    int rc = 0;
#ifdef TNFS_SOURCE_DIR
    const char *const poison = getenv("TNFS_EXE_TABLES_POISON");
    if (poison && poison[0] && poison[0] != '0') {
        exe_tables_publish(t);
        free(t);
        return 0;
    }
#endif
    rc = exe_tables_load_dir(data_root, t, err, errsz);
    if (rc == 0) exe_tables_publish(t);
    free(t);
    return rc;
}

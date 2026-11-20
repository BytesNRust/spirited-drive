/* tnfs-studio pack, shared by the command line and the editor (studio/pack_tree.h). */
#include "studio/pack_tree.h"

#include "formats/resource_exchange.h"
#include "studio/fsio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *studio_base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* "path: message", unless the message already starts with the file's name. */
void studio_name_the_file(ExchangeError *err, const char *path)
{
    char tmp[sizeof err->message];
    snprintf(tmp, sizeof tmp, "%s", err->message);
    if (strncmp(tmp, studio_base_name(path), strlen(studio_base_name(path))) == 0) exchange_fail(err, "%s: %s", path, tmp + strlen(studio_base_name(path)) + (tmp[strlen(studio_base_name(path))] == ':' ? 2 : 0));
    else exchange_fail(err, "%s: %s", path, tmp);
}

int studio_pack(const char *tree, const char *out_path, const char *from_path, const uint8_t *palette, int force, ExchangeError *err)
{
    if (from_path == NULL) { exchange_fail(err, "pack needs --from ORIGINAL (the file the tree was unpacked from)"); return 0; }
    size_t size = 0;
    uint8_t *original = fsio_read_file(from_path, &size, err);
    if (original == NULL) return 0;
    FileSet set;
    fileset_init(&set);
    uint8_t *out = NULL;
    size_t out_len = 0;
    ImportStats stats;
    int ok = fsio_read_tree(tree, &set, err);
    if (ok) {
        ok = resource_import(original, size, studio_base_name(from_path), palette, &set, &out, &out_len, &stats, err);
        if (!ok) studio_name_the_file(err, from_path);
    }
    if (ok) ok = fsio_write_file(out_path, out, out_len, force, err);
    if (ok) {
        printf("%s: %zu bytes written", out_path, out_len);
        if (stats.pixels) printf("; %u pixels, %u reassigned to the palette", stats.pixels, stats.reassigned);
        printf("\n");
    }
    free(out);
    fileset_free(&set);
    free(original);
    return ok;
}

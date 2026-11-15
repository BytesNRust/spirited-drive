#include "studio/mod_pack.h"

#include "formats/mod_check.h"
#include "formats/validate.h"
#include "formats/zip.h"
#include "studio/fsio.h"
#include "studio/mod_validate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USAGE "usage: tnfs-studio mod pack DIR OUT.zip [--force]"

typedef struct {
    const char *dir, *out;
    int force;
} Args;

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (argv[i][0] == '-' && argv[i][1] == '-') { exchange_fail(err, "unknown option '%s'; %s", argv[i], USAGE); return 0; }
        else if (a->dir == NULL) a->dir = argv[i];
        else if (a->out == NULL) a->out = argv[i];
        else { exchange_fail(err, "too many arguments; %s", USAGE); return 0; }
    }
    if (a->dir == NULL || a->out == NULL) { exchange_fail(err, USAGE); return 0; }
    return 1;
}

/* mod.ini first, then by name: the order the zip is written in */
static int pack_order(const void *a, const void *b)
{
    const char *x = ((const SetFile *)a)->name, *y = ((const SetFile *)b)->name;
    const int xi = strcmp(x, "mod.ini") == 0, yi = strcmp(y, "mod.ini") == 0;
    return xi != yi ? yi - xi : strcmp(x, y);
}

/* the folder passes validate; its findings are printed when there are any */
static int checked(const FileSet *set, const char *dir, ExchangeError *err)
{
    if (fileset_find(set, "mod.ini") == NULL) { exchange_fail(err, "%s has no mod.ini", dir); return 0; }
    Report r;
    report_init(&r);
    int ok = mod_validate_tree(set, &r, err);
    const size_t fatal = report_fatal_count(&r);
    if (ok && r.count > 0) report_print(&r, VALIDATE_COMPATIBLE, stdout);
    report_free(&r);
    if (ok && fatal > 0) exchange_fail(err, "%s does not pass validate (%zu errors): nothing written", dir, fatal), ok = 0;
    return ok;
}

/* what goes into the zip: the files the installation takes, the litter left out; 0 naming the first it would refuse */
static int packed_files(const FileSet *set, FileSet *out, ExchangeError *err)
{
    for (size_t i = 0; i < set->count; i++) {
        const ModFileKind kind = mod_file_kind(set->file[i].name);
        if (kind == MOD_KIND_SKIP) continue;
        if (kind == MOD_KIND_NONE) { exchange_fail(err, "%s is nothing the game installs: nothing written", set->file[i].name); return 0; }
        if (!fileset_add(out, set->file[i].name, set->file[i].data, set->file[i].size)) { exchange_fail(err, "out of memory"); return 0; }
    }
    qsort(out->file, out->count, sizeof *out->file, pack_order);
    return 1;
}

static int mod_pack(const Args *a, ExchangeError *err)
{
    if (!a->force && fsio_exists(a->out)) { exchange_fail(err, "%s exists (use --force to replace it)", a->out); return 0; }
    FileSet set, files;
    fileset_init(&set);
    fileset_init(&files);
    uint8_t *zip = NULL;
    size_t size = 0;
    int ok = fsio_read_tree(a->dir, &set, err) && checked(&set, a->dir, err) && packed_files(&set, &files, err);
    if (ok && (zip = zip_write_stored(&files, &size)) == NULL) exchange_fail(err, "the files do not fit a zip the game reads"), ok = 0;
    if (ok) {
        ModCheck c;
        char why[256];
        ok = mod_check_zip(zip, size, &c, why, sizeof why);
        if (ok) mod_check_free(&c);
        else exchange_fail(err, "the game would not install the zip: %s; nothing written", why);
    }
    if (ok) ok = fsio_write_atomic(a->out, zip, size, err);
    if (ok) printf("mod pack: %s, %zu files, %zu bytes\n", a->out, files.count, size);
    free(zip);
    fileset_free(&files);
    fileset_free(&set);
    return ok;
}

int mod_command(int argc, char **argv, ExchangeError *err)
{
    if (argc < 2 || strcmp(argv[1], "pack") != 0) { exchange_fail(err, USAGE); return 0; }
    Args a;
    return parse(argc, argv, &a, err) && mod_pack(&a, err);
}

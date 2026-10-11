#include "studio/track_new.h"

#include "formats/slots.h"
#include "formats/track_gen.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *layout, *slot, *out, *stem, *data;
    int force;
} Args;

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    a->data = "original";
    for (int i = 2; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--template") == 0 && has_value) a->slot = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && has_value) a->out = argv[++i];
        else if (strcmp(argv[i], "--stem") == 0 && has_value) a->stem = argv[++i];
        else if (strcmp(argv[i], "--data") == 0 && has_value) a->data = argv[++i];
        else if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (argv[i][0] == '-' && argv[i][1] == '-') { exchange_fail(err, "unknown option '%s'", argv[i]); return 0; }
        else if (a->layout == NULL) a->layout = argv[i];
        else { exchange_fail(err, "too many arguments"); return 0; }
    }
    if (a->layout == NULL || a->slot == NULL || a->out == NULL) {
        exchange_fail(err, "usage: tnfs-studio track new LAYOUT --template SLOT --out DIR [--stem NAME] [--data DIR] [--force]");
        return 0;
    }
    return 1;
}

static int stem_ok(const char *stem)
{
    const size_t n = strlen(stem);
    if (n == 0 || n > 8) return 0;
    for (size_t i = 0; i < n; ++i) if (!isalnum((unsigned char)stem[i])) return 0;
    return 1;
}

/* Adds `real` (a path in the data root) to the set as `SIMDATA/<dir>/<new_name>`. */
static int add_copy(FileSet *set, const char *real, const char *dir, const char *new_name, ExchangeError *err)
{
    size_t size = 0;
    uint8_t *data = fsio_read_file(real, &size, err);
    if (data == NULL) return 0;
    char name[PATH_MAX];
    snprintf(name, sizeof name, "SIMDATA/%s/%s", dir, new_name);
    const int ok = fileset_add(set, name, data, size);
    free(data);
    if (!ok) exchange_fail(err, "cannot add %s to the track", name);
    return ok;
}

/* The FAMs of the template: STEM_*.FAM in the three track-family directories. */
static int add_fams(FileSet *set, const char *data, const char *stem, const char *new_stem, ExchangeError *err)
{
    static const char *const dirs[] = { "NTRACKFM", "ETRACKFM", "GTRACKFM" };
    const size_t stem_len = strlen(stem);
    for (size_t d = 0; d < 3; ++d) {
        char rel[64], dir[PATH_MAX];
        snprintf(rel, sizeof rel, "SIMDATA/%s", dirs[d]);
        if (!fsio_find_ci(data, rel, dir, sizeof dir)) continue;
        DIR *handle = opendir(dir);
        if (handle == NULL) continue;
        struct dirent *entry;
        while ((entry = readdir(handle)) != NULL) {
            const char *n = entry->d_name;
            const size_t len = strlen(n);
            if (len < stem_len + 6 || strncasecmp(n, stem, stem_len) != 0 || n[stem_len] != '_' || strcasecmp(n + len - 4, ".FAM") != 0) continue;
            char real[PATH_MAX], renamed[PATH_MAX];
            snprintf(real, sizeof real, "%s/%s", dir, n);
            snprintf(renamed, sizeof renamed, "%s%s", new_stem, n + stem_len);
            if (!add_copy(set, real, dirs[d], renamed, err)) { closedir(handle); return 0; }
        }
        closedir(handle);
    }
    return 1;
}

static int track_new(const Args *a, ExchangeError *err)
{
    char stem[16];
    if (!slot_find(SLOT_TRACK, a->slot, stem, sizeof stem)) { exchange_fail(err, "'%s' is not a track slot", a->slot); return 0; }
    char new_stem[16];
    if (a->stem != NULL) {
        if (!stem_ok(a->stem)) { exchange_fail(err, "--stem '%s' must be 1..8 letters or digits", a->stem); return 0; }
        snprintf(new_stem, sizeof new_stem, "%s", a->stem);
        for (char *c = new_stem; *c; ++c) *c = (char)toupper((unsigned char)*c);
    } else {
        snprintf(new_stem, sizeof new_stem, "%s", stem);
    }

    size_t layout_size = 0, tri_size = 0;
    uint8_t *layout = fsio_read_file(a->layout, &layout_size, err);
    if (layout == NULL) return 0;
    char rel[64], real[PATH_MAX];
    snprintf(rel, sizeof rel, "SIMDATA/MISC/%s.TRI", stem);
    uint8_t *tri = NULL, *made = NULL;
    size_t made_size = 0;
    FileSet set;
    fileset_init(&set);
    int ok = 0;
    if (!fsio_find_ci(a->data, rel, real, sizeof real)) { exchange_fail(err, "no such file in the data root: %s", rel); goto done; }
    if ((tri = fsio_read_file(real, &tri_size, err)) == NULL) goto done;
    if (!track_generate((const char *)layout, layout_size, tri, tri_size, &made, &made_size, err)) {
        char detail[sizeof err->message];
        snprintf(detail, sizeof detail, "%s", err->message);
        exchange_fail(err, "%s: %s", a->layout, detail);
        goto done;
    }
    char name[PATH_MAX];
    snprintf(name, sizeof name, "SIMDATA/MISC/%s.TRI", new_stem);
    if (!fileset_add(&set, name, made, made_size)) { exchange_fail(err, "cannot add %s", name); goto done; }
    snprintf(rel, sizeof rel, "SIMDATA/MISC/%sINFO", stem);
    if (fsio_find_ci(a->data, rel, real, sizeof real)) {
        snprintf(name, sizeof name, "%sINFO", new_stem);
        if (!add_copy(&set, real, "MISC", name, err)) goto done;
    }
    if (!add_fams(&set, a->data, stem, new_stem, err)) goto done;
    if (!fsio_write_tree(a->out, &set, a->force, err)) goto done;
    printf("track %s: %d segments built from %s; %zu files in %s (for slot %s: slot replace track %s %s --stem %s --mods DIR)\n",
           new_stem, made[6] | made[7] << 8, stem, set.count, a->out, stem, stem, a->out, new_stem);
    ok = 1;
done:
    free(layout);
    free(tri);
    free(made);
    fileset_free(&set);
    return ok;
}

int track_command(int argc, char **argv, ExchangeError *err)
{
    if (argc < 2 || strcmp(argv[1], "new") != 0) { exchange_fail(err, "usage: tnfs-studio track new LAYOUT --template SLOT --out DIR"); return 0; }
    Args a;
    return parse(argc, argv, &a, err) && track_new(&a, err);
}

#include "studio/track_new.h"

#include "formats/mod_ini.h"
#include "formats/slots.h"
#include "formats/track_gen.h"
#include "formats/track_piece.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *layout, *slot, *out, *stem, *data, *id, *name, *road, *edge, *author, *description;
    int force, piece;
} Args;

/* a piece's card on disk, read before --force writes it again: its parsed keys, and the lines of the keys the generator does not write */
typedef struct {
    ModInfo info;
    char others[MOD_INI_MAX_SIZE];
} Kept;

/* 1 when `text` (a flag's value, NULL when not given) holds a line break, which would break mod.ini's line */
static int has_line_break(const char *text)
{
    return text != NULL && (strchr(text, '\n') != NULL || strchr(text, '\r') != NULL);
}

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
        else if (strcmp(argv[i], "--id") == 0 && has_value) a->id = argv[++i];
        else if (strcmp(argv[i], "--name") == 0 && has_value) a->name = argv[++i];
        else if (strcmp(argv[i], "--road") == 0 && has_value) a->road = argv[++i];
        else if (strcmp(argv[i], "--edge") == 0 && has_value) a->edge = argv[++i];
        else if (strcmp(argv[i], "--author") == 0 && has_value) a->author = argv[++i];
        else if (strcmp(argv[i], "--description") == 0 && has_value) a->description = argv[++i];
        else if (strcmp(argv[i], "--piece") == 0) a->piece = 1;
        else if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (argv[i][0] == '-' && argv[i][1] == '-') { exchange_fail(err, "unknown option '%s'", argv[i]); return 0; }
        else if (a->layout == NULL) a->layout = argv[i];
        else { exchange_fail(err, "too many arguments"); return 0; }
    }
    if (a->layout == NULL || a->slot == NULL || a->out == NULL) {
        exchange_fail(err, "usage: tnfs-studio track new LAYOUT --template SLOT --out DIR [--stem NAME] [--data DIR] [--force]");
        return 0;
    }
    if (a->piece && (a->id == NULL || a->stem == NULL)) {
        exchange_fail(err, "usage: tnfs-studio track new LAYOUT --template SLOT --piece --id ID --stem STEM --out DIR [--name TEXT] [--road asphalt|dirt] [--edge WORD] [--author TEXT] [--description TEXT] [--data DIR] [--force]");
        return 0;
    }
    if (!a->piece && (a->id != NULL || a->name != NULL || a->road != NULL || a->edge != NULL || a->author != NULL || a->description != NULL)) {
        exchange_fail(err, "--id, --name, --road, --edge, --author and --description go with --piece");
        return 0;
    }
    if (has_line_break(a->author) || has_line_break(a->description)) {
        exchange_fail(err, "--author and --description are one line of text");
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
/* `piece` (a piece's stem, or NULL): only the files that piece may carry */
static int add_fams(FileSet *set, const char *data, const char *stem, const char *new_stem, const char *piece, ExchangeError *err)
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
            if (piece != NULL) {
                char rel_name[PATH_MAX];
                snprintf(rel_name, sizeof rel_name, "SIMDATA/%s/%s", dirs[d], renamed);
                if (!track_piece_owns(piece, rel_name)) continue;
            }
            if (!add_copy(set, real, dirs[d], renamed, err)) { closedir(handle); return 0; }
        }
        closedir(handle);
    }
    return 1;
}

/* the piece's card, judged by the rules the game reads it with, and its files by track_piece_check */
/* `flag` when given, else the kept card's `kept` when it has one, else `dflt` */
static const char *pick(const char *flag, const char *kept, const char *dflt)
{
    if (flag != NULL) return flag;
    return kept[0] != '\0' ? kept : dflt;
}

/* the card of a piece, the keys the generator writes from the flags, else the kept card (--force), else the defaults; the kept card's
 * other keys follow as written */
static int add_card(FileSet *set, const Args *a, const char *stem, const Kept *kept, ExchangeError *err)
{
    char head[MOD_INI_MAX_SIZE], texts[MOD_INI_MAX_SIZE], card[MOD_INI_MAX_SIZE];
    const char *name = pick(a->name, kept->info.name, a->id);
    const char *version = pick(NULL, kept->info.version, "0.1");
    const char *road = pick(a->road, kept->info.road, "asphalt");
    const char *edge = pick(a->edge, kept->info.edge, "grass");
    const char *author = pick(a->author, kept->info.author, "");
    const char *description = pick(a->description, kept->info.description, "");
    ModInfo info;
    char why[160];
    const int h = snprintf(head, sizeof head, "id=%s\nname=%s\nversion=%s\ntype=track\nstem=%s\nkind=stage\nsegments=1\nroad=%s\nedge=%s\n",
                           a->id, name, version, stem, road, edge);
    if (h < 0 || (size_t)h >= sizeof head || !mod_ini_text_keys(author, description, texts, sizeof texts)) {
        exchange_fail(err, "the card does not fit mod.ini's %d bytes", MOD_INI_MAX_SIZE);
        return 0;
    }
    const int n = snprintf(card, sizeof card, "%s%s%s", head, texts, kept->others);
    if (n < 0 || (size_t)n >= sizeof card) { exchange_fail(err, "the card does not fit mod.ini's %d bytes", MOD_INI_MAX_SIZE); return 0; }
    if (!mod_ini_parse((const uint8_t *)card, (size_t)n, &info, why, sizeof why)) { exchange_fail(err, "mod.ini: %s", why); return 0; }
    Report r;
    report_init(&r);
    if (!track_piece_check(&info, set, NULL, &r)) {
        char text[512];
        report_text(&r, VALIDATE_COMPATIBLE, text, sizeof text);
        exchange_fail(err, "the piece is not good: %s", text);
        report_free(&r);
        return 0;
    }
    report_free(&r);
    if (!fileset_add(set, "mod.ini", (const uint8_t *)card, (size_t)n)) { exchange_fail(err, "out of memory"); return 0; }
    return 1;
}

/* --force over a piece's folder: the card already there is read, so its hand-written keys are kept. 0 with `err` when it is not a card. */
static int read_kept(const Args *a, Kept *kept, ExchangeError *err)
{
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/mod.ini", a->out);
    if (!a->piece || !fsio_exists(path)) return 1;
    size_t size = 0;
    uint8_t *text = fsio_read_file(path, &size, err);
    if (text == NULL) return 0;
    char why[160];
    int ok = mod_ini_parse(text, size, &kept->info, why, sizeof why);
    if (!ok) exchange_fail(err, "%s: %s; fix it, or move it aside to write a new piece", path, why);
    size_t others = 0;
    if (ok && !mod_ini_others(text, size, kept->others, sizeof kept->others, &others)) {
        exchange_fail(err, "%s: its other keys are longer than %d bytes", path, MOD_INI_MAX_SIZE);
        ok = 0;
    }
    free(text);
    return ok;
}

static int track_new(const Args *a, ExchangeError *err)
{
    Kept kept;
    memset(&kept, 0, sizeof kept);
    char stem[16];
    if (!slot_find(SLOT_TRACK, a->slot, stem, sizeof stem)) { exchange_fail(err, "'%s' is not a track slot", a->slot); return 0; }
    char new_stem[16], piece_stem[16] = "";
    if (a->piece) {   /* port (dep59 step 5, T6): a track piece's root is its stem and the stage's number, as the CD's */
        snprintf(new_stem, sizeof new_stem, "%s", a->stem);
        for (char *c = new_stem; *c; ++c) *c = (char)toupper((unsigned char)*c);
        if (!track_piece_stem_ok(new_stem)) {
            exchange_fail(err, "--stem '%s' must be 1..%d of A-Z and 0-9 and not begin with AL, CL, CY or TR (the CD's)", a->stem, TRACK_PIECE_STEM_MAX);
            return 0;
        }
        if (!mod_ini_id_ok(a->id)) { exchange_fail(err, "--id '%s' must be up to %d of a-z, 0-9 and '-', not first", a->id, MOD_INI_ID_MAX); return 0; }
        snprintf(piece_stem, sizeof piece_stem, "%s", new_stem);
        strncat(new_stem, "1", sizeof new_stem - strlen(new_stem) - 1);
    } else if (a->stem != NULL) {
        if (!stem_ok(a->stem)) { exchange_fail(err, "--stem '%s' must be 1..8 letters or digits", a->stem); return 0; }
        snprintf(new_stem, sizeof new_stem, "%s", a->stem);
        for (char *c = new_stem; *c; ++c) *c = (char)toupper((unsigned char)*c);
    } else {
        snprintf(new_stem, sizeof new_stem, "%s", stem);
    }

    size_t layout_size = 0, tri_size = 0;
    if (!read_kept(a, &kept, err)) return 0;
    uint8_t *layout = fsio_read_file(a->layout, &layout_size, err);
    if (layout == NULL) return 0;
    char rel[64], real[PATH_MAX];
    snprintf(rel, sizeof rel, "SIMDATA/MISC/%s.TRI", stem);
    uint8_t *tri = NULL, *made = NULL;
    size_t made_size = 0;
    FileSet set;
    fileset_init(&set);
    int ok = 0;
    if (!fsio_find_ci(a->data, rel, real, sizeof real)) {
        exchange_fail(err, "no %s in the data root %s: give the CD's folder with --data DIR", rel, a->data);
        goto done;
    }
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
    if (!add_fams(&set, a->data, stem, new_stem, a->piece ? piece_stem : NULL, err)) goto done;
    if (a->piece && !add_card(&set, a, piece_stem, &kept, err)) goto done;
    if (!fsio_write_tree(a->out, &set, a->force, err)) goto done;
    if (a->piece) printf("track piece %s (stem %s): %d segments built from %s; %zu files in %s; pack it with: tnfs-studio mod pack %s OUT.zip, or install the folder\n",
                         a->id, piece_stem, made[6] | made[7] << 8, stem, set.count, a->out, a->out);
    else printf("track %s: %d segments built from %s; %zu files in %s (for slot %s: slot replace track %s %s --stem %s --mods DIR)\n",
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

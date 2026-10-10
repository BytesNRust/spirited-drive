#include "studio/slot.h"

#include "formats/slots.h"
#include "formats/validate.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_ALSO = 64, PATH_BYTES = 1024, STATE_DIR_LEN = 32 };

static const char state_dir[] = ".tnfs-studio";
static const char journal_name[] = "journal.tsv";

typedef struct {
    const char *mods;
    const char *data;
    const char *stem;
    const char *also[MAX_ALSO];
    int also_count;
    int all;
    int force;
    const char *positional[4];
    int positionals;
} SlotArgs;

static int parse_args(int argc, char **argv, SlotArgs *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    a->data = "original";
    for (int i = 2; i < argc; ++i) {   /* argv[0] is "slot", argv[1] the subcommand */
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--mods") == 0 && has_value) a->mods = argv[++i];
        else if (strcmp(argv[i], "--data") == 0 && has_value) a->data = argv[++i];
        else if (strcmp(argv[i], "--stem") == 0 && has_value) a->stem = argv[++i];
        else if (strcmp(argv[i], "--also") == 0 && has_value) {
            if (a->also_count == MAX_ALSO) { exchange_fail(err, "more than %d --also files", MAX_ALSO); return 0; }
            a->also[a->also_count++] = argv[++i];
        } else if (strcmp(argv[i], "--all") == 0) a->all = 1;
        else if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (argv[i][0] == '-' && argv[i][1] == '-') { exchange_fail(err, "unknown option '%s'", argv[i]); return 0; }
        else if (a->positionals < 4) a->positional[a->positionals++] = argv[i];
        else { exchange_fail(err, "too many arguments"); return 0; }
    }
    return 1;
}

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

static int strncasecmp_n(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
}

static int parse_kind(const char *s, SlotKind *kind)
{
    if (ieq(s, "track")) { *kind = SLOT_TRACK; return 1; }
    if (ieq(s, "car")) { *kind = SLOT_CAR; return 1; }
    return 0;
}

/* ------------------------------------------------------------------ journal */

typedef struct {
    uint32_t id;
    char kind[8], slot[16], op[12], rel[PATH_BYTES];
    uint64_t before, after;
} Row;

typedef struct {
    Row *row;
    size_t count, cap;
} Journal;

static void journal_path(const char *mods, char *out, size_t cap)
{
    snprintf(out, cap, "%s/%s/%s", mods, state_dir, journal_name);
}

static int journal_read(const char *mods, Journal *j, ExchangeError *err)
{
    memset(j, 0, sizeof *j);
    char path[PATH_BYTES];
    journal_path(mods, path, sizeof path);
    if (!fsio_exists(path)) return 1;
    size_t size = 0;
    uint8_t *text = fsio_read_file(path, &size, err);
    if (text == NULL) return 0;
    char *copy = malloc(size + 1);
    if (copy == NULL) { free(text); exchange_fail(err, "out of memory"); return 0; }
    memcpy(copy, text, size);
    copy[size] = '\0';
    free(text);
    char *save = copy;
    int ok = 1;
    for (uint32_t line = 1; ok && *save; ++line) {
        char *nl = strchr(save, '\n');
        if (nl) *nl = '\0';
        char *cur = save;
        save = nl ? nl + 1 : save + strlen(save);
        if (*cur == '#' || *cur == '\0') continue;
        Row r;
        memset(&r, 0, sizeof r);
        char before[32], after[32];
        char *cells[8];
        int n = 0;
        for (char *p = cur; n < 8; ) {
            cells[n++] = p;
            char *tab = strchr(p, '\t');
            if (!tab) break;
            *tab = '\0';
            p = tab + 1;
        }
        if (n != 7 || strlen(cells[1]) >= sizeof r.kind || strlen(cells[2]) >= sizeof r.slot || strlen(cells[3]) >= sizeof r.op ||
            strlen(cells[4]) >= sizeof r.rel || strlen(cells[5]) >= sizeof before || strlen(cells[6]) >= sizeof after) {
            exchange_fail(err, "%s:%u: damaged journal row", path, line);
            ok = 0;
            break;
        }
        r.id = (uint32_t)strtoul(cells[0], NULL, 10);
        strcpy(r.kind, cells[1]); strcpy(r.slot, cells[2]); strcpy(r.op, cells[3]); strcpy(r.rel, cells[4]);
        r.before = strtoull(cells[5], NULL, 16);
        r.after = strtoull(cells[6], NULL, 16);
        if (j->count == j->cap) {
            const size_t cap = j->cap ? j->cap * 2 : 32;
            Row *grown = realloc(j->row, cap * sizeof *grown);
            if (grown == NULL) { exchange_fail(err, "out of memory"); ok = 0; break; }
            j->row = grown;
            j->cap = cap;
        }
        j->row[j->count++] = r;
    }
    free(copy);
    if (!ok) { free(j->row); memset(j, 0, sizeof *j); }
    return ok;
}

static int journal_write(const char *mods, const Journal *j, ExchangeError *err)
{
    char path[PATH_BYTES];
    journal_path(mods, path, sizeof path);
    if (j->count == 0) {   /* nothing left to undo: the state directory goes away */
        char dir[PATH_BYTES];
        snprintf(dir, sizeof dir, "%s/%s", mods, state_dir);
        if (!fsio_remove_tree(dir)) { exchange_fail(err, "%s: cannot remove", dir); return 0; }
        return 1;
    }
    size_t cap = 256 + j->count * (PATH_BYTES + 128);
    char *text = malloc(cap);
    if (text == NULL) { exchange_fail(err, "out of memory"); return 0; }
    size_t n = (size_t)snprintf(text, cap, "# id\\tkind\\tslot\\top\\trel\\tbefore\\tafter\\n");
    n = (size_t)snprintf(text, cap, "# id\tkind\tslot\top\trel\tbefore\tafter\n");
    for (size_t i = 0; i < j->count; ++i) {
        const Row *r = &j->row[i];
        n += (size_t)snprintf(text + n, cap - n, "%u\t%s\t%s\t%s\t%s\t%016" PRIx64 "\t%016" PRIx64 "\n", r->id, r->kind, r->slot, r->op, r->rel, r->before, r->after);
    }
    const int ok = fsio_write_atomic(path, (const uint8_t *)text, n, err);
    free(text);
    return ok;
}

static void backup_path(const char *mods, uint32_t id, const char *rel, char *out, size_t cap)
{
    snprintf(out, cap, "%s/%s/backup/%u/%s", mods, state_dir, id, rel);
}

/* ------------------------------------------------------------------ replace */

typedef struct {
    char rel[PATH_BYTES];      /* the CD's spelling, below the data root */
    const uint8_t *data;
    size_t size;
} Planned;

/* A source name with --stem applied: a base name starting OLD, or M followed by OLD, takes the slot's stem. */
static void apply_stem(const char *rel, const char *old, const char *stem, char *out, size_t cap)
{
    const char *base = strrchr(rel, '/');
    base = base ? base + 1 : rel;
    const size_t dir_len = (size_t)(base - rel), old_len = old ? strlen(old) : 0;
    snprintf(out, cap, "%s", rel);
    if (old_len == 0) return;
    const char *rest = NULL;
    const char *lead = "";
    if (strlen(base) >= old_len && strncasecmp_n(base, old, old_len)) rest = base + old_len;
    else if ((base[0] == 'M' || base[0] == 'm') && strlen(base + 1) >= old_len && strncasecmp_n(base + 1, old, old_len)) { rest = base + 1 + old_len; lead = "M"; }
    if (rest) snprintf(out + dir_len, cap - dir_len, "%s%s%s", lead, stem, rest);
}

static int listed(const SlotArgs *a, const char *rel_src, const char *rel)
{
    for (int i = 0; i < a->also_count; ++i)
        if (ieq(a->also[i], rel_src) || ieq(a->also[i], rel)) return 1;
    return 0;
}

static int cmd_replace(const SlotArgs *a, ExchangeError *err)
{
    SlotKind kind;
    char stem[16];
    if (a->positionals != 3 || !parse_kind(a->positional[0], &kind)) { exchange_fail(err, "usage: slot replace track|car SLOT SOURCE_DIR --mods DIR"); return 0; }
    if (a->mods == NULL) { exchange_fail(err, "slot replace needs --mods DIR (nothing is ever written to the data root)"); return 0; }
    if (!slot_find(kind, a->positional[1], stem, sizeof stem)) {
        exchange_fail(err, "'%s' is not a %s slot of the original game (the slots are fixed; `slot list %s` shows them; adding one is Level 2)", a->positional[1], a->positional[0], a->positional[0]);
        return 0;
    }
    FileSet source;
    fileset_init(&source);
    Planned *plan = NULL;
    int ok = fsio_read_tree(a->positional[2], &source, err);
    if (ok && source.count == 0) { exchange_fail(err, "%s holds no files", a->positional[2]); ok = 0; }
    if (ok) plan = calloc(source.count, sizeof *plan);
    if (ok && plan == NULL) { exchange_fail(err, "out of memory"); ok = 0; }

    /* every --also must name a file of the source */
    for (int i = 0; ok && i < a->also_count; ++i) {
        int found = 0;
        for (size_t f = 0; f < source.count && !found; ++f) found = ieq(source.file[f].name, a->also[i]);
        if (!found) { exchange_fail(err, "--also %s: not in %s", a->also[i], a->positional[2]); ok = 0; }
    }
    /* ownership, existence in the data root, and the CD's own spelling of each name */
    Report report;
    report_init(&report);
    for (size_t f = 0; ok && f < source.count; ++f) {
        char rel[PATH_BYTES], real[PATH_BYTES];
        apply_stem(source.file[f].name, a->stem, stem, rel, sizeof rel);
        if (!slot_owns(kind, stem, rel) && !listed(a, source.file[f].name, rel)) {
            exchange_fail(err, "%s is not a file of %s slot %s (its own files: see slots.h; name other files of the slot with --also)", source.file[f].name, a->positional[0], stem);
            ok = 0;
            break;
        }
        if (!fsio_find_ci(a->data, rel, real, sizeof real)) {
            exchange_fail(err, "%s: no such file in %s; the game only reads files it already has, so a slot cannot gain files (Level 2)", rel, a->data);
            ok = 0;
            break;
        }
        const size_t root_len = strlen(a->data) + (a->data[strlen(a->data) - 1] == '/' ? 0 : 1);
        snprintf(plan[f].rel, sizeof plan[f].rel, "%s", real + root_len);
        plan[f].data = source.file[f].data;
        plan[f].size = source.file[f].size;
        for (size_t g = 0; g < f; ++g)
            if (strcmp(plan[g].rel, plan[f].rel) == 0) { exchange_fail(err, "%s and %s both become %s", source.file[g].name, source.file[f].name, plan[f].rel); ok = 0; }
        validate_file(source.file[f].data, source.file[f].size, plan[f].rel, &report);
    }
    if (ok && report_fatal_count(&report) > 0) {
        char text[4096];
        report_text(&report, VALIDATE_COMPATIBLE, text, sizeof text);
        fputs(text, stderr);
        exchange_fail(err, "%zu limits of the original executable are exceeded; nothing was written", report_fatal_count(&report));
        ok = 0;
    }
    report_free(&report);

    Journal journal;
    memset(&journal, 0, sizeof journal);
    if (ok) ok = journal_read(a->mods, &journal, err);
    uint32_t id = 1;
    for (size_t i = 0; ok && i < journal.count; ++i) if (journal.row[i].id >= id) id = journal.row[i].id + 1;
    for (size_t f = 0; ok && f < source.count; ++f) {
        char dest[PATH_BYTES], backup[PATH_BYTES];
        snprintf(dest, sizeof dest, "%s/%s", a->mods, plan[f].rel);
        Row r;
        memset(&r, 0, sizeof r);
        r.id = id;
        snprintf(r.kind, sizeof r.kind, "%s", a->positional[0]);
        snprintf(r.slot, sizeof r.slot, "%s", stem);
        snprintf(r.rel, sizeof r.rel, "%s", plan[f].rel);
        if (fsio_exists(dest)) {
            size_t old_size = 0;
            uint8_t *old = fsio_read_file(dest, &old_size, err);
            if (old == NULL) { ok = 0; break; }
            backup_path(a->mods, id, plan[f].rel, backup, sizeof backup);
            ok = fsio_write_file(backup, old, old_size, 1, err);
            r.before = fsio_hash(old, old_size);
            free(old);
            snprintf(r.op, sizeof r.op, "replaced");
        } else {
            snprintf(r.op, sizeof r.op, "added");
        }
        r.after = fsio_hash(plan[f].data, plan[f].size);
        if (ok) {
            /* the journal row goes in first: an interrupted run is then still undoable */
            if (journal.count == journal.cap) {
                const size_t cap = journal.cap ? journal.cap * 2 : 32;
                Row *grown = realloc(journal.row, cap * sizeof *grown);
                if (grown == NULL) { exchange_fail(err, "out of memory"); ok = 0; break; }
                journal.row = grown;
                journal.cap = cap;
            }
            journal.row[journal.count++] = r;
            ok = journal_write(a->mods, &journal, err) && fsio_write_atomic(dest, plan[f].data, plan[f].size, err);
        }
        if (ok) printf("%s %s\n", r.op, plan[f].rel);
    }
    if (ok) {
        printf("slot %s %s: %zu files in %s (transaction %u); `slot undo --mods %s` reverts it\n", a->positional[0], stem, source.count, a->mods, id, a->mods);
        if (kind == SLOT_CAR)
            printf("note: the short car names on the results and records screens are text inside the EXE (Level 2); the full names on the selection screens are bitmaps and can be replaced with --also\n");
    }
    free(journal.row);
    free(plan);
    fileset_free(&source);
    return ok;
}

/* ------------------------------------------------------------------ undo */

static int undo_transaction(const SlotArgs *a, Journal *j, uint32_t id, ExchangeError *err)
{
    for (size_t i = j->count; i-- > 0;) {
        const Row *r = &j->row[i];
        if (r->id != id) continue;
        char dest[PATH_BYTES], backup[PATH_BYTES];
        snprintf(dest, sizeof dest, "%s/%s", a->mods, r->rel);
        if (fsio_exists(dest) && !a->force) {
            size_t size = 0;
            uint8_t *now = fsio_read_file(dest, &size, err);
            if (now == NULL) return 0;
            const uint64_t h = fsio_hash(now, size);
            free(now);
            if (h != r->after) { exchange_fail(err, "%s changed since transaction %u wrote it (use --force to undo anyway)", dest, id); return 0; }
        }
        if (strcmp(r->op, "replaced") == 0) {
            backup_path(a->mods, id, r->rel, backup, sizeof backup);
            size_t size = 0;
            uint8_t *old = fsio_read_file(backup, &size, err);
            if (old == NULL) return 0;
            const int ok = fsio_write_atomic(dest, old, size, err);
            const uint64_t h = fsio_hash(old, size);
            free(old);
            if (!ok) return 0;
            if (h != r->before) { exchange_fail(err, "%s: the saved copy does not match the journal", backup); return 0; }
        } else {
            if (!fsio_remove(dest)) { exchange_fail(err, "%s: cannot remove", dest); return 0; }
            char dir[PATH_BYTES];
            snprintf(dir, sizeof dir, "%s", dest);
            char *slash = strrchr(dir, '/');
            if (slash) { *slash = '\0'; fsio_prune_empty(dir, a->mods); }
        }
    }
    char dir[PATH_BYTES];
    snprintf(dir, sizeof dir, "%s/%s/backup/%u", a->mods, state_dir, id);
    fsio_remove_tree(dir);
    size_t keep = 0;
    for (size_t i = 0; i < j->count; ++i) if (j->row[i].id != id) j->row[keep++] = j->row[i];
    j->count = keep;
    return 1;
}

static int cmd_undo(const SlotArgs *a, ExchangeError *err)
{
    if (a->mods == NULL || a->positionals != 0) { exchange_fail(err, "usage: slot undo --mods DIR [--all] [--force]"); return 0; }
    Journal j;
    if (!journal_read(a->mods, &j, err)) return 0;
    if (j.count == 0) { exchange_fail(err, "%s: nothing to undo", a->mods); free(j.row); return 0; }
    int ok = 1;
    while (ok && j.count > 0) {
        uint32_t id = 0;
        for (size_t i = 0; i < j.count; ++i) if (j.row[i].id > id) id = j.row[i].id;
        ok = undo_transaction(a, &j, id, err) && journal_write(a->mods, &j, err);
        if (ok) printf("undid transaction %u\n", id);
        if (!a->all) break;
    }
    free(j.row);
    return ok;
}

static int cmd_list(const SlotArgs *a, ExchangeError *err)
{
    SlotKind kind;
    if (a->positionals != 1 || !parse_kind(a->positional[0], &kind)) { exchange_fail(err, "usage: slot list track|car"); return 0; }
    for (const char *const *s = slot_stems(kind); *s; ++s) printf("%s\n", *s);
    return 1;
}

int slot_command(int argc, char **argv, ExchangeError *err)
{
    SlotArgs a;
    if (argc < 2) { exchange_fail(err, "usage: tnfs-studio slot list|replace|undo ..."); return 0; }
    const char *sub = argv[1];
    if (!parse_args(argc, argv, &a, err)) return 0;
    if (strcmp(sub, "replace") == 0) return cmd_replace(&a, err);
    if (strcmp(sub, "undo") == 0) return cmd_undo(&a, err);
    if (strcmp(sub, "list") == 0) return cmd_list(&a, err);
    exchange_fail(err, "unknown slot command '%s'", sub);
    return 0;
}

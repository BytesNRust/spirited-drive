#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
/* original_track.h: an original track as a workspace project. */
#include "original_track.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const STEMS[OT_STEMS] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7" };

const char *original_track_stem(int index)
{
    return index >= 0 && index < OT_STEMS ? STEMS[index] : NULL;
}

static int is_stem(const char *s)
{
    for (int i = 0; s != NULL && i < OT_STEMS; ++i)
        if (strcmp(s, STEMS[i]) == 0) return 1;
    return 0;
}

static void fail(char *why, size_t cap, const char *fmt, const char *a)
{
    if (why != NULL && cap > 0) snprintf(why, cap, fmt, a);
}

int original_track_dir(const char *workspace, const char *stem, char *out, size_t cap)
{
    if (workspace == NULL || !is_stem(stem)) return 0;
    const int n = snprintf(out, cap, "%s/tracks/%s", workspace, stem);
    return n > 0 && (size_t)n < cap;
}

int original_track_light_dir(const char *dir, char *out, size_t cap)
{
    const int n = snprintf(out, cap, "%s/light", dir);
    return n > 0 && (size_t)n < cap;
}

int original_track_stem_of(const char *dir, char *stem, size_t cap)
{
    char path[1024], line[128];
    int kind = 0, got = 0;
    snprintf(path, sizeof path, "%s/original.ini", dir);
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    while (fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "kind=original-track") == 0) kind = 1;
        else if (strncmp(line, "stem=", 5) == 0 && is_stem(line + 5) && strlen(line + 5) < cap) snprintf(stem, cap, "%s", line + 5), got = 1;
    }
    fclose(f);
    return kind && got;
}

int original_track_present(const char *dir)
{
    char stem[16];
    return original_track_stem_of(dir, stem, sizeof stem);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int make_dir(const char *path)
{
    return is_dir(path) || mkdir(path, 0777) == 0;
}

static int make_dirs(const char *dir, const char *const *sub, int n)
{
    char path[1100];
    if (!make_dir(dir)) return 0;
    for (int i = 0; i < n; ++i) {
        snprintf(path, sizeof path, "%s/%s", dir, sub[i]);
        if (!make_dir(path)) return 0;
    }
    return 1;
}

static int exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static unsigned char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    rewind(f);
    unsigned char *b = malloc((size_t)(n > 0 ? n : 0) + 1);
    *len = b != NULL && fread(b, 1, (size_t)n, f) == (size_t)n ? (size_t)n : 0;
    fclose(f);
    if (b != NULL && *len != (size_t)n) free(b), b = NULL;
    if (b != NULL) b[*len] = '\0';
    return b;
}

static int write_file(const char *path, const unsigned char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

/* copies `from` to `to`; 1 copied, 2 `to` was there already (kept), 0 failed */
static int copy_keep(const char *from, const char *to)
{
    if (exists(to)) return 2;
    size_t len;
    unsigned char *b = slurp(from, &len);
    if (b == NULL) return 0;
    const int ok = write_file(to, b, len);
    free(b);
    return ok;
}

static int add_light(const char *dir, const char *stem, const char *data_dir, OriginalAdd *out, char *why, size_t cap)
{
    char from[1100], to[1100], rel[64];
    snprintf(rel, sizeof rel, "track_light/%s.txt", stem);   /* the track's one light file */
    snprintf(from, sizeof from, "%s/%s", data_dir, rel);
    snprintf(to, sizeof to, "%s/light/%s", dir, rel);
    const int r = copy_keep(from, to);
    if (r == 0) return fail(why, cap, "cannot copy %s from the game's data", rel), 0;
    if (r == 2) ++out->kept;
    else ++out->light_files;
    return 1;
}

static int is_psd(const char *name)
{
    const size_t n = strlen(name);
    if (n < 5 || strcmp(name + n - 4, ".psd") != 0) return 0;
    for (size_t i = 0; i + 4 < n; ++i)
        if (name[i] == '.') return 0;   /* a backup (slot_03.before-paint.psd) or a hidden file */
    return 1;
}

static void add_psds(const char *dir, const char *stem, const char *psd_root, OriginalAdd *out)
{
    char from_dir[1100];
    snprintf(from_dir, sizeof from_dir, "%s/%s", psd_root, stem);
    DIR *d = opendir(from_dir);
    if (d == NULL) return;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (!is_psd(e->d_name)) { ++out->skipped; continue; }
        char from[1700], to[1700];
        snprintf(from, sizeof from, "%s/%s", from_dir, e->d_name);
        snprintf(to, sizeof to, "%s/art-source/masks/%s", dir, e->d_name);
        const int r = copy_keep(from, to);
        if (r == 2) ++out->kept;
        else if (r == 1) ++out->psds;
    }
    closedir(d);
}

int original_track_add(const char *workspace, const char *stem, const char *data_dir, const char *psd_root, OriginalAdd *out, char *why, size_t cap)
{
    char dir[1024], ini[1100];
    if (!original_track_dir(workspace, stem, dir, sizeof dir)) return fail(why, cap, "%s is not one of the game's tracks", stem != NULL ? stem : "(none)"), 0;
    static const char *const SUB[] = { "light", "light/track_light", "art-source", "art-source/masks", "art-generated", "art-generated/track_masks" };
    char tracks[1100];
    snprintf(tracks, sizeof tracks, "%s/tracks", workspace);
    if (!make_dir(tracks) || !make_dirs(dir, SUB, 7)) return fail(why, cap, "cannot make the folder %s", dir), 0;
    if (!add_light(dir, stem, data_dir, out, why, cap)) return 0;
    snprintf(ini, sizeof ini, "%s/original.ini", dir);
    if (!exists(ini)) {
        char body[96];
        const int n = snprintf(body, sizeof body, "kind=original-track\nstem=%s\n", stem);
        if (!write_file(ini, (const unsigned char *)body, (size_t)n)) return fail(why, cap, "cannot write %s", ini), 0;
    }
    if (psd_root != NULL && psd_root[0] != '\0') add_psds(dir, stem, psd_root, out);
    ++out->tracks;
    return 1;
}

int original_track_add_all(const char *workspace, const char *data_dir, const char *psd_root, OriginalAdd *out, char *why, size_t cap)
{
    for (int i = 0; i < OT_STEMS; ++i)
        if (!original_track_add(workspace, STEMS[i], data_dir, psd_root, out, why, cap)) return 0;
    return 1;
}

/* ---- publish ---- */

static int line_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* the lines of `text` (modified in place), sorted: how many, into `*lines` */
static int split_sorted(unsigned char *text, char ***lines)
{
    int n = 0, cap = 64;
    char **l = malloc((size_t)cap * sizeof *l);
    char *p = (char *)text;
    while (l != NULL && *p != '\0') {
        char *e = strchr(p, '\n');
        if (e != NULL) *e = '\0';
        if (n == cap) { cap *= 2; l = realloc(l, (size_t)cap * sizeof *l); if (l == NULL) break; }
        l[n++] = p;
        p = e != NULL ? e + 1 : p + strlen(p);
    }
    if (l != NULL && n > 1) qsort(l, (size_t)n, sizeof *l, line_cmp);
    *lines = l;
    return l != NULL ? n : 0;
}

/* the lines `a` has that `b` has not (a multiset difference), and the reverse */
static void line_diff(unsigned char *a_text, unsigned char *b_text, int *added, int *removed)
{
    char **a, **b;
    const int na = split_sorted(a_text, &a), nb = split_sorted(b_text, &b);
    int i = 0, j = 0;
    *added = *removed = 0;
    while (i < na || j < nb) {
        const int c = i >= na ? 1 : j >= nb ? -1 : strcmp(a[i], b[j]);
        if (c == 0) ++i, ++j;
        else if (c < 0) ++*added, ++i;
        else ++*removed, ++j;
    }
    free(a), free(b);
}

static void rel_of(char *out, size_t cap, const char *stem)
{
    snprintf(out, cap, "track_light/%s.txt", stem);
}

int original_track_publish_plan(const char *dir, const char *data_dir, OriginalPublish *out, char *why, size_t cap)
{
    char stem[16];
    memset(out, 0, sizeof *out);
    if (!original_track_stem_of(dir, stem, sizeof stem)) return fail(why, cap, "%s is not an original track's folder", dir), 0;
    {
        OriginalPublishItem *it = &out->item[out->count];
        char mine[1100], theirs[1100];
        rel_of(it->file, sizeof it->file, stem);
        snprintf(mine, sizeof mine, "%s/light/%s", dir, it->file);
        snprintf(theirs, sizeof theirs, "%s/%s", data_dir, it->file);
        size_t lm, lt;
        unsigned char *m = slurp(mine, &lm);
        if (m == NULL) return fail(why, cap, "the project has no %s", it->file), 0;
        unsigned char *t = slurp(theirs, &lt);
        if (t == NULL) it->state = OT_NEW, ++out->added;
        else if (lm == lt && memcmp(m, t, lm) == 0) it->state = OT_SAME, ++out->same;
        else it->state = OT_CHANGED, ++out->changed, line_diff(m, t, &it->lines_added, &it->lines_removed);
        free(m), free(t);
        ++out->count;
    }
    return 1;
}

int original_track_publish(const char *dir, const char *data_dir, int *written, char *why, size_t cap)
{
    OriginalPublish plan;
    *written = 0;
    if (!original_track_publish_plan(dir, data_dir, &plan, why, cap)) return 0;
    for (int i = 0; i < plan.count; ++i) {
        if (plan.item[i].state == OT_SAME) continue;
        char mine[1100], theirs[1100], parent[1100];
        snprintf(mine, sizeof mine, "%s/light/%s", dir, plan.item[i].file);
        snprintf(theirs, sizeof theirs, "%s/%s", data_dir, plan.item[i].file);
        snprintf(parent, sizeof parent, "%s/%.*s", data_dir, (int)strcspn(plan.item[i].file, "/"), plan.item[i].file);
        size_t len;
        unsigned char *b = slurp(mine, &len);
        const int ok = b != NULL && make_dir(parent) && write_file(theirs, b, len);
        free(b);
        if (!ok) return fail(why, cap, "cannot write %s into the game's data", plan.item[i].file), 0;
        ++*written;
    }
    return 1;
}

void original_track_publish_text(const OriginalPublish *p, const char *stem, char *out, size_t cap)
{
    size_t n = (size_t)snprintf(out, cap, "%s:", stem);
    for (int i = 0; i < p->count && n < cap; ++i) {
        const OriginalPublishItem *it = &p->item[i];
        const int mask = strncmp(it->file, "track_masks/", 12) == 0;
        if (it->state == OT_CHANGED && it->lines_added < 0) n += (size_t)snprintf(out + n, cap - n, "\n  %s changed (size differs from the game's)", it->file);
        else if (it->state == OT_CHANGED) n += (size_t)snprintf(out + n, cap - n, "\n  %s changed (+%d -%d %s)", it->file, it->lines_added, it->lines_removed, mask ? "texels" : "lines");
        else if (it->state == OT_NEW) n += (size_t)snprintf(out + n, cap - n, "\n  %s new", it->file);
    }
    if (n < cap) snprintf(out + n, cap - n, "\n  %d unchanged", p->same);
}

/* ---- acceptance check ---- */

typedef struct { char path[600]; size_t size; } Entry;
typedef struct { Entry *v; int n, cap; } Entries;

static int collect_entries(const char *root, const char *rel, Entries *out)
{
    char dir[1400];
    snprintf(dir, sizeof dir, "%s/%s", root, rel);
    DIR *d = opendir(dir);
    if (d == NULL) return 1;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || strcmp(e->d_name, ".DS_Store") == 0) continue;
        char sub[600], full[2100];
        snprintf(sub, sizeof sub, "%s%s%s", rel, rel[0] != '\0' ? "/" : "", e->d_name);
        snprintf(full, sizeof full, "%s/%s", root, sub);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { collect_entries(root, sub, out); continue; }
        if (out->n == out->cap) {
            out->cap = out->cap != 0 ? out->cap * 2 : 64;
            out->v = realloc(out->v, (size_t)out->cap * sizeof *out->v);
            if (out->v == NULL) return closedir(d), 0;
        }
        snprintf(out->v[out->n].path, sizeof out->v[out->n].path, "%s", sub);
        out->v[out->n++].size = (size_t)st.st_size;
    }
    closedir(d);
    return 1;
}

static int derived(const char *path)
{
    return strncmp(path, "art-source/masks/", 17) == 0 || strncmp(path, "art-generated/track_masks/", 26) == 0;
}

static int entry_cmp(const void *x, const void *y) { return strcmp(((const Entry *)x)->path, ((const Entry *)y)->path); }

static int same_bytes(const char *a_root, const char *b_root, const char *rel)
{
    char pa[1400], pb[1400];
    snprintf(pa, sizeof pa, "%s/%s", a_root, rel);
    snprintf(pb, sizeof pb, "%s/%s", b_root, rel);
    size_t la, lb;
    unsigned char *ba = slurp(pa, &la), *bb = slurp(pb, &lb);
    const int same = ba != NULL && bb != NULL && la == lb && memcmp(ba, bb, la) == 0;
    free(ba), free(bb);
    return same;
}

int original_track_compare(const char *a, const char *b, char *report, size_t cap)
{
    Entries ea = { NULL, 0, 0 }, eb = { NULL, 0, 0 };
    int bad = 0, compared = 0, in_masks = 0;
    size_t n = 0;
    if (cap > 0) report[0] = '\0';
    collect_entries(a, "", &ea);
    collect_entries(b, "", &eb);
    for (int i = 0; i < ea.n; ++i) if (derived(ea.v[i].path)) ++in_masks;
    for (int i = 0; i < eb.n; ++i) if (derived(eb.v[i].path)) ++in_masks;
    qsort(ea.v, (size_t)ea.n, sizeof *ea.v, entry_cmp);
    qsort(eb.v, (size_t)eb.n, sizeof *eb.v, entry_cmp);
    for (int i = 0; i < ea.n; ++i) {
        if (derived(ea.v[i].path)) continue;
        const Entry *other = eb.v != NULL ? bsearch(&ea.v[i], eb.v, (size_t)eb.n, sizeof *eb.v, entry_cmp) : NULL;
        const char *what = other == NULL ? "missing in the hand-made folder" : !same_bytes(a, b, ea.v[i].path) ? "differs" : NULL;
        ++compared;
        if (what == NULL) continue;
        ++bad;
        if (n < cap) n += (size_t)snprintf(report + n, cap - n, "  %s: %s\n", ea.v[i].path, what);
    }
    for (int i = 0; i < eb.n; ++i) {
        if (derived(eb.v[i].path) || (ea.v != NULL && bsearch(&eb.v[i], ea.v, (size_t)ea.n, sizeof *ea.v, entry_cmp) != NULL)) continue;
        ++bad;
        if (n < cap) n += (size_t)snprintf(report + n, cap - n, "  %s: extra in the hand-made folder\n", eb.v[i].path);
    }
    if (n < cap) snprintf(report + n, cap - n, "%d files compared, %d in the mask folders (not compared)\n", compared, in_masks);
    free(ea.v), free(eb.v);
    return bad;
}

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

int original_track_acceptance(const char *dir, const char *stem, const char *data_dir, char *report, size_t cap, char *why, size_t why_cap)
{
    char scratch[1024], tmpl[1024];
    const char *base = getenv("TMPDIR");
    snprintf(tmpl, sizeof tmpl, "%s/original_accept_XXXXXX", base != NULL && base[0] != '\0' ? base : "/tmp");
    snprintf(scratch, sizeof scratch, "%s", tmpl);
    if (mkdtemp(scratch) == NULL) return fail(why, why_cap, "cannot make a scratch folder: %s", tmpl), -1;
    OriginalAdd add = { 0 };
    char handmade[1100];
    int result = -1;
    if (original_track_add(scratch, stem, data_dir, NULL, &add, why, why_cap) && original_track_dir(scratch, stem, handmade, sizeof handmade))
        result = original_track_compare(dir, handmade, report, cap);
    nftw(scratch, remove_entry, 8, FTW_DEPTH | FTW_PHYS);
    return result;
}

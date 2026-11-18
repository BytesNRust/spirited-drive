#define _POSIX_C_SOURCE 200809L   /* mkdir, rename, fork */
#include "build.h"

#include "formats/exchange.h"
#include "formats/mod_ini.h"
#include "formats/slots.h"
#include "formats/track_gen.h"
#include "formats/track_piece.h"
#include "formats/track_trace.h"
#include "formats/validate.h"
#include "formats/zip.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

enum {
    PATH_LEN = 4096,
    NAME_LEN = 256,
    TEMPLATE_FILES = 4         /* the piece's four files: the TRI, INFO and two day FAMs (from[0] is the template's TRI, read for the nodes) */
};

/* the file `rel` (its folders in any case) below `root`, as `out`; 0 when absent */
static int find_ci(const char *root, const char *rel, char *out, size_t cap)
{
    char cur[PATH_LEN];
    snprintf(cur, sizeof cur, "%s", root);
    const char *p = rel;
    for (;;) {
        const char *slash = strchr(p, '/');
        const size_t len = slash != NULL ? (size_t)(slash - p) : strlen(p);
        if (len == 0 || len >= NAME_LEN) return 0;
        char want[NAME_LEN], next[PATH_LEN];
        memcpy(want, p, len);
        want[len] = '\0';
        DIR *dir = opendir(cur);
        if (dir == NULL) return 0;
        int hit = 0;
        struct dirent *e;
        while (!hit && (e = readdir(dir)) != NULL) {
            if (strcasecmp(e->d_name, want) == 0) {
                snprintf(next, sizeof next, "%s/%s", cur, e->d_name);
                hit = 1;
            }
        }
        closedir(dir);
        if (!hit) return 0;
        snprintf(cur, sizeof cur, "%s", next);
        if (slash == NULL) break;
        p = slash + 1;
    }
    snprintf(out, cap, "%s", cur);
    return 1;
}

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long n = ftell(f);
        if (n >= 0 && fseek(f, 0, SEEK_SET) == 0) {
            data = malloc(n > 0 ? (size_t)n : 1);
            if (data != NULL && fread(data, 1, (size_t)n, f) == (size_t)n) {
                *size = (size_t)n;
            } else {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(f);
    return data;
}

static int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

/* the road's nodes, refused while one breaks a limit: the nodes are malloc'd into *nodes (count in *count) */
static int road_nodes(const Trace *t, TrackGenNode **nodes, int *count, char *why, size_t why_cap)
{
    ExchangeError err;
    memset(&err, 0, sizeof err);
    if (!track_trace_nodes(t->points, t->count, t->width, nodes, count, &err)) {
        snprintf(why, why_cap, "%s", err.message);
        return 0;
    }
    uint8_t *flags = calloc((size_t)*count, 1);
    if (flags == NULL) {
        free(*nodes);
        snprintf(why, why_cap, "out of memory");
        return 0;
    }
    track_trace_flags(*nodes, *count, flags);
    int ok = 1;
    for (int i = 0; ok && i < *count; ++i) {
        if (flags[i] & TRACE_FLAG_TURN) {
            snprintf(why, why_cap, "node %d turns more than %g degrees from the node before", i, TRACE_TURN_MAX_DEGREES);
            ok = 0;
        } else if (flags[i] & TRACE_FLAG_RADIUS) {
            snprintf(why, why_cap, "the curve at node %d is tighter than %g units", i, TRACE_RADIUS_MIN);
            ok = 0;
        }
    }
    free(flags);
    if (!ok) {
        free(*nodes);
        *nodes = NULL;
    }
    return ok;
}

/* the template's file `<stem>` + `suffix` (the CD's name, any case) into the piece's `dest`, read from the data folder */
static int copy_template(FileSet *set, const char *data_dir, const char *rel_template, const char *dest, char *why, size_t why_cap)
{
    char real[PATH_LEN];
    size_t size = 0;
    if (!find_ci(data_dir, rel_template, real, sizeof real)) {
        snprintf(why, why_cap, "no %s in the data folder %s: give the CD's folder with --data", rel_template, data_dir);
        return 0;
    }
    uint8_t *data = read_file(real, &size);
    if (data == NULL) {
        snprintf(why, why_cap, "%s: cannot read it", real);
        return 0;
    }
    const int ok = fileset_add(set, dest, data, size);
    free(data);
    if (!ok) snprintf(why, why_cap, "cannot add %s to the piece", dest);
    return ok;
}

/* the card of the piece, its keys the generator writes and the author line; 1 with `info` parsed */
static int piece_card(const Project *p, const char *piece, uint8_t *text, size_t cap, size_t *len, ModInfo *info, char *why, size_t why_cap)
{
    char head[MOD_INI_MAX_SIZE], texts[MOD_INI_MAX_SIZE];
    const char *name = p->name[0] != '\0' ? p->name : p->id;
    if (snprintf(head, sizeof head, "id=%s\nname=%s\nversion=0.1\ntype=track\nstem=%s\nkind=stage\nsegments=1\nroad=asphalt\nedge=grass\n",
                 p->id, name, piece) >= (int)sizeof head ||
        !mod_ini_text_keys(p->author, "", texts, sizeof texts)) {
        snprintf(why, why_cap, "the card does not fit mod.ini");
        return 0;
    }
    const int n = snprintf((char *)text, cap, "%s%s", head, texts);
    if (n < 0 || (size_t)n >= cap) {
        snprintf(why, why_cap, "the card does not fit mod.ini");
        return 0;
    }
    *len = (size_t)n;
    return mod_ini_parse(text, *len, info, why, why_cap);
}

/* the piece's files of `set` and its card, as the zip's entries (mod.ini first) in `pack` */
static int pack_piece(const FileSet *set, const uint8_t *card, size_t card_len, FileSet *pack, char *why, size_t why_cap)
{
    fileset_init(pack);
    if (!fileset_add(pack, "mod.ini", card, card_len)) {
        snprintf(why, why_cap, "cannot add mod.ini");
        return 0;
    }
    for (size_t i = 0; i < set->count; ++i) {
        if (!fileset_add(pack, set->file[i].name, set->file[i].data, set->file[i].size)) {
            snprintf(why, why_cap, "cannot add %s", set->file[i].name);
            return 0;
        }
    }
    return 1;
}

static int write_zip(const char *dir, const char *id, const FileSet *pack, char *zip, size_t zip_cap, char *why, size_t why_cap)
{
    size_t size = 0;
    uint8_t *bytes = zip_write_stored(pack, &size);
    if (bytes == NULL) {
        snprintf(why, why_cap, "the piece does not fit a zip");
        return 0;
    }
    char build[PATH_LEN], path[PATH_LEN], tmp[PATH_LEN];
    snprintf(build, sizeof build, "%s/build", dir);
    if (mkdir(build, 0777) != 0 && errno != EEXIST) {
        snprintf(why, why_cap, "%s: %s", build, strerror(errno));
        free(bytes);
        return 0;
    }
    snprintf(path, sizeof path, "%s/%s.zip", build, id);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    const int ok = write_file(tmp, bytes, size) && rename(tmp, path) == 0;
    free(bytes);
    if (!ok) {
        snprintf(why, why_cap, "%s: cannot write it", path);
        return 0;
    }
    snprintf(zip, zip_cap, "%s", path);
    return 1;
}

int build_project(const char *dir, const Project *p, const Trace *t, const char *data_dir, char *zip, size_t zip_cap, char *why,
                  size_t why_cap)
{
    if (p->redistributable) {
        snprintf(why, why_cap, "redistributable=1 needs the generic textures (CC0), not in this version: build with redistributable=0");
        return 0;
    }
    TrackGenNode *nodes = NULL;
    int count = 0;
    if (!road_nodes(t, &nodes, &count, why, why_cap)) return 0;
    char tstem[16];
    if (!slot_find(SLOT_TRACK, p->template, tstem, sizeof tstem)) {
        snprintf(why, why_cap, "'%s' is not a track slot", p->template);
        free(nodes);
        return 0;
    }
    char rel[PATH_LEN], real[PATH_LEN];
    snprintf(rel, sizeof rel, "SIMDATA/MISC/%s.TRI", tstem);
    size_t tsize = 0;
    uint8_t *tmpl = NULL;
    if (!find_ci(data_dir, rel, real, sizeof real) || (tmpl = read_file(real, &tsize)) == NULL) {
        snprintf(why, why_cap, "no %s in the data folder %s: give the CD's folder with --data", rel, data_dir);
        free(nodes);
        return 0;
    }
    ExchangeError err;
    memset(&err, 0, sizeof err);
    uint8_t *tri = NULL;
    size_t tri_len = 0;
    const int made = track_generate_nodes(nodes, count, tmpl, tsize, &tri, &tri_len, &err);
    free(tmpl);
    free(nodes);
    if (!made) {
        snprintf(why, why_cap, "%s", err.message);
        return 0;
    }
    char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH];
    track_piece_paths(p->stem, 1, paths);
    FileSet set;
    fileset_init(&set);
    int ok = fileset_add(&set, paths[0], tri, tri_len);
    free(tri);
    if (!ok) snprintf(why, why_cap, "cannot add %s", paths[0]);
    static const char *const from[TEMPLATE_FILES] = { "SIMDATA/MISC/%s.TRI", "SIMDATA/MISC/%sINFO", "SIMDATA/ETRACKFM/%s_001.FAM",
                                                       "SIMDATA/GTRACKFM/%s_001.FAM" };
    for (int i = 1; ok && i < TEMPLATE_FILES; ++i) {   /* paths[0] is the TRI built above */
        char rel_t[PATH_LEN];
        snprintf(rel_t, sizeof rel_t, from[i], tstem);
        ok = copy_template(&set, data_dir, rel_t, paths[i], why, why_cap);
    }
    uint8_t card[MOD_INI_MAX_SIZE];
    size_t card_len = 0;
    ModInfo info;
    if (ok) ok = piece_card(p, p->stem, card, sizeof card, &card_len, &info, why, why_cap);
    if (ok) {
        Report report;
        report_init(&report);
        if (!track_piece_check(&info, &set, NULL, &report)) {
            char text[512];
            report_text(&report, VALIDATE_COMPATIBLE, text, sizeof text);
            snprintf(why, why_cap, "the piece is not good: %s", text);
            ok = 0;
        }
        report_free(&report);
    }
    FileSet pack;
    if (ok) ok = pack_piece(&set, card, card_len, &pack, why, why_cap);
    if (ok) {
        ok = write_zip(dir, p->id, &pack, zip, zip_cap, why, why_cap);
        fileset_free(&pack);
    }
    fileset_free(&set);
    return ok;
}

int install_zip(const char *game, const char *zip, const char *user_dir, char *why, size_t why_cap)
{
#if defined(_WIN32)
    snprintf(why, why_cap, "install runs the game from a shell here: %s --user %s --install %s", game, user_dir, zip);
    return 0;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        snprintf(why, why_cap, "cannot start %s: %s", game, strerror(errno));
        return 0;
    }
    if (pid == 0) {
        execl(game, game, "--user", user_dir, "--install", zip, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        snprintf(why, why_cap, "waiting for %s failed", game);
        return 0;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 1;
    snprintf(why, why_cap, "%s --install stopped with status %d", game, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return 0;
#endif
}

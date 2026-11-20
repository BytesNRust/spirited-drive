#define _POSIX_C_SOURCE 200809L   /* mkdir, rename, fork */
#include "build.h"

#include "formats/exchange.h"
#include "formats/mod_ini.h"
#include "formats/track_gen.h"
#include "formats/track_piece.h"
#include "formats/track_template.h"
#include "formats/track_trace.h"
#include "formats/validate.h"
#include "formats/zip.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

enum {
    PATH_LEN = 4096,
    INFO_LINES = 29,          /* the INFO file's data lines (validate_misc.c); lines 9 and 10 are the sky colours, three values each */
    INFO_SKY_TOP = 9,
    INFO_SKY_BOTTOM = 10,
    INFO_TEXT_MAX = 512
};

/* The INFO text of a generic piece: 29 data lines, the sky colours a b c (0..31, packed by the game) and the rest zero. The values are
 * not the CD's: the sky is a plain blue until a track sets its own (docs/track-editor.md). */
static const char *const SKY_TOP = "12 20 31", *const SKY_BOTTOM = "20 28 31";

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
    if (!(t->width >= PROJECT_WIDTH_MIN && t->width <= PROJECT_WIDTH_MAX)) {
        snprintf(why, why_cap, "width %g is outside %d..%d", t->width, PROJECT_WIDTH_MIN, PROJECT_WIDTH_MAX);
        return 0;
    }
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
            snprintf(why, why_cap, "the curve at node %d is tighter than %g m", i, TRACE_METRES(TRACE_RADIUS_MIN));
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

/* the card of the piece, its keys the generator writes and the author line; 1 with `info` parsed */
int build_card(const Project *p, uint8_t *text, size_t cap, size_t *len, ModInfo *info, char *why, size_t why_cap)
{
    char head[MOD_INI_MAX_SIZE], texts[MOD_INI_MAX_SIZE];
    const char *name = p->name[0] != '\0' ? p->name : p->id;
    int edge_known = p->edge[0] == '\0';
    for (int i = 0; PROJECT_EDGES[i] != NULL; ++i) edge_known |= strcmp(p->edge, PROJECT_EDGES[i]) == 0;
    if (!edge_known) {
        snprintf(why, why_cap, "edge=%s is not grass, gravel, sand, snow, dirt or water", p->edge);
        return 0;
    }
    if (snprintf(head, sizeof head, "id=%s\nname=%s\nversion=0.1\ntype=track\nstem=%s\nkind=stage\nsegments=1\nroad=asphalt\nedge=%s\n",
                 p->id, name, p->stem, p->edge[0] != '\0' ? p->edge : "grass") >= (int)sizeof head ||
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

/* the piece's four files into `set`: the TRI made from the road, the INFO, and the stage's art (a FAM, read from `art`) as the two day
 * FAMs of the game's two languages */
static int piece_files(const Project *p, const char *dir, const uint8_t *tri, size_t tri_len, FileSet *set, char *why, size_t why_cap)
{
    char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH], art[PATH_LEN];
    track_piece_paths(p->stem, 1, paths);
    if (p->art[0] == '\0') {
        snprintf(why, why_cap, "set art= in project.ini: a stage's FAM (a new project copies the editor's generic one as GENERIC.FAM; this editor has none beside it)");
        return 0;
    }
    if (p->art[0] == '/') snprintf(art, sizeof art, "%s", p->art);
    else snprintf(art, sizeof art, "%s/%s", dir, p->art);
    size_t art_len = 0;
    uint8_t *fam = read_file(art, &art_len);
    if (fam == NULL) {
        snprintf(why, why_cap, "%s: cannot read the art", art);
        return 0;
    }
    char info_text[INFO_TEXT_MAX];
    int ok = fileset_add(set, paths[0], tri, tri_len) && fileset_add(set, paths[2], fam, art_len) && fileset_add(set, paths[3], fam, art_len);
    free(fam);
    if (!ok) snprintf(why, why_cap, "cannot add the piece's files");
    if (ok) {
        int n = 0;
        for (int i = 0; i < INFO_LINES; ++i) {
            const char *line = i == INFO_SKY_TOP - 1 ? SKY_TOP : i == INFO_SKY_BOTTOM - 1 ? SKY_BOTTOM : "0";
            n += snprintf(info_text + n, sizeof info_text - (size_t)n, "%s\n", line);
        }
        ok = fileset_add(set, paths[1], (const uint8_t *)info_text, (size_t)n);
        if (!ok) snprintf(why, why_cap, "cannot add %s", paths[1]);
    }
    return ok;
}

int build_project(const char *dir, const Project *p, const Trace *t, char *zip, size_t zip_cap, char *why, size_t why_cap)
{
    TrackGenNode *nodes = NULL;
    int count = 0;
    if (!road_nodes(t, &nodes, &count, why, why_cap)) return 0;
    size_t tmpl_len = 0;
    uint8_t *tmpl = NULL;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    int ok = track_template_generic(&tmpl, &tmpl_len, &err);
    uint8_t *tri = NULL;
    size_t tri_len = 0;
    if (ok) ok = track_generate_nodes_turned(nodes, count, tmpl, tmpl_len, &tri, &tri_len, &err);
    if (!ok) snprintf(why, why_cap, "%s", err.message);
    free(tmpl);
    free(nodes);
    FileSet set;
    fileset_init(&set);
    if (ok) ok = piece_files(p, dir, tri, tri_len, &set, why, why_cap);
    free(tri);
    uint8_t card[MOD_INI_MAX_SIZE];
    size_t card_len = 0;
    ModInfo info;
    if (ok) ok = build_card(p, card, sizeof card, &card_len, &info, why, why_cap);
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

void install_command_text(const char *game, const char *zip, const char *user_dir, char *out, size_t cap)
{
    if (user_dir[0] != '\0') snprintf(out, cap, "%s --user %s --install %s", game, user_dir, zip);
    else snprintf(out, cap, "%s --install %s", game, zip);
}

int install_zip(const char *game, const char *zip, const char *user_dir, char *why, size_t why_cap)
{
#if defined(_WIN32)
    char command[2 * PATH_LEN + 64];
    install_command_text(game, zip, user_dir, command, sizeof command);
    snprintf(why, why_cap, "install runs the game from a shell here: %s", command);
    return 0;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        snprintf(why, why_cap, "cannot start %s: %s", game, strerror(errno));
        return 0;
    }
    if (pid == 0) {
        if (user_dir[0] != '\0') execl(game, game, "--user", user_dir, "--install", zip, (char *)NULL);
        else execl(game, game, "--install", zip, (char *)NULL);   /* the game's own player folder */
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

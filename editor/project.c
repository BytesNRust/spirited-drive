#define _POSIX_C_SOURCE 200809L   /* mkdir, rename */
#include "project.h"

#include "formats/track_piece.h"
#include "game/sys/sha256.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    DEFAULT_WIDTH = 40,
    PATH_MAX_LEN = 4096
};

#define GENERIC_ART "GENERIC.FAM"
#define LEGACY_ART "../generic-straight/GENERIC.FAM"

static char *read_text(const char *path, size_t *size, char *why, size_t why_cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        snprintf(why, why_cap, "%s: %s", path, strerror(errno));
        return NULL;
    }
    char *text = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long n = ftell(f);
        if (n >= 0 && fseek(f, 0, SEEK_SET) == 0) {
            text = malloc((size_t)n + 1);
            if (text != NULL && fread(text, 1, (size_t)n, f) == (size_t)n) {
                text[n] = '\0';
                *size = (size_t)n;
            } else {
                free(text);
                text = NULL;
            }
        }
    }
    fclose(f);
    if (text == NULL) snprintf(why, why_cap, "%s: cannot read it", path);
    return text;
}

static int write_text(const char *path, const char *text, char *why, size_t why_cap)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        snprintf(why, why_cap, "%s: %s", path, strerror(errno));
        return 0;
    }
    const size_t n = strlen(text);
    const int ok = fwrite(text, 1, n, f) == n;
    if (fclose(f) != 0 || !ok) {
        snprintf(why, why_cap, "%s: cannot write it", path);
        return 0;
    }
    return 1;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    char *start = s;
    while (*start != '\0' && isspace((unsigned char)*start)) ++start;
    memmove(s, start, strlen(start) + 1);
}

/* the value of key `key` in project.ini text, copied to out; 1 when given */
static int ini_value(const char *text, const char *key, char *out, size_t cap)
{
    const size_t n = strlen(key);
    for (const char *line = text; *line != '\0';) {
        const char *end = strchr(line, '\n');
        const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        if (len > n && strncmp(line, key, n) == 0 && line[n] == '=') {
            char buf[PROJECT_TEXT_MAX * 2];
            const size_t take = len - n - 1 < sizeof buf - 1 ? len - n - 1 : sizeof buf - 1;
            memcpy(buf, line + n + 1, take);
            buf[take] = '\0';
            trim(buf);
            snprintf(out, cap, "%s", buf);
            return 1;
        }
        if (end == NULL) break;
        line = end + 1;
    }
    return 0;
}

const char *const PROJECT_EDGES[] = { "grass", "gravel", "sand", "snow", "dirt", "water", NULL };

static int edge_ok(const char *word)
{
    for (int i = 0; PROJECT_EDGES[i] != NULL; ++i)
        if (strcmp(word, PROJECT_EDGES[i]) == 0) return 1;
    return 0;
}

static int parse_project(const char *text, Project *p, char *why, size_t why_cap)
{
    memset(p, 0, sizeof *p);
    if (!ini_value(text, "id", p->id, sizeof p->id) || !mod_ini_id_ok(p->id)) {
        snprintf(why, why_cap, "project.ini: id must be given, of a-z 0-9 and '-', up to %d", MOD_INI_ID_MAX);
        return 0;
    }
    if (!ini_value(text, "name", p->name, sizeof p->name)) snprintf(p->name, sizeof p->name, "%s", p->id);
    ini_value(text, "author", p->author, sizeof p->author);
    if (!ini_value(text, "stem", p->stem, sizeof p->stem)) snprintf(p->stem, sizeof p->stem, "EDIT");
    if (!track_piece_stem_ok(p->stem)) {
        snprintf(why, why_cap, "project.ini: stem '%s' must be 1..%d of A-Z and 0-9, not beginning AL, CL, CY or TR", p->stem, TRACK_PIECE_STEM_MAX);
        return 0;
    }
    ini_value(text, "art", p->art, sizeof p->art);
    char flag[8] = "0";
    ini_value(text, "redistributable", flag, sizeof flag);
    p->redistributable = strcmp(flag, "1") == 0;
    char edge[PROJECT_TEXT_MAX] = "grass";
    ini_value(text, "edge", edge, sizeof edge);
    if (!edge_ok(edge)) {
        snprintf(why, why_cap, "project.ini: edge=%s is not grass, gravel, sand, snow, dirt or water", edge);
        return 0;
    }
    snprintf(p->edge, sizeof p->edge, "%s", edge);
    return 1;
}

/* the numbers of the JSON array `[[x, z], ...]` after "points", pairs into trace; 1 when well formed */
static int parse_points(const char *text, Trace *t, char *why, size_t why_cap)
{
    const char *p = strstr(text, "\"points\"");
    p = p != NULL ? strchr(p, '[') : NULL;
    if (p == NULL) {
        snprintf(why, why_cap, "trace.json: no \"points\" array");
        return 0;
    }
    ++p;
    int cap = 0, closed = 0;
    t->points = NULL;
    t->count = 0;
    for (;;) {
        while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) ++p;
        if (*p == ']') {
            closed = 1;
            break;
        }
        if (*p != '[') break;
        ++p;
        char *end = NULL;
        const double x = strtod(p, &end);
        if (end == p) break;
        p = end;
        while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) ++p;
        const double z = strtod(p, &end);
        if (end == p) break;
        p = end;
        while (isspace((unsigned char)*p)) ++p;
        if (*p != ']') break;
        ++p;
        if (t->count == cap) {
            const int grown = cap == 0 ? 16 : cap * 2;
            TracePoint *more = realloc(t->points, sizeof *more * (size_t)grown);
            if (more == NULL) {
                snprintf(why, why_cap, "trace.json: out of memory");
                free(t->points);
                t->points = NULL;
                return 0;
            }
            t->points = more;
            cap = grown;
        }
        t->points[t->count].x = x;
        t->points[t->count].z = z;
        ++t->count;
        if (t->count > PROJECT_POINTS_MAX) break;
    }
    if (!closed || t->count < 2 || t->count > PROJECT_POINTS_MAX) {
        snprintf(why, why_cap, "trace.json: the points array must hold 2..%d [x, z] pairs and close", PROJECT_POINTS_MAX);
        free(t->points);
        t->points = NULL;
        t->count = 0;
        return 0;
    }
    return 1;
}

const char *const PROJECT_ROAD_TIP = "Width of the tarmac, edge to edge. Beyond it the surface changes and the walls start further out. Default 10 m. A width set on a point in Points takes over from there.";

double project_road_metres(double width_byte)
{
    return width_byte / 4.0;
}

double project_width_from_road(double metres)
{
    return metres * 4.0;
}

static int parse_width(const char *text, double *width, char *why, size_t why_cap)
{
    const char *p = strstr(text, "\"width\"");
    p = p != NULL ? strchr(p, ':') : NULL;
    char *end = NULL;
    const double w = p != NULL ? strtod(p + 1, &end) : DEFAULT_WIDTH;
    if (p != NULL && end == p + 1) {
        snprintf(why, why_cap, "trace.json: width is not a number");
        return 0;
    }
    if (w < PROJECT_WIDTH_MIN || w > PROJECT_WIDTH_MAX) {
        snprintf(why, why_cap, "trace.json: width %g is outside %d..%d", w, PROJECT_WIDTH_MIN, PROJECT_WIDTH_MAX);
        return 0;
    }
    *width = w;
    return 1;
}

/* trace.json's version: none or 1 is the first Studio's, 2 has the road keys, 3 their shoulder surfaces, a newer one was written by a Studio that knows more than this one */
static int parse_version(const char *text, int *version, char *why, size_t why_cap)
{
    *version = 1;
    const char *p = strstr(text, "\"version\"");
    p = p != NULL ? strchr(p, ':') : NULL;
    if (p == NULL) return 1;
    char *end = NULL;
    const double v = strtod(p + 1, &end);
    if (end == p + 1 || v < 1 || v != floor(v)) {
        snprintf(why, why_cap, "trace.json: version is not a whole number from 1");
        return 0;
    }
    if (v > PROJECT_TRACE_VERSION) {
        snprintf(why, why_cap, "trace.json: version %.0f is newer than the version %d this Studio reads: update Studio", v, PROJECT_TRACE_VERSION);
        return 0;
    }
    *version = (int)v;
    return 1;
}

/* a number after "name" inside the object text `obj`; 1 when there is one */
static int object_number(const char *obj, const char *name, double *value)
{
    char key[32];
    snprintf(key, sizeof key, "\"%s\"", name);
    const char *p = strstr(obj, key);
    p = p != NULL ? strchr(p, ':') : NULL;
    if (p == NULL) return 0;
    char *end = NULL;
    *value = strtod(p + 1, &end);
    return end != p + 1;
}

/* a string after "name" inside the object text `obj` into `out` (cap bytes): 1 when there is one, 0 when the name is not there, -1 when its value is no string */
static int object_string(const char *obj, const char *name, char *out, size_t cap)
{
    char key[40];
    snprintf(key, sizeof key, "\"%s\"", name);
    const char *p = strstr(obj, key);
    p = p != NULL ? strchr(p + strlen(key), ':') : NULL;
    if (p == NULL) return 0;
    ++p;
    while (isspace((unsigned char)*p)) ++p;
    const char *end = *p == '"' ? strchr(p + 1, '"') : NULL;
    if (end == NULL || (size_t)(end - p - 1) >= cap) return -1;
    memcpy(out, p + 1, (size_t)(end - p - 1));
    out[end - p - 1] = '\0';
    return 1;
}

/* a side's shoulder surface of a key (version 3): "verge" or "tarmac"; none is TRACE_SURFACE_INHERIT. 0 with `why` for another value */
static int parse_surface(const char *obj, const char *name, double point, TraceSurface *out, char *why, size_t why_cap)
{
    char value[16];
    const int found = object_string(obj, name, value, sizeof value);
    *out = TRACE_SURFACE_INHERIT;
    if (found == 0) return 1;
    if (found > 0 && strcmp(value, "verge") == 0) *out = TRACE_SURFACE_VERGE;
    else if (found > 0 && strcmp(value, "tarmac") == 0) *out = TRACE_SURFACE_TARMAC;
    else {
        snprintf(why, why_cap, "trace.json: the road key on point %.0f has %s \"%s\": it is \"verge\" or \"tarmac\"", point, name, found > 0 ? value : "(not a string)");
        return 0;
    }
    return 1;
}

/* the objects of the JSON array after "keys" (version 2 on): [{"point": 3, "width_m": 14.0, "shoulder_l_surface": "tarmac"}, ...] into trace->keys; the
 * surfaces are version 3's. A field this version does not know is left alone (the next steps' fields are additive); a key that sets nothing this version
 * knows is no key. 1 when well formed and every key is on a point and in the road's range */
static int parse_keys(const char *text, int version, Trace *t, char *why, size_t why_cap)
{
    const char *p = strstr(text, "\"keys\"");
    p = p != NULL ? strchr(p, '[') : NULL;
    if (p == NULL) return 1;
    ++p;
    for (;;) {
        while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) ++p;
        if (*p == ']') return 1;
        const char *close = *p == '{' ? strchr(p, '}') : NULL;
        if (close == NULL || (size_t)(close - p) >= 256) {
            snprintf(why, why_cap, "trace.json: the keys array must hold {...} objects and close");
            return 0;
        }
        char obj[256];
        memcpy(obj, p, (size_t)(close - p));
        obj[close - p] = '\0';
        p = close + 1;
        double point = 0, metres = 0;
        const int has_width = object_number(obj, "width_m", &metres);
        if (!object_number(obj, "point", &point) || point != floor(point)) {
            snprintf(why, why_cap, "trace.json: a road key has no point number");
            return 0;
        }
        TraceSurface left = TRACE_SURFACE_INHERIT, right = TRACE_SURFACE_INHERIT;
        if (version >= 3 && (!parse_surface(obj, "shoulder_l_surface", point, &left, why, why_cap) || !parse_surface(obj, "shoulder_r_surface", point, &right, why, why_cap)))
            return 0;
        const int has_surface = left != TRACE_SURFACE_INHERIT || right != TRACE_SURFACE_INHERIT;
        if (!has_width && !has_surface) continue;
        if (point < 0 || point >= t->count) {
            snprintf(why, why_cap, "trace.json: a road key is on point %.0f, and the trace has points 0..%d", point, t->count - 1);
            return 0;
        }
        if (has_width && !(metres >= TRACE_ROAD_MIN_M && metres <= TRACE_ROAD_MAX_M)) {
            snprintf(why, why_cap, "trace.json: the road key on point %.0f has width %g m, outside %g..%g m", point, metres, TRACE_ROAD_MIN_M, TRACE_ROAD_MAX_M);
            return 0;
        }
        if (track_trace_key_find(t->keys, t->key_count, (int)point) >= 0) {
            snprintf(why, why_cap, "trace.json: point %.0f has a road key twice", point);
            return 0;
        }
        if ((has_width && !track_trace_key_set_width(&t->keys, &t->key_count, &t->key_cap, (int)point, metres)) ||
            !track_trace_key_set_surface(&t->keys, &t->key_count, &t->key_cap, (int)point, TRACK_GEN_LEFT, left) ||
            !track_trace_key_set_surface(&t->keys, &t->key_count, &t->key_cap, (int)point, TRACK_GEN_RIGHT, right)) {
            snprintf(why, why_cap, "trace.json: out of memory");
            return 0;
        }
    }
}

int project_open(const char *dir, Project *out, Trace *trace, char *why, size_t why_cap)
{
    char path[PATH_MAX_LEN];
    size_t size = 0;
    snprintf(path, sizeof path, "%s/project.ini", dir);
    char *ini = read_text(path, &size, why, why_cap);
    if (ini == NULL) return 0;
    const int ok_ini = parse_project(ini, out, why, why_cap);
    free(ini);
    if (!ok_ini) return 0;
    snprintf(path, sizeof path, "%s/trace.json", dir);
    char *json = read_text(path, &size, why, why_cap);
    if (json == NULL) return 0;
    trace->points = NULL;
    trace->count = 0;
    trace->keys = NULL;
    trace->key_count = trace->key_cap = 0;
    int version = 1;
    const int ok = parse_version(json, &version, why, why_cap) && parse_width(json, &trace->width, why, why_cap) && parse_points(json, trace, why, why_cap) &&
                   (version < 2 || parse_keys(json, version, trace, why, why_cap));   // keys are version 2's: an older file with a "keys" array has none
    free(json);
    if (!ok) {   // what was read before the fault goes with it
        free(trace->points);
        free(trace->keys);
        trace->points = NULL;
        trace->keys = NULL;
        trace->count = trace->key_count = trace->key_cap = 0;
    }
    return ok;
}


/* `text` into `path` through a temporary file, then renamed: a write that fails leaves the old file as it was, not a short one */
static int write_atomic(const char *path, const void *bytes, size_t size, char *why, size_t why_cap)
{
    char tmp[PATH_MAX_LEN];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    const int ok = f != NULL && fwrite(bytes, 1, size, f) == size;
    const int closed = f == NULL || fclose(f) == 0;
    if (!ok || !closed || rename(tmp, path) != 0) {
        snprintf(why, why_cap, "%s: cannot write it", path);
        remove(tmp);
        return 0;
    }
    return 1;
}

static int copy_file(const char *from, const char *to, char *why, size_t why_cap)
{
    size_t size = 0;
    char *bytes = read_text(from, &size, why, why_cap);   /* the art is binary: read_text keeps every byte (and adds a NUL beyond them) */
    if (bytes == NULL) return 0;
    const int ok = write_atomic(to, bytes, size, why, why_cap);
    free(bytes);
    return ok;
}

/* the start of the line of `ini` that begins with `key` (key and '='), or NULL */
static char *line_of(char *ini, const char *key)
{
    const size_t n = strlen(key);
    for (char *line = ini; *line != '\0';) {
        if (strncmp(line, key, n) == 0 && line[n] == '=') return line;
        char *end = strchr(line, '\n');
        if (end == NULL) break;
        line = end + 1;
    }
    return NULL;
}

/* the project's own copy of the generic art, made when it is not there: `art` (project.ini's art=) is empty, names it as GENERIC.FAM, or names the
 * ../generic-straight/GENERIC.FAM the first Studio wrote, which only the source tree had. The author's own art= is never touched. */
static int adopt_art(const char *dir, const char *ini_path, const char *ini, const char *generic, char *why, size_t why_cap)
{
    char art[PROJECT_PATH_MAX], path[PATH_MAX_LEN];
    if (!ini_value(ini, "art", art, sizeof art)) return 1;
    if (art[0] != '\0' && strcmp(art, GENERIC_ART) != 0 && strcmp(art, LEGACY_ART) != 0) return 1;
    snprintf(path, sizeof path, "%s/%s", dir, art[0] != '\0' ? art : GENERIC_ART);
    struct stat st;
    if (stat(path, &st) == 0) return 1;
    if (generic == NULL) return 1;   /* the build says the art cannot be read */
    snprintf(path, sizeof path, "%s/%s", dir, GENERIC_ART);
    if (stat(path, &st) != 0 && !copy_file(generic, path, why, why_cap)) return 0;
    if (strcmp(art, GENERIC_ART) == 0) return 1;
    char *copy = strdup(ini);
    char *line = copy != NULL ? line_of(copy, "art") : NULL;
    if (line == NULL) {
        free(copy);
        snprintf(why, why_cap, "%s: no art= line to give the copied art to", ini_path);
        return 0;
    }
    char *end = strchr(line, '\n');
    const size_t cap = strlen(ini) + sizeof GENERIC_ART;
    char *text = malloc(cap);
    if (text == NULL) {
        free(copy);
        snprintf(why, why_cap, "out of memory");
        return 0;
    }
    const int n = snprintf(text, cap, "%.*sart=%s%s", (int)(line - copy), copy, GENERIC_ART, end != NULL ? end : "");
    const int ok = n > 0 && write_atomic(ini_path, text, (size_t)n, why, why_cap);
    free(text);
    free(copy);
    return ok;
}

/* the SHA-256 of every GENERIC.FAM Studio carried before the one it carries now (the road lines' shades, editor/art/README.md): a project's copy that is
 * byte for byte one of these is the author's untouched copy of an old Studio's, and is replaced */
const char *const PROJECT_OLD_ARTS[] = { "ad2a9b645848ad93fcb939ac954baf6c543233a28db898b58ab708049617e59d", NULL };

static void sha256_of(const void *bytes, size_t size, char out[65])
{
    Sha256 s;
    sha256_init(&s);
    sha256_update(&s, bytes, size);
    sha256_hex(&s, out);
}

int project_art_refresh(const char *dir, const char *generic, const char *const old[], char *note, size_t note_cap)
{
    note[0] = '\0';
    if (generic == NULL) return 0;
    char path[PATH_MAX_LEN], why[160], art[PROJECT_PATH_MAX];
    snprintf(path, sizeof path, "%s/project.ini", dir);
    size_t size = 0, copy_size = 0, bundled_size = 0;
    char *ini = read_text(path, &size, why, sizeof why);
    const int own = ini != NULL && ini_value(ini, "art", art, sizeof art) && strcmp(art, GENERIC_ART) == 0;   /* an art= of the author's own is never touched */
    free(ini);
    if (!own) return 0;
    snprintf(path, sizeof path, "%s/%s", dir, GENERIC_ART);
    char *copy = read_text(path, &copy_size, why, sizeof why), *bundled = read_text(generic, &bundled_size, why, sizeof why);
    int replaced = 0;
    if (copy != NULL && bundled != NULL && (copy_size != bundled_size || memcmp(copy, bundled, copy_size) != 0)) {
        char hex[65];
        sha256_of(copy, copy_size, hex);
        int known = 0;
        for (int i = 0; old != NULL && old[i] != NULL; i++) known |= strcmp(old[i], hex) == 0;
        if (known && write_atomic(path, bundled, bundled_size, why, sizeof why)) {
            snprintf(note, note_cap, "art: GENERIC.FAM was the one of an older Studio and is now the current one");
            replaced = 1;
        } else if (!known) {
            snprintf(note, note_cap, "art: this project's GENERIC.FAM is not Studio's (edited, or from another version): left as it is");
        }
    }
    free(copy);
    free(bundled);
    return replaced;
}

static int make_dir(const char *dir, char *why, size_t why_cap)
{
    if (mkdir(dir, 0777) == 0 || errno == EEXIST) return 1;
    snprintf(why, why_cap, "%s: %s", dir, strerror(errno));
    return 0;
}

int project_create(const char *dir, const char *id, const char *generic_art, char *why, size_t why_cap)
{
    if (!mod_ini_id_ok(id)) {
        snprintf(why, why_cap, "id '%s' must be of a-z 0-9 and '-', up to %d, not first", id, MOD_INI_ID_MAX);
        return 0;
    }
    if (!make_dir(dir, why, why_cap)) return 0;
    char path[PATH_MAX_LEN], text[1024];
    snprintf(path, sizeof path, "%s/project.ini", dir);
    struct stat st;
    if (stat(path, &st) != 0) {
        snprintf(text, sizeof text,
                 "# Spirited Drive track project: the source. build/ is regenerated by the track editor.\n"
                 "id=%s\nname=%s\nauthor=\nstem=EDIT\nart=%s\nredistributable=1\n", id, id, generic_art != NULL ? GENERIC_ART : "");
        if (!write_text(path, text, why, why_cap)) return 0;
    }
    char ini_path[PATH_MAX_LEN];
    snprintf(ini_path, sizeof ini_path, "%s", path);
    size_t ini_size = 0;
    char *ini = read_text(ini_path, &ini_size, why, why_cap);
    if (ini == NULL) return 0;
    const int adopted = adopt_art(dir, ini_path, ini, generic_art, why, why_cap);
    free(ini);
    if (!adopted) return 0;
    snprintf(path, sizeof path, "%s/trace.json", dir);
    if (stat(path, &st) != 0) {
        Trace straight = { NULL, PROJECT_START_POINTS, DEFAULT_WIDTH, NULL, 0, 0 };
        TracePoint pts[PROJECT_START_POINTS];
        for (int i = 0; i < PROJECT_START_POINTS; ++i) {
            pts[i].x = 0;
            pts[i].z = (double)PROJECT_START_LENGTH * i / (PROJECT_START_POINTS - 1);
        }
        straight.points = pts;
        return project_save_trace(dir, &straight, why, why_cap);
    }
    return 1;
}

int project_set_edge(const char *dir, const char *edge, char *why, size_t why_cap)
{
    if (!edge_ok(edge)) {
        snprintf(why, why_cap, "project.ini: edge=%s is not grass, gravel, sand, snow, dirt or water", edge);
        return 0;
    }
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof path, "%s/project.ini", dir);
    size_t size = 0;
    char *ini = read_text(path, &size, why, why_cap);
    if (ini == NULL) return 0;
    const char *line = line_of(ini, "edge");
    const char *end = line != NULL ? strchr(line, '\n') : NULL;
    const size_t cap = size + 32;
    char *text = malloc(cap);
    if (text == NULL) {
        free(ini);
        snprintf(why, why_cap, "out of memory");
        return 0;
    }
    int n;
    if (line != NULL) n = snprintf(text, cap, "%.*sedge=%s%s", (int)(line - ini), ini, edge, end != NULL ? end : "");
    else n = snprintf(text, cap, "%s%sedge=%s\n", ini, size > 0 && ini[size - 1] != '\n' ? "\n" : "", edge);
    const int ok = n > 0 && write_atomic(path, text, (size_t)n, why, why_cap);
    free(text);
    free(ini);
    return ok;
}

/* the fields a key sets, each ", \"name\": value", into `out`; 0 when they do not fit */
static int key_fields(const TraceKey *k, char *out, size_t cap)
{
    size_t at = 0;
    out[0] = '\0';
    if (k->has & TRACE_KEY_WIDTH) {
        const int n = snprintf(out + at, cap - at, ", \"width_m\": %.2f", k->width_m);
        if (n < 0 || (size_t)n >= cap - at) return 0;
        at += (size_t)n;
    }
    const TraceSurface sides[2] = { k->shoulder_l_surface, k->shoulder_r_surface };
    static const char *const NAMES[2] = { "shoulder_l_surface", "shoulder_r_surface" };
    for (int side = 0; side < 2 && (k->has & TRACE_KEY_SHOULDER_SURFACE); ++side) {
        if (sides[side] == TRACE_SURFACE_INHERIT) continue;
        const int n = snprintf(out + at, cap - at, ", \"%s\": \"%s\"", NAMES[side], sides[side] == TRACE_SURFACE_TARMAC ? "tarmac" : "verge");
        if (n < 0 || (size_t)n >= cap - at) return 0;
        at += (size_t)n;
    }
    return 1;
}

int project_save_trace(const char *dir, const Trace *trace, char *why, size_t why_cap)
{
    if (trace->count < 2) {
        snprintf(why, why_cap, "a trace has 2 points or more");
        return 0;
    }
    if (!(trace->width >= PROJECT_WIDTH_MIN && trace->width <= PROJECT_WIDTH_MAX)) {   /* what parse_width would refuse on the way back */
        snprintf(why, why_cap, "trace.json: width %g is outside %d..%d", trace->width, PROJECT_WIDTH_MIN, PROJECT_WIDTH_MAX);
        return 0;
    }
    enum { KEY_TEXT = 160 };   /* the longest key this Studio writes is about 110 bytes (and under the reader's 256): {"point": 599, "width_m": 30.00, both surfaces} */
    const size_t cap = 256 + (size_t)trace->count * 40 + (size_t)trace->key_count * KEY_TEXT;
    int version = 2;
    for (int i = 0; i < trace->key_count; ++i)
        if (trace->keys[i].has & TRACE_KEY_SHOULDER_SURFACE) version = 3;
    char *text = malloc(cap);
    if (text == NULL) {
        snprintf(why, why_cap, "out of memory");
        return 0;
    }
    size_t at = (size_t)snprintf(text, cap, "{\n  \"version\": %d,\n  \"width\": %.1f,\n  \"points\": [\n", version, trace->width);
    for (int i = 0; i < trace->count; ++i)
        at += (size_t)snprintf(text + at, cap - at, "    [%.3f, %.3f]%s\n", trace->points[i].x, trace->points[i].z,
                               i + 1 < trace->count ? "," : "");
    if (trace->key_count == 0) {
        snprintf(text + at, cap - at, "  ]\n}\n");
    } else {
        at += (size_t)snprintf(text + at, cap - at, "  ],\n  \"keys\": [\n");
        for (int i = 0; i < trace->key_count && at < cap; ++i) {
            char fields[KEY_TEXT] = "";
            if (!key_fields(&trace->keys[i], fields, sizeof fields)) {
                free(text);
                snprintf(why, why_cap, "trace.json: the road key on point %d does not fit its line", trace->keys[i].point);
                return 0;
            }
            at += (size_t)snprintf(text + at, cap - at, "    {\"point\": %d%s}%s\n", trace->keys[i].point, fields, i + 1 < trace->key_count ? "," : "");
        }
        snprintf(text + at, cap - at, "  ]\n}\n");
    }
    char path[PATH_MAX_LEN], tmp[PATH_MAX_LEN];
    snprintf(path, sizeof path, "%s/trace.json", dir);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    int ok = write_text(tmp, text, why, why_cap);
    if (ok && rename(tmp, path) != 0) {
        snprintf(why, why_cap, "%s: %s", path, strerror(errno));
        ok = 0;
    }
    free(text);
    return ok;
}

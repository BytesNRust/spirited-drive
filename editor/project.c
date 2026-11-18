#define _POSIX_C_SOURCE 200809L   /* mkdir, rename */
#include "project.h"

#include "formats/track_piece.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    DEFAULT_WIDTH = 40,
    PATH_MAX_LEN = 4096
};

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
    const int ok = parse_width(json, &trace->width, why, why_cap) && parse_points(json, trace, why, why_cap);
    free(json);
    return ok;
}

static int make_dir(const char *dir, char *why, size_t why_cap)
{
    if (mkdir(dir, 0777) == 0 || errno == EEXIST) return 1;
    snprintf(why, why_cap, "%s: %s", dir, strerror(errno));
    return 0;
}

int project_create(const char *dir, const char *id, char *why, size_t why_cap)
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
                 "id=%s\nname=%s\nauthor=\nstem=EDIT\nart=../generic-straight/GENERIC.FAM\nredistributable=1\n", id, id);
        if (!write_text(path, text, why, why_cap)) return 0;
    }
    snprintf(path, sizeof path, "%s/trace.json", dir);
    if (stat(path, &st) != 0) {
        Trace straight = { NULL, PROJECT_START_POINTS, DEFAULT_WIDTH };
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
    const size_t cap = 256 + (size_t)trace->count * 40;
    char *text = malloc(cap);
    if (text == NULL) {
        snprintf(why, why_cap, "out of memory");
        return 0;
    }
    size_t at = (size_t)snprintf(text, cap, "{\n  \"version\": 1,\n  \"width\": %.1f,\n  \"points\": [\n", trace->width);
    for (int i = 0; i < trace->count; ++i)
        at += (size_t)snprintf(text + at, cap - at, "    [%.3f, %.3f]%s\n", trace->points[i].x, trace->points[i].z,
                               i + 1 < trace->count ? "," : "");
    snprintf(text + at, cap - at, "  ]\n}\n");
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

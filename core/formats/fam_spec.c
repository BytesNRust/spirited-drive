/* See fam_spec.h. */
#include "formats/fam_spec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WORD = 32, LINE = FAM_SPEC_PATH + 64 };

static int is_blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

/* the line without its comment and its trailing blanks, in buf (NUL-terminated); len bytes of the line are read */
static void strip_line(const char *line, size_t len, char *buf)
{
    size_t n = 0;
    for (size_t i = 0; i < len && line[i] != '#'; i++) buf[n++] = line[i];
    while (n > 0 && is_blank(buf[n - 1])) n--;
    buf[n] = '\0';
}

/* the word at *at (after its blanks), copied into word; 0 when there is none */
static int next_word(const char *s, size_t *at, char *word, size_t cap)
{
    while (is_blank(s[*at])) (*at)++;
    size_t n = 0;
    while (s[*at] != '\0' && !is_blank(s[*at]) && n + 1 < cap) word[n++] = s[(*at)++];
    word[n] = '\0';
    return n > 0;
}

static int parse_id(const char *word, long *id)
{
    char *end = NULL;
    *id = strtol(word, &end, 10);
    return word[0] != '\0' && *end == '\0' && *id >= 0;
}

static int add_item(FamSpec *out, FamSpecKind kind, const char *path, size_t line, ExchangeError *err)
{
    if (out->count == FAM_SPEC_ITEMS) { exchange_fail(err, "line %zu: more than %d lines", line, FAM_SPEC_ITEMS); return 0; }
    FamSpecItem *it = &out->item[out->count++];
    it->kind = kind;
    snprintf(it->path, sizeof it->path, "%s", path);
    return 1;
}

/* the rest of the line from `at` (after its blanks) as the picture's path */
static int line_path(const char *s, size_t at, char *path, size_t cap, size_t line, ExchangeError *err)
{
    while (is_blank(s[at])) at++;
    if (s[at] == '\0') { exchange_fail(err, "line %zu: no picture named", line); return 0; }
    if (strlen(s + at) >= cap) { exchange_fail(err, "line %zu: the path is longer than %zu bytes", line, cap - 1); return 0; }
    snprintf(path, cap, "%s", s + at);
    return 1;
}

static size_t terrain_count_of(const FamSpec *out)
{
    size_t n = 0;
    for (size_t i = 0; i < out->count; i++) n += out->item[i].kind == FAM_SPEC_TERRAIN;
    return n;
}

/* "terrain ID PATH": ID is the next texture id (0, 1, ...) */
static int terrain_line(const char *s, size_t *at, FamSpec *out, size_t line, ExchangeError *err)
{
    char word[WORD];
    long id = -1;
    const size_t next = terrain_count_of(out);
    if (!next_word(s, at, word, sizeof word) || !parse_id(word, &id)) {
        exchange_fail(err, "line %zu: a terrain line is 'terrain ID PNG', the id a number", line);
        return 0;
    }
    if ((size_t)id != next) { exchange_fail(err, "line %zu: terrain id %ld where %zu comes: the ids go 0, 1, 2 in order", line, id, next); return 0; }
    char path[FAM_SPEC_PATH];
    return line_path(s, *at, path, sizeof path, line, err) && add_item(out, FAM_SPEC_TERRAIN, path, line, err);
}

static int horizon_seen(const FamSpec *out)
{
    for (size_t i = 0; i < out->count; i++)
        if (out->item[i].kind == FAM_SPEC_HORIZON) return 1;
    return 0;
}

static int horizon_line(const char *s, size_t at, FamSpec *out, size_t line, ExchangeError *err)
{
    char path[FAM_SPEC_PATH];
    if (!line_path(s, at, path, sizeof path, line, err)) return 0;
    if (horizon_seen(out)) { exchange_fail(err, "line %zu: a second horizon (the first is on another line)", line); return 0; }
    return add_item(out, FAM_SPEC_HORIZON, path, line, err);
}

/* one line of the spec, comment stripped */
static int line_of(const char *s, size_t line, FamSpec *out, ExchangeError *err)
{
    size_t at = 0;
    char word[WORD];
    if (!next_word(s, &at, word, sizeof word)) return 1;
    char path[FAM_SPEC_PATH];
    if (strcmp(word, "terrain") == 0) return terrain_line(s, &at, out, line, err);
    if (strcmp(word, "scenery") == 0) return line_path(s, at, path, sizeof path, line, err) && add_item(out, FAM_SPEC_SCENERY, path, line, err);
    if (strcmp(word, "horizon") == 0) return horizon_line(s, at, out, line, err);
    exchange_fail(err, "line %zu: '%s' is not terrain, scenery or horizon", line, word);
    return 0;
}

int fam_spec_parse(const char *text, FamSpec *out, ExchangeError *err)
{
    out->count = 0;
    size_t line = 0;
    const char *p = text;
    while (*p != '\0') {
        const char *end = strchr(p, '\n');
        const size_t len = end != NULL ? (size_t)(end - p) : strlen(p);
        line++;
        if (len >= LINE) { exchange_fail(err, "line %zu: longer than %d bytes", line, LINE - 1); return 0; }
        char buf[LINE];
        strip_line(p, len, buf);
        if (!line_of(buf, line, out, err)) return 0;
        p = end != NULL ? end + 1 : p + len;
    }
    if (!horizon_seen(out)) { exchange_fail(err, "the spec names no horizon (a line 'horizon PNG')"); return 0; }
    return 1;
}

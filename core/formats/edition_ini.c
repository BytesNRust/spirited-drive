/* port (dep59 step 4): see edition_ini.h. */
#include "formats/edition_ini.h"
#include "formats/text_bom.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list ap;
        va_start(ap, format);
        vsnprintf(why, cap, format, ap);
        va_end(ap);
    }
    return 0;
}

int edition_id_ok(const char *id)
{
    return id != NULL && mod_ini_id_ok(id) && strcmp(id, "original") != 0;
}

int edition_name_ok(const char *name, char *why, size_t why_cap)
{
    const size_t n = strlen(name);
    if (n == 0 || n > EDITION_NAME_MAX) return refuse(why, why_cap, "an edition's name is 1 to %d characters", EDITION_NAME_MAX);
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)name[i] < 0x20 || (unsigned char)name[i] > 0x7e)
            return refuse(why, why_cap, "an edition's name has a byte that is not printable ASCII at %zu", i + 1);
    if (strcasecmp(name, "Customize game") == 0) return refuse(why, why_cap, "an edition may not be named Customize game");
    return 1;
}

/* the three lists: their key, their place in Edition, their limit */
typedef struct {
    const char *key;
    int max;
} List;

enum { LIST_CARS, LIST_TRACKS, LIST_TWEAKS, LIST_COUNT, KEY_NAME = LIST_COUNT, KEY_COUNT };

static const List LISTS[LIST_COUNT] = {
    [LIST_CARS] = { "cars", EDITION_LIST_MAX },
    [LIST_TRACKS] = { "tracks", EDITION_TRACKS_MAX },
    [LIST_TWEAKS] = { "tweaks", EDITION_TWEAKS_MAX },
};

static int *count_of(Edition *e, int list)
{
    return list == LIST_CARS ? &e->cars : list == LIST_TRACKS ? &e->tracks : &e->tweaks;
}

static char (*ids_of(Edition *e, int list))[MOD_INI_ID_MAX + 1]
{
    return list == LIST_CARS ? e->car : list == LIST_TRACKS ? e->track : e->tweak;
}

/* `s` .. `e` without the spaces round it */
static void trim(const char **s, const char **e)
{
    while (*s < *e && (**s == ' ' || **s == '\t')) (*s)++;
    while (*e > *s && ((*e)[-1] == ' ' || (*e)[-1] == '\t' || (*e)[-1] == '\r')) (*e)--;
}

/* the list's value `s` .. `e` into `out`; 0 with `why` naming the entry when one is refused (by its place: a bad id is not printed back) */
static int take_list(const char *s, const char *e, Edition *out, int list, char *why, size_t cap)
{
    int *const count = count_of(out, list);
    char (*const ids)[MOD_INI_ID_MAX + 1] = ids_of(out, list);
    int entry = 0;
    for (const char *p = s; p <= e;) {
        const char *comma = memchr(p, ',', (size_t)(e - p));
        const char *is = p, *ie = comma != NULL ? comma : e;
        trim(&is, &ie);
        entry++;
        const size_t n = (size_t)(ie - is);
        if (n > 0) {
            char id[MOD_INI_ID_MAX + 1] = "";
            if (n <= MOD_INI_ID_MAX) memcpy(id, is, n), id[n] = '\0';
            if (n > MOD_INI_ID_MAX || !edition_id_ok(id))
                return refuse(why, cap, "edition: %s: entry %d is not an id (a-z, 0-9 and '-', not first, up to %d; not \"original\")",
                              LISTS[list].key, entry, MOD_INI_ID_MAX);
            for (int k = 0; k < *count; k++)
                if (strcmp(ids[k], id) == 0) return refuse(why, cap, "edition: %s names %s twice", LISTS[list].key, id);
            if (*count == LISTS[list].max) return refuse(why, cap, "edition: %s has more than %d entries", LISTS[list].key, LISTS[list].max);
            memcpy(ids[(*count)++], id, n + 1);
        }
        if (comma == NULL) break;
        p = comma + 1;
    }
    return 1;
}

/* one `key=value` line into `out`; 0 with `why` set when it is refused */
static int take_line(const char *line, const char *end, Edition *out, int seen[KEY_COUNT], char *why, size_t cap)
{
    const char *eq = memchr(line, '=', (size_t)(end - line));
    if (eq == NULL) return refuse(why, cap, "edition: a line has no '='");
    const char *ks = line, *ke = eq, *vs = eq + 1, *ve = end;
    trim(&ks, &ke), trim(&vs, &ve);
    const size_t klen = (size_t)(ke - ks), vlen = (size_t)(ve - vs);
    for (int k = 0; k < KEY_COUNT; k++) {   /* only a key the game knows is read */
        const char *const key = k == KEY_NAME ? "name" : LISTS[k].key;
        if (strlen(key) != klen || memcmp(key, ks, klen) != 0) continue;
        if (seen[k]) return refuse(why, cap, "edition: %s is given twice", key);
        seen[k] = 1;
        if (k != KEY_NAME) return take_list(vs, ve, out, k, why, cap);
        char name[EDITION_NAME_MAX + 2] = "";   /* one more than a name: a longer one stays too long */
        const size_t n = vlen < sizeof name - 1 ? vlen : sizeof name - 1;
        memcpy(name, vs, n), name[n] = '\0';
        char reason[96];
        if (!edition_name_ok(name, reason, sizeof reason)) return refuse(why, cap, "edition: %s", reason);
        memcpy(out->name, name, n + 1);
        return 1;
    }
    return 1;   /* a key of a later version */
}

int edition_parse(const uint8_t *text, size_t size, Edition *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (text == NULL) return refuse(why, why_cap, "edition: no file");
    if (text_is_utf16(text, size)) return refuse(why, why_cap, "edition: the file is UTF-16: save it as UTF-8");
    text = text_skip_bom(text, &size);
    if (size > EDITION_MAX_SIZE) return refuse(why, why_cap, "edition: the file is larger than %d bytes", EDITION_MAX_SIZE);
    if (memchr(text, '\0', size) != NULL) return refuse(why, why_cap, "edition: the file is not text");
    int seen[KEY_COUNT] = { 0 };
    const char *p = (const char *)text, *end = p + size;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *s = p, *e = nl != NULL ? nl : end;
        trim(&s, &e);
        if (s < e && *s != '#' && !take_line(s, e, out, seen, why, why_cap)) return memset(out, 0, sizeof *out), 0;
        p = nl != NULL ? nl + 1 : end;
    }
    if (!seen[KEY_NAME]) return memset(out, 0, sizeof *out), refuse(why, why_cap, "edition: the file has no name");
    return 1;
}

/* `text` at `*at` in `out`; 0 when it does not fit with its NUL */
static int put(char *out, size_t cap, size_t *at, const char *text)
{
    const size_t n = strlen(text);
    if (n >= cap - *at) return 0;
    memcpy(out + *at, text, n + 1);
    *at += n;
    return 1;
}

int edition_format(const Edition *e, char *out, size_t cap)
{
    if (cap == 0) return 0;
    const int counts[LIST_COUNT] = { [LIST_CARS] = e->cars, [LIST_TRACKS] = e->tracks, [LIST_TWEAKS] = e->tweaks };
    const char (*const ids[LIST_COUNT])[MOD_INI_ID_MAX + 1] = { [LIST_CARS] = e->car, [LIST_TRACKS] = e->track, [LIST_TWEAKS] = e->tweak };
    size_t at = 0;
    int ok = put(out, cap, &at, "name=") && put(out, cap, &at, e->name) && put(out, cap, &at, "\n");
    for (int list = 0; ok && list < LIST_COUNT; list++) {
        if (counts[list] == 0) continue;
        ok = put(out, cap, &at, LISTS[list].key) && put(out, cap, &at, "=");
        for (int i = 0; ok && i < counts[list]; i++) ok = (i == 0 || put(out, cap, &at, ",")) && put(out, cap, &at, ids[list][i]);
        ok = ok && put(out, cap, &at, "\n");
    }
    if (!ok) return out[0] = '\0', 0;
    return (int)at;
}

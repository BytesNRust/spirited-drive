/* See mod_ini.h. */
#include "formats/mod_ini.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

int mod_ini_id_ok(const char *id)
{
    const size_t n = strlen(id);
    if (n == 0 || n > MOD_INI_ID_MAX || id[0] == '-' || strcmp(id, "legacy") == 0) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((id[i] >= 'a' && id[i] <= 'z') || (id[i] >= '0' && id[i] <= '9') || id[i] == '-')) return 0;
    return 1;
}

typedef struct {
    const char *key;
    size_t offset, cap;   /* the field in ModInfo and its size, with the NUL */
} Field;

static const Field FIELDS[] = {
    { "id", offsetof(ModInfo, id), sizeof ((ModInfo *)0)->id },
    { "name", offsetof(ModInfo, name), sizeof ((ModInfo *)0)->name },
    { "version", offsetof(ModInfo, version), sizeof ((ModInfo *)0)->version },
    { "author", offsetof(ModInfo, author), sizeof ((ModInfo *)0)->author },
    { "description", offsetof(ModInfo, description), sizeof ((ModInfo *)0)->description },
};
enum { FIELD_COUNT = sizeof FIELDS / sizeof FIELDS[0], REQUIRED = 3 };   /* id, name and version, the first three */

/* `s` .. `e` without the spaces round it */
static void trim(const char **s, const char **e)
{
    while (*s < *e && (**s == ' ' || **s == '\t')) (*s)++;
    while (*e > *s && ((*e)[-1] == ' ' || (*e)[-1] == '\t' || (*e)[-1] == '\r')) (*e)--;
}

/* one `key=value` line into `out`; 0 with `why` set when it is refused */
static int take_line(const char *line, const char *end, ModInfo *out, int seen[FIELD_COUNT], char *why, size_t cap)
{
    const char *eq = memchr(line, '=', (size_t)(end - line));
    if (eq == NULL) return refuse(why, cap, "mod.ini: a line has no '='");
    const char *ks = line, *ke = eq, *vs = eq + 1, *ve = end;
    trim(&ks, &ke), trim(&vs, &ve);
    const size_t klen = (size_t)(ke - ks), vlen = (size_t)(ve - vs);
    for (const char *c = vs; c < ve; c++)
        if ((unsigned char)*c < 0x20 || *c == 0x7f) return refuse(why, cap, "mod.ini: %.*s has a control character", (int)klen, ks);
    for (int f = 0; f < FIELD_COUNT; f++) {
        if (strlen(FIELDS[f].key) != klen || memcmp(FIELDS[f].key, ks, klen) != 0) continue;
        if (seen[f]) return refuse(why, cap, "mod.ini: %s is given twice", FIELDS[f].key);
        if (vlen == 0) return refuse(why, cap, "mod.ini: %s is empty", FIELDS[f].key);
        if (vlen >= FIELDS[f].cap) return refuse(why, cap, "mod.ini: %s is longer than %zu characters", FIELDS[f].key, FIELDS[f].cap - 1);
        char *field = (char *)out + FIELDS[f].offset;
        memcpy(field, vs, vlen);
        field[vlen] = '\0';
        seen[f] = 1;
        return 1;
    }
    return 1;   /* a key of a later version */
}

int mod_ini_parse(const uint8_t *text, size_t size, ModInfo *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (text == NULL) return refuse(why, why_cap, "mod.ini is missing");
    if (size > MOD_INI_MAX_SIZE) return refuse(why, why_cap, "mod.ini is larger than %d bytes", MOD_INI_MAX_SIZE);
    if (memchr(text, '\0', size) != NULL) return refuse(why, why_cap, "mod.ini is not text");
    int seen[FIELD_COUNT] = { 0 };
    const char *p = (const char *)text, *end = p + size;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *line_end = nl != NULL ? nl : end;
        const char *s = p, *e = line_end;
        trim(&s, &e);
        if (s < e && *s != '#' && !take_line(s, e, out, seen, why, why_cap)) return memset(out, 0, sizeof *out), 0;
        p = nl != NULL ? nl + 1 : end;
    }
    for (int f = 0; f < REQUIRED; f++)
        if (!seen[f]) return memset(out, 0, sizeof *out), refuse(why, why_cap, "mod.ini has no %s", FIELDS[f].key);
    if (!mod_ini_id_ok(out->id))
        return memset(out, 0, sizeof *out),
               refuse(why, why_cap, "mod.ini: the id may hold only a-z, 0-9 and '-' (not first), up to %d, and not \"legacy\"", MOD_INI_ID_MAX);
    return 1;
}

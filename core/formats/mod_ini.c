/* See mod_ini.h. */
#include "formats/mod_ini.h"
#include "formats/zip.h"

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
    if (n == 0 || n > MOD_INI_ID_MAX || id[0] == '-' || strcmp(id, "legacy") == 0 || zip_name_is_device(id, n)) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((id[i] >= 'a' && id[i] <= 'z') || (id[i] >= '0' && id[i] <= '9') || id[i] == '-')) return 0;
    return 1;
}

enum { KIND_TEXT, KIND_STEM, KIND_WORD, KIND_TYPE };   /* what a value may hold: text; A-Z 0-9; a-z 0-9; a type's name */

typedef struct {
    const char *key;
    size_t offset, cap;   /* the field in ModInfo and its size, with the NUL (KIND_TYPE: the longest name's) */
    int kind;
} Field;

static const Field FIELDS[] = {
    { "id", offsetof(ModInfo, id), sizeof ((ModInfo *)0)->id, KIND_TEXT },
    { "name", offsetof(ModInfo, name), sizeof ((ModInfo *)0)->name, KIND_TEXT },
    { "version", offsetof(ModInfo, version), sizeof ((ModInfo *)0)->version, KIND_TEXT },
    { "author", offsetof(ModInfo, author), sizeof ((ModInfo *)0)->author, KIND_TEXT },
    { "description", offsetof(ModInfo, description), sizeof ((ModInfo *)0)->description, KIND_TEXT },
    { "type", 0, sizeof "track", KIND_TYPE },
    { "stem", offsetof(ModInfo, stem), sizeof ((ModInfo *)0)->stem, KIND_STEM },
    { "short_name", offsetof(ModInfo, short_name), sizeof ((ModInfo *)0)->short_name, KIND_TEXT },
    { "sound_from", offsetof(ModInfo, sound_from), sizeof ((ModInfo *)0)->sound_from, KIND_WORD },
    { "ai_from", offsetof(ModInfo, ai_from), sizeof ((ModInfo *)0)->ai_from, KIND_WORD },
    { "dash_from", offsetof(ModInfo, dash_from), sizeof ((ModInfo *)0)->dash_from, KIND_WORD },
};

static const char *const TYPE_NAMES[] = { [MOD_TYPE_TWEAK] = "tweak", [MOD_TYPE_CAR] = "car", [MOD_TYPE_TRACK] = "track" };

/* 1 when every one of the `n` characters of `s` is one of `kind`'s */
static int chars_ok(const char *s, size_t n, int kind)
{
    for (size_t i = 0; i < n; i++) {
        const char c = s[i];
        const int digit = c >= '0' && c <= '9';
        if (kind == KIND_STEM && !(digit || (c >= 'A' && c <= 'Z'))) return 0;
        if (kind == KIND_WORD && !(digit || (c >= 'a' && c <= 'z'))) return 0;
    }
    return 1;
}

/* the type named by `s` (`n` long), or -1 */
static int type_named(const char *s, size_t n)
{
    for (int t = 0; t < (int)(sizeof TYPE_NAMES / sizeof TYPE_NAMES[0]); t++)
        if (strlen(TYPE_NAMES[t]) == n && memcmp(TYPE_NAMES[t], s, n) == 0) return t;
    return -1;
}
enum { FIELD_COUNT = sizeof FIELDS / sizeof FIELDS[0], REQUIRED = 3 };   /* id, name and version, the first three */

/* `s` .. `e` without the spaces round it */
static void trim(const char **s, const char **e)
{
    while (*s < *e && (**s == ' ' || **s == '\t')) (*s)++;
    while (*e > *s && ((*e)[-1] == ' ' || (*e)[-1] == '\t' || (*e)[-1] == '\r')) (*e)--;
}

/* 0 when `s` is not UTF-8 (overlong forms and surrogates included), or holds what would change or hide what the player reads: a C0 or
 * C1 control, DEL, or a bidirectional mark (U+061C, U+200E-200F, U+202A-202E, U+2066-2069) */
static int text_clean(const unsigned char *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        uint32_t c = s[i];
        size_t len;
        if (c < 0x80) {
            if (c < 0x20 || c == 0x7f) return 0;
            i++;
            continue;
        }
        if (c >= 0xc2 && c <= 0xdf) len = 2, c &= 0x1f;
        else if (c >= 0xe0 && c <= 0xef) len = 3, c &= 0x0f;
        else if (c >= 0xf0 && c <= 0xf4) len = 4, c &= 0x07;
        else return 0;
        if (len > n - i) return 0;
        for (size_t k = 1; k < len; k++) {
            if ((s[i + k] & 0xc0) != 0x80) return 0;
            c = c << 6 | (s[i + k] & 0x3f);
        }
        if ((len == 3 && c < 0x800) || (len == 4 && (c < 0x10000 || c > 0x10ffff)) || (c >= 0xd800 && c <= 0xdfff)) return 0;
        if (c <= 0x9f || c == 0x61c || c == 0x200e || c == 0x200f || (c >= 0x202a && c <= 0x202e) || (c >= 0x2066 && c <= 0x2069)) return 0;
        i += len;
    }
    return 1;
}

/* one `key=value` line into `out`; 0 with `why` set when it is refused */
static int take_line(const char *line, const char *end, ModInfo *out, int seen[FIELD_COUNT], char *why, size_t cap)
{
    const char *eq = memchr(line, '=', (size_t)(end - line));
    if (eq == NULL) return refuse(why, cap, "mod.ini: a line has no '='");
    const char *ks = line, *ke = eq, *vs = eq + 1, *ve = end;
    trim(&ks, &ke), trim(&vs, &ve);
    const size_t klen = (size_t)(ke - ks), vlen = (size_t)(ve - vs);
    for (int f = 0; f < FIELD_COUNT; f++) {   /* only a key the game knows is read, and only its name is ever said back */
        if (strlen(FIELDS[f].key) != klen || memcmp(FIELDS[f].key, ks, klen) != 0) continue;
        if (seen[f]) return refuse(why, cap, "mod.ini: %s is given twice", FIELDS[f].key);
        if (!text_clean((const unsigned char *)vs, vlen))
            return refuse(why, cap, "mod.ini: %s is not UTF-8 text, or holds a control or direction character", FIELDS[f].key);
        if (vlen == 0) return refuse(why, cap, "mod.ini: %s is empty", FIELDS[f].key);
        seen[f] = 1;
        if (FIELDS[f].kind == KIND_TYPE) {
            out->type = type_named(vs, vlen);
            return out->type >= 0 ? 1 : refuse(why, cap, "mod.ini: type is not tweak, car or track");
        }
        if (vlen >= FIELDS[f].cap) return refuse(why, cap, "mod.ini: %s is longer than %zu characters", FIELDS[f].key, FIELDS[f].cap - 1);
        if (!chars_ok(vs, vlen, FIELDS[f].kind))
            return refuse(why, cap, "mod.ini: %s may hold only %s", FIELDS[f].key, FIELDS[f].kind == KIND_STEM ? "A-Z and 0-9" : "a-z and 0-9");
        char *field = (char *)out + FIELDS[f].offset;
        memcpy(field, vs, vlen);
        field[vlen] = '\0';
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

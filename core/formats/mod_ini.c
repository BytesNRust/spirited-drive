/* See mod_ini.h. */
#include "formats/mod_ini.h"
#include "formats/text_bom.h"
#include "formats/zip.h"

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

int mod_ini_id_ok(const char *id)
{
    const size_t n = strlen(id);
    if (n == 0 || n > MOD_INI_ID_MAX || id[0] == '-' || strcmp(id, "legacy") == 0 || zip_name_is_device(id, n)) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((id[i] >= 'a' && id[i] <= 'z') || (id[i] >= '0' && id[i] <= '9') || id[i] == '-')) return 0;
    return 1;
}

/* what a value may hold: text; printable ASCII (drawn raw in the game's big face, GRAVER18); A-Z 0-9; a-z 0-9; a type's name; paints;
 * on or off (an int field, 1 for off) */
enum { KIND_TEXT, KIND_PLAIN, KIND_STEM, KIND_WORD, KIND_TYPE, KIND_PAINTS, KIND_SWITCH, KIND_PHOTOS, KIND_PHOTO };

typedef struct {
    const char *key;
    size_t offset, cap;   /* the field in ModInfo and its size, with the NUL (KIND_TYPE: the longest name's) */
    int kind;
} Field;

static const Field FIELDS[] = {
    { "id", offsetof(ModInfo, id), sizeof ((ModInfo *)0)->id, KIND_TEXT },
    { "name", offsetof(ModInfo, name), sizeof ((ModInfo *)0)->name, KIND_PLAIN },
    { "version", offsetof(ModInfo, version), sizeof ((ModInfo *)0)->version, KIND_TEXT },
    { "author", offsetof(ModInfo, author), sizeof ((ModInfo *)0)->author, KIND_TEXT },
    { "description", offsetof(ModInfo, description), sizeof ((ModInfo *)0)->description, KIND_TEXT },
    { "type", 0, sizeof "track", KIND_TYPE },
    { "stem", offsetof(ModInfo, stem), sizeof ((ModInfo *)0)->stem, KIND_STEM },
    { "short_name", offsetof(ModInfo, short_name), sizeof ((ModInfo *)0)->short_name, KIND_PLAIN },
    { "sound_from", offsetof(ModInfo, sound_from), sizeof ((ModInfo *)0)->sound_from, KIND_WORD },
    { "ai_from", offsetof(ModInfo, ai_from), sizeof ((ModInfo *)0)->ai_from, KIND_WORD },
    { "dash_from", offsetof(ModInfo, dash_from), sizeof ((ModInfo *)0)->dash_from, KIND_WORD },
    { "ai_level", offsetof(ModInfo, ai_level), sizeof ((ModInfo *)0)->ai_level, KIND_WORD },   /* port (dep59 3b) */
    { "abs", offsetof(ModInfo, assist_abs), sizeof ((ModInfo *)0)->assist_abs, KIND_WORD },   /* port (dep78): a car's ABS and traction control, yes or no */
    { "traction", offsetof(ModInfo, assist_traction), sizeof ((ModInfo *)0)->assist_traction, KIND_WORD },
    { "year", offsetof(ModInfo, year), sizeof ((ModInfo *)0)->year, KIND_TEXT },
    { "power", offsetof(ModInfo, power), sizeof ((ModInfo *)0)->power, KIND_TEXT },
    { "weight", offsetof(ModInfo, weight), sizeof ((ModInfo *)0)->weight, KIND_TEXT },
    { "paints", 0, 0, KIND_PAINTS },
    { "category", offsetof(ModInfo, category), sizeof ((ModInfo *)0)->category, KIND_TEXT },   /* port (dep59 showcase) */
    { "make", offsetof(ModInfo, make), sizeof ((ModInfo *)0)->make, KIND_TEXT },   /* port (owner 2026-11-18): kept for later */
    { "model", offsetof(ModInfo, model), sizeof ((ModInfo *)0)->model, KIND_TEXT },
    { "country", offsetof(ModInfo, country), sizeof ((ModInfo *)0)->country, KIND_TEXT },
    { "photos", 0, 0, KIND_PHOTOS },
    { "photo_small", offsetof(ModInfo, photo_small), sizeof ((ModInfo *)0)->photo_small, KIND_PHOTO },
    { "card_name", offsetof(ModInfo, card_name_off), sizeof "off", KIND_SWITCH },   /* port (dep59 gallery plan section 1) */
    { "kind", offsetof(ModInfo, kind), sizeof ((ModInfo *)0)->kind, KIND_WORD },   /* port (dep59 step 5): a track's */
    { "segments", offsetof(ModInfo, segments), sizeof ((ModInfo *)0)->segments, KIND_WORD },
    { "road", offsetof(ModInfo, road), sizeof ((ModInfo *)0)->road, KIND_WORD },
    { "edge", offsetof(ModInfo, edge), sizeof ((ModInfo *)0)->edge, KIND_WORD },
    { "length", offsetof(ModInfo, length), sizeof ((ModInfo *)0)->length, KIND_TEXT },
    { "laps", offsetof(ModInfo, laps), sizeof ((ModInfo *)0)->laps, KIND_TEXT },
    { "times", offsetof(ModInfo, times), sizeof ((ModInfo *)0)->times, KIND_TEXT },
    { "showcase_tint", offsetof(ModInfo, showcase_tint), sizeof ((ModInfo *)0)->showcase_tint, KIND_TEXT },   /* port (dep59 step 5, T7d): a track page's background tone */
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

static void trim(const char **s, const char **e);

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* one `Name:rrggbb` (`s` .. `e`, trimmed) into `p`; 0 with `why` naming it when it is refused */
/* the position (from 1) of the first of the `n` bytes of `s` that is not printable ASCII (0x20..0x7e); 0 when there is none */
static size_t not_plain_at(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7e) return i + 1;
    return 0;
}

static int take_paint(const char *s, const char *e, int entry, ModPaint *p, char *why, size_t cap)
{
    const int n = (int)(e - s);
    if (n == 0) return refuse(why, cap, "mod.ini: paints has an empty entry");
    const char *colon = memchr(s, ':', (size_t)n);
    if (colon == NULL || memchr(colon + 1, ':', (size_t)(e - colon - 1)) != NULL)
        return refuse(why, cap, "mod.ini: paints: \"%.*s\" is not Name:rrggbb", n, s);
    const char *ns = s, *ne = colon, *hs = colon + 1, *he = e;
    trim(&ns, &ne), trim(&hs, &he);
    const size_t name_len = (size_t)(ne - ns);
    if (name_len == 0 || name_len > MOD_INI_PAINT_NAME_MAX)
        return refuse(why, cap, "mod.ini: paints: \"%.*s\" needs a name of 1 to %d characters", n, s, MOD_INI_PAINT_NAME_MAX);
    const size_t bad = not_plain_at(ns, name_len);   /* said by its place: the name itself is not something to print back */
    if (bad != 0) return refuse(why, cap, "mod.ini: paints: entry %d's name has a byte that is not printable ASCII at %zu", entry, bad);
    if (he - hs != 6) return refuse(why, cap, "mod.ini: paints: %.*s's colour is not six hex digits", (int)name_len, ns);
    for (int i = 0; i < 3; i++) {
        const int hi = hex_digit(hs[2 * i]), lo = hex_digit(hs[2 * i + 1]);
        if (hi < 0 || lo < 0) return refuse(why, cap, "mod.ini: paints: %.*s's colour is not six hex digits", (int)name_len, ns);
        p->rgb[i] = (uint8_t)(hi << 4 | lo);
    }
    memcpy(p->name, ns, name_len);
    p->name[name_len] = '\0';
    return 1;
}

/* paints=, the value `s` .. `e`, into `out`; 0 with `why` naming the entry when one is refused */
static int take_paints(const char *s, const char *e, ModInfo *out, char *why, size_t cap)
{
    for (const char *p = s; p <= e;) {
        const char *comma = memchr(p, ',', (size_t)(e - p));
        const char *end = comma != NULL ? comma : e, *es = p, *ee = end;
        trim(&es, &ee);
        if (out->paint_count == MOD_INI_PAINTS_MAX) return refuse(why, cap, "mod.ini: paints has more than %d entries", MOD_INI_PAINTS_MAX);
        ModPaint paint;
        if (!take_paint(es, ee, out->paint_count + 1, &paint, why, cap)) return 0;
        for (int k = 0; k < out->paint_count; k++)
            if (strcasecmp(out->paints[k].name, paint.name) == 0) return refuse(why, cap, "mod.ini: paints names %s twice", paint.name);
        out->paints[out->paint_count++] = paint;
        if (comma == NULL) break;
        p = comma + 1;
    }
    return 1;
}

/* why `s` (n bytes) is no photo's file name (a-z 0-9 '-' '_' '.', ending ".png", up to MOD_INI_PHOTO_NAME_MAX, none the game's own pictures
 * and tweaks use); NULL when it is one */
static const char *photo_fault(const char *s, size_t n)
{
    static const char *const OWN[] = { "card.png", "rival.png", "gallery.png", "preview.png", "car_envmap.png" };
    if (n == 0) return "is empty";
    if (n > MOD_INI_PHOTO_NAME_MAX) return "is longer than 24 characters";
    for (size_t i = 0; i < n; i++) {
        const char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
            return "may hold only a-z, 0-9, '-', '_' and '.' (a file in the piece's folder)";
    }
    if (n < 5 || memcmp(s + n - 4, ".png", 4) != 0) return "is not a .png";
    for (size_t k = 0; k < sizeof OWN / sizeof OWN[0]; k++)
        if (strlen(OWN[k]) == n && memcmp(OWN[k], s, n) == 0) return "is a name the game's own pictures use";
    return NULL;
}

int mod_ini_photo_name_ok(const char *name) { return photo_fault(name, strlen(name)) == NULL; }

/* one photo's file name, `s` .. `e`, into `name` (MOD_INI_PHOTO_NAME_MAX + 1 bytes); 0 with `why` saying which key and why */
static int take_photo_name(const char *key, const char *s, const char *e, char *name, char *why, size_t cap)
{
    const size_t n = (size_t)(e - s);
    const char *fault = photo_fault(s, n);
    if (fault != NULL) return refuse(why, cap, "mod.ini: %s: \"%.*s\" %s", key, (int)n, s, fault);
    memcpy(name, s, n);
    name[n] = '\0';
    return 1;
}

/* photos=, the value `s` .. `e`, into `out`: up to MOD_INI_PHOTOS_MAX names, none twice */
static int take_photos(const char *s, const char *e, ModInfo *out, char *why, size_t cap)
{
    for (const char *p = s; p <= e;) {
        const char *comma = memchr(p, ',', (size_t)(e - p));
        const char *end = comma != NULL ? comma : e, *es = p, *ee = end;
        trim(&es, &ee);
        if (out->photo_count == MOD_INI_PHOTOS_MAX) return refuse(why, cap, "mod.ini: photos has more than %d entries", MOD_INI_PHOTOS_MAX);
        char *name = out->photo[out->photo_count];
        if (!take_photo_name("photos", es, ee, name, why, cap)) return 0;
        for (int k = 0; k < out->photo_count; k++)
            if (strcmp(out->photo[k], name) == 0) return refuse(why, cap, "mod.ini: photos names %s twice", name);
        out->photo_count++;
        if (comma == NULL) break;
        p = comma + 1;
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
        const size_t bad = FIELDS[f].kind == KIND_PLAIN ? not_plain_at(vs, vlen) : 0;
        if (bad != 0) return refuse(why, cap, "mod.ini: %s has a byte that is not printable ASCII at %zu", FIELDS[f].key, bad);
        if (!text_clean((const unsigned char *)vs, vlen))
            return refuse(why, cap, "mod.ini: %s is not UTF-8 text, or holds a control or direction character", FIELDS[f].key);
        if (vlen == 0) return refuse(why, cap, "mod.ini: %s is empty", FIELDS[f].key);
        seen[f] = 1;
        if (FIELDS[f].kind == KIND_PAINTS) return take_paints(vs, ve, out, why, cap);
        if (FIELDS[f].kind == KIND_PHOTOS) return take_photos(vs, ve, out, why, cap);
        if (FIELDS[f].kind == KIND_PHOTO) return take_photo_name(FIELDS[f].key, vs, ve, (char *)out + FIELDS[f].offset, why, cap);
        if (FIELDS[f].kind == KIND_TYPE) {
            out->type = type_named(vs, vlen);
            return out->type >= 0 ? 1 : refuse(why, cap, "mod.ini: type is not tweak, car or track");
        }
        if (FIELDS[f].kind == KIND_SWITCH) {
            const int on = vlen == 2 && memcmp(vs, "on", 2) == 0, off = vlen == 3 && memcmp(vs, "off", 3) == 0;
            if (!on && !off) return refuse(why, cap, "mod.ini: %s is on or off", FIELDS[f].key);
            *(int *)((char *)out + FIELDS[f].offset) = off;
            return 1;
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
    if (text_is_utf16(text, size)) return refuse(why, why_cap, "mod.ini is UTF-16: save it as UTF-8");
    text = text_skip_bom(text, &size);
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

/* the keys a generator writes itself: a rewritten card takes these from its own values, never from the card it replaces */
static int generated_key(const char *s, size_t n)
{
    static const char *const OWN[] = { "id", "name", "version", "type", "stem", "kind", "segments", "road", "edge", "author", "description" };
    for (size_t i = 0; i < sizeof OWN / sizeof OWN[0]; i++)
        if (strlen(OWN[i]) == n && memcmp(OWN[i], s, n) == 0) return 1;
    return 0;
}

int mod_ini_others(const uint8_t *text, size_t size, char *out, size_t cap, size_t *len)
{
    size_t n = 0;
    if (cap == 0) return 0;
    out[0] = '\0';
    const char *p = (const char *)text, *end = p + size;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *s = p, *e = nl != NULL ? nl : end;
        trim(&s, &e);
        const char *eq = s < e && *s != '#' ? memchr(s, '=', (size_t)(e - s)) : NULL;
        if (eq != NULL) {
            const char *ks = s, *ke = eq;
            trim(&ks, &ke);
            if (!generated_key(ks, (size_t)(ke - ks))) {
                const size_t line = (size_t)(e - s);
                if (n + line + 2 > cap) return 0;
                memcpy(out + n, s, line);
                n += line;
                out[n++] = '\n';
                out[n] = '\0';
            }
        }
        p = nl != NULL ? nl + 1 : end;
    }
    *len = n;
    return 1;
}

int mod_ini_text_keys(const char *author, const char *description, char *out, size_t cap)
{
    const int n = author != NULL && author[0] != '\0'
        ? snprintf(out, cap, "author=%s\n", author)
        : snprintf(out, cap, "# author=   (your name, up to %d characters)\n", MOD_INI_AUTHOR_MAX);
    if (n < 0 || (size_t)n >= cap) return 0;
    const size_t at = (size_t)n;
    const int m = description != NULL && description[0] != '\0'
        ? snprintf(out + at, cap - at, "description=%s\n", description)
        : snprintf(out + at, cap - at, "# description=   (what the piece is, up to %d characters)\n", MOD_INI_DESCRIPTION_MAX);
    return m >= 0 && (size_t)m < cap - at;
}

/* a key mod_ini_set may write: 1..32 of a-z 0-9 '_' (every key the game reads is, and so is a later version's) */
static int set_key_ok(const char *key)
{
    const size_t n = key != NULL ? strnlen(key, 64) : 0;
    if (n == 0 || n > 32) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((key[i] >= 'a' && key[i] <= 'z') || (key[i] >= '0' && key[i] <= '9') || key[i] == '_')) return 0;
    return 1;
}

/* why `value` cannot be written after a key as it is, or NULL when it can */
static const char *set_value_fault(const char *value, size_t n)
{
    if (memchr(value, '\n', n) != NULL || memchr(value, '\r', n) != NULL) return "the value holds a line break, which would start another line";
    if (n > 0 && (value[0] == ' ' || value[0] == '\t' || value[n - 1] == ' ' || value[n - 1] == '\t'))
        return "the value starts or ends with a space, which the card's reader drops";
    return NULL;
}

/* the first line of `text` whose key is `key` (and whether another has it): its start `*from`, the end of what it says `*content` and of its line
 * end `*next`, the '=' `*eq`; 0 when there is none */
typedef struct {
    size_t from, content, next, eq;
    int again;
} SetLine;

static int set_find(const char *text, size_t size, const char *key, SetLine *found)
{
    const size_t klen = strlen(key);
    int have = 0;
    for (size_t at = 0; at < size;) {
        const char *nl = memchr(text + at, '\n', size - at);
        const size_t stop = nl != NULL ? (size_t)(nl - text) : size, next = nl != NULL ? stop + 1 : size;
        const char *s = text + at, *e = text + stop;
        trim(&s, &e);
        const char *eq = s < e && *s != '#' ? memchr(s, '=', (size_t)(e - s)) : NULL;
        if (eq != NULL) {
            const char *ks = s, *ke = eq;
            trim(&ks, &ke);
            if ((size_t)(ke - ks) == klen && memcmp(ks, key, klen) == 0) {
                if (have) return found->again = 1, 1;
                const size_t content = stop > at && text[stop - 1] == '\r' ? stop - 1 : stop;
                *found = (SetLine){ .from = at, .content = content, .next = next, .eq = (size_t)(eq - text), .again = 0 };
                have = 1;
            }
        }
        at = next;
    }
    return have;
}

int mod_ini_set(const char *text, size_t size, const char *key, const char *value, char *out, size_t cap, size_t *len, int *duplicate,
                char *why, size_t why_cap)
{
    if (text == NULL) return refuse(why, why_cap, "mod.ini is missing");
    if (size > MOD_INI_MAX_SIZE) return refuse(why, why_cap, "mod.ini is larger than %d bytes", MOD_INI_MAX_SIZE);
    if (memchr(text, '\0', size) != NULL) return refuse(why, why_cap, "mod.ini is not text");
    if (!set_key_ok(key)) return refuse(why, why_cap, "mod.ini: a key is 1 to 32 of a-z, 0-9 and '_'");
    const size_t vlen = value != NULL ? strlen(value) : 0;
    const char *fault = vlen > 0 ? set_value_fault(value, vlen) : NULL;
    if (fault != NULL) return refuse(why, why_cap, "mod.ini: %s: %s", key, fault);
    SetLine at = { 0 };
    const int there = set_find(text, size, key, &at);
    if (duplicate != NULL) *duplicate = there && at.again;
    const char *nl = memchr(text, '\n', size);
    const char *end = nl != NULL && nl > text && nl[-1] == '\r' ? "\r\n" : "\n";   /* the card's own line end, from its first */
    size_t total, keep_to = 0, resume = 0;   /* out = text[0..keep_to) + `line` + text[resume..) */
    const char *line = "";
    size_t line_len = 0;
    char fresh[MOD_INI_MAX_SIZE + 64];
    if (there && vlen == 0) {   /* removed: the line and its line end go */
        keep_to = at.from, resume = at.next;
    } else if (there) {   /* changed: what it said up to the value (its spacing round '=') stays, its line end too */
        size_t v = at.eq + 1;
        while (v < at.content && (text[v] == ' ' || text[v] == '\t')) v++;
        keep_to = v, resume = at.content;
        line = value, line_len = vlen;
    } else if (vlen == 0) {   /* nothing to remove */
        keep_to = size, resume = size;
    } else {   /* appended after the last line, which gets a line end first if it has none */
        const int open = size > 0 && text[size - 1] != '\n';
        keep_to = size, resume = size;
        line_len = (size_t)snprintf(fresh, sizeof fresh, "%s%s=%s%s", open ? end : "", key, value, end);
        line = fresh;
    }
    total = keep_to + line_len + (size - resume);
    if (total > MOD_INI_MAX_SIZE) return refuse(why, why_cap, "mod.ini would be larger than %d bytes", MOD_INI_MAX_SIZE);
    if (out == NULL || total + 1 > cap) return refuse(why, why_cap, "mod.ini: no room for the new card");
    memcpy(out, text, keep_to);
    memcpy(out + keep_to, line, line_len);
    memcpy(out + keep_to + line_len, text + resume, size - resume);
    out[total] = '\0';
    if (len != NULL) *len = total;
    return 1;
}

int mod_ini_paints_parse(const char *value, ModPaint *paints, int *count, char *why, size_t why_cap)
{
    if (value == NULL || paints == NULL || count == NULL) return refuse(why, why_cap, "mod.ini: paints is missing");
    const char *vs = value, *ve = value + strlen(value);
    trim(&vs, &ve);
    if (vs == ve) return refuse(why, why_cap, "mod.ini: paints is empty");
    if (!text_clean((const unsigned char *)vs, (size_t)(ve - vs)))
        return refuse(why, why_cap, "mod.ini: paints is not UTF-8 text, or holds a control or direction character");
    ModInfo tmp;
    memset(&tmp, 0, sizeof tmp);
    if (!take_paints(vs, ve, &tmp, why, why_cap)) return 0;
    memcpy(paints, tmp.paints, sizeof(ModPaint) * (size_t)tmp.paint_count);
    *count = tmp.paint_count;
    return 1;
}

int mod_ini_paints_format(const ModPaint *paints, int count, char *out, size_t cap, char *why, size_t why_cap)
{
    if (paints == NULL || count < 1) return refuse(why, why_cap, "mod.ini: paints needs at least one paint");
    if (count > MOD_INI_PAINTS_MAX) return refuse(why, why_cap, "mod.ini: paints has more than %d entries", MOD_INI_PAINTS_MAX);
    char line[MOD_INI_PAINTS_MAX * (MOD_INI_PAINT_NAME_MAX + 8) + 1];
    size_t n = 0;
    for (int i = 0; i < count; i++) {
        const size_t name_len = strnlen(paints[i].name, sizeof paints[i].name);
        const char *name = paints[i].name;
        if (name_len == sizeof paints[i].name) return refuse(why, why_cap, "mod.ini: paints: entry %d's name is longer than %d", i + 1, MOD_INI_PAINT_NAME_MAX);
        if (name_len > 0 && (name[0] == ' ' || name[0] == '\t' || name[name_len - 1] == ' ' || name[name_len - 1] == '\t'))
            return refuse(why, why_cap, "mod.ini: paints: entry %d's name starts or ends with a space", i + 1);
        if (memchr(name, ':', name_len) != NULL || memchr(name, ',', name_len) != NULL)
            return refuse(why, why_cap, "mod.ini: paints: entry %d's name holds ':' or ','", i + 1);
        n += (size_t)snprintf(line + n, sizeof line - n, "%s%s:%02x%02x%02x", i > 0 ? "," : "", name, paints[i].rgb[0], paints[i].rgb[1], paints[i].rgb[2]);
    }
    ModPaint back[MOD_INI_PAINTS_MAX];
    int got = 0;
    if (!mod_ini_paints_parse(line, back, &got, why, why_cap)) return 0;   /* the parser's own rules, and its words */
    if (got != count) return refuse(why, why_cap, "mod.ini: paints would not read back as written");
    if (n + 1 > cap || out == NULL) return refuse(why, why_cap, "mod.ini: no room for paints");
    memcpy(out, line, n + 1);
    return 1;
}

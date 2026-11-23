#include "car_table_rewrite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LINE_MAX_COLS = 124 };

typedef struct {
    const char *p, *end;
} Scan;

typedef struct {
    char name[40];   /* the table's own name: a card keeps 24 characters */
    uint8_t rgb[3];
} TablePaint;

static void blanks(Scan *s)
{
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\n')) s->p++;
}

static int lit(Scan *s, const char *text)
{
    const size_t n = strlen(text);
    blanks(s);
    if ((size_t)(s->end - s->p) < n || strncmp(s->p, text, n) != 0) return 0;
    s->p += n;
    return 1;
}

static int num(Scan *s, int *v)
{
    blanks(s);
    if (s->p >= s->end || *s->p < '0' || *s->p > '9') return 0;
    int n = 0, digits = 0;
    while (s->p < s->end && *s->p >= '0' && *s->p <= '9' && digits < 4) n = n * 10 + (*s->p++ - '0'), digits++;
    if (n > 255 || (s->p < s->end && *s->p >= '0' && *s->p <= '9')) return 0;
    *v = n;
    return 1;
}

static int rgb(Scan *s, uint8_t out[3])
{
    int v[3];
    if (!lit(s, "{") || !num(s, &v[0]) || !lit(s, ",") || !num(s, &v[1]) || !lit(s, ",") || !num(s, &v[2]) || !lit(s, "}")) return 0;
    for (int i = 0; i < 3; i++) out[i] = (uint8_t)v[i];
    return 1;
}

static int quoted(Scan *s, char *out, size_t cap)
{
    blanks(s);
    if (s->p >= s->end || *s->p != '"') return 0;
    const char *q = s->p + 1;
    size_t n = 0;
    while (q < s->end && *q != '"') {
        if (*q == '\\' || *q == '\n' || n + 1 >= cap) return 0;
        out[n++] = *q++;
    }
    if (q >= s->end) return 0;
    out[n] = '\0';
    s->p = q + 1;
    return 1;
}

static int say(char *why, size_t cap, const char *fmt, const char *a)
{
    if (why != NULL && cap > 0) snprintf(why, cap, fmt, a);
    return 0;
}

static void list_of(const TablePaint *p, int n, char *out)
{
    size_t used = 0;
    out[0] = '\0';
    for (int k = 0; k < n && used < CAR_TABLE_LIST; k++)
        used += (size_t)snprintf(out + used, CAR_TABLE_LIST - used, "%s%s #%02x%02x%02x", k > 0 ? ", " : "", p[k].name, p[k].rgb[0], p[k].rgb[1], p[k].rgb[2]);
}

/* the row as the file has it, from its first brace: `{ "STEM", N, { o, o, o }, { {paints} } },`. On success `*row_end` is past the closing "},". */
static int read_row(Scan *s, const char *stem, int *count, uint8_t own[3], TablePaint *have, const char **row_end, char *why, size_t why_cap)
{
    char name[40];
    int declared = 0, n = 0;
    if (!lit(s, "{") || !quoted(s, name, sizeof name) || strcmp(name, stem) != 0 || !lit(s, ",") || !num(s, &declared) || !lit(s, ",") || !rgb(s, own) || !lit(s, ",") || !lit(s, "{"))
        return say(why, why_cap, "the row of %s is not laid out as the writer expects ({ \"STEM\", COUNT, { r, g, b }, { { \"Name\", { r, g, b } }, ... } },)", stem);
    if (declared < 1 || declared > CAR_TABLE_MAX_PAINTS) return say(why, why_cap, "the row of %s has a count the table does not allow", stem);
    for (;;) {
        if (n >= CAR_TABLE_MAX_PAINTS) return say(why, why_cap, "the row of %s has more paints than CAR_PAINT_MAX", stem);
        char pn[40];
        if (!lit(s, "{") || !quoted(s, pn, sizeof pn) || strlen(pn) >= sizeof have[n].name || !lit(s, ",") || !rgb(s, have[n].rgb) || !lit(s, "}"))
            return say(why, why_cap, "a paint of %s is not laid out as the writer expects ({ \"Name\", { r, g, b } })", stem);
        snprintf(have[n].name, sizeof have[n].name, "%s", pn);   /* the table's names may be longer than a card's 24 ("30th Anniversary Lavender") */
        n++;
        if (lit(s, ",")) continue;
        break;
    }
    if (!lit(s, "}") || !lit(s, "}") || !lit(s, ",")) return say(why, why_cap, "the row of %s does not end as the writer expects (} },)", stem);
    if (n != declared) return say(why, why_cap, "the row of %s says a count the list does not have", stem);
    *count = n;
    *row_end = s->p;
    return 1;
}

/* the row's text: the first line, then the paints wrapped under LINE_MAX_COLS columns */
static char *row_text(const char *stem, const uint8_t own[3], const TablePaint *p, int n)
{
    char *out = malloc(1200);
    if (out == NULL) return NULL;
    size_t used = (size_t)snprintf(out, 1200, "    { \"%s\", %d, { %d, %d, %d },\n      { ", stem, n, own[0], own[1], own[2]);
    size_t line = 8;   /* the columns of the paints' line so far: "      { " */
    for (int k = 0; k < n; k++) {
        char e[120];
        const int el = snprintf(e, sizeof e, "{ \"%s\", { %d, %d, %d } }", p[k].name, p[k].rgb[0], p[k].rgb[1], p[k].rgb[2]);
        if (k > 0 && line + 2 + (size_t)el + 1 > LINE_MAX_COLS) used += (size_t)snprintf(out + used, 1200 - used, ",\n        "), line = 8;
        else if (k > 0) used += (size_t)snprintf(out + used, 1200 - used, ", "), line += 2;
        used += (size_t)snprintf(out + used, 1200 - used, "%s", e), line += (size_t)el;
    }
    snprintf(out + used, 1200 - used, " } },");
    return out;
}

static int name_ok(const char *n)
{
    const size_t len = strlen(n);
    if (len == 0 || len > MOD_INI_PAINT_NAME_MAX) return 0;
    for (size_t i = 0; i < len; i++)
        if (n[i] < 32 || n[i] > 126 || n[i] == '"' || n[i] == '\\') return 0;
    return 1;
}

char *car_table_rewrite(const char *src, size_t len, const char *stem, const ModPaint *paints, int count, int *changed, char *before, char *after, char *why, size_t why_cap)
{
    static const char DECL[] = "static const CarPaints TABLE[CAR_PAINT_MODELS] = {\n";
    if (before != NULL) before[0] = '\0';
    if (after != NULL) after[0] = '\0';
    if (count < 1 || count > CAR_TABLE_MAX_PAINTS) return say(why, why_cap, "%s", count < 1 ? "the card has no colors: the game needs at least one" : "the game offers up to 8 colors for a car: take the others off the card"), NULL;
    for (int k = 0; k < count; k++)
        if (!name_ok(paints[k].name)) return say(why, why_cap, "a color's name cannot be written into the table (1 to 24 printable characters, no quote or backslash): %s", paints[k].name), NULL;
    char *text = malloc(len + 1);
    if (text == NULL) return say(why, why_cap, "%s", "out of memory"), NULL;
    memcpy(text, src, len), text[len] = '\0';
    const char *decl = strstr(text, DECL);
    if (decl == NULL) return free(text), say(why, why_cap, "car_paint.c has no `%s` line: the table is not as the writer expects", "static const CarPaints TABLE[CAR_PAINT_MODELS] = {"), NULL;
    const char *table_end = strstr(decl, "\n};\n");
    char key[64];
    snprintf(key, sizeof key, "\n    { \"%s\", ", stem);
    const char *row = strstr(decl, key);
    if (table_end == NULL || row == NULL || row > table_end) return free(text), say(why, why_cap, "the table in car_paint.c has no row for %s", stem), NULL;
    row++;   /* past the newline: the row's four spaces */
    Scan s = { row + 4, text + len };
    int have_count = 0;
    uint8_t own[3];
    TablePaint have[CAR_TABLE_MAX_PAINTS];
    const char *row_end = NULL;
    if (!read_row(&s, stem, &have_count, own, have, &row_end, why, why_cap)) return free(text), NULL;
    if (row_end > table_end) return free(text), say(why, why_cap, "the row of %s runs past the table", stem), NULL;
    /* what the row should be: the card's colors, a name the card has cut at its 24 characters being the table's full one */
    TablePaint want[CAR_TABLE_MAX_PAINTS];
    for (int k = 0; k < count; k++) {
        snprintf(want[k].name, sizeof want[k].name, "%s", paints[k].name);
        memcpy(want[k].rgb, paints[k].rgb, 3);
        if (k < have_count && strlen(have[k].name) > MOD_INI_PAINT_NAME_MAX && strncmp(have[k].name, paints[k].name, MOD_INI_PAINT_NAME_MAX) == 0 && strlen(paints[k].name) == MOD_INI_PAINT_NAME_MAX)
            snprintf(want[k].name, sizeof want[k].name, "%s", have[k].name);
    }
    if (before != NULL) list_of(have, have_count, before);
    if (after != NULL) list_of(want, count, after);
    int same = have_count == count;
    for (int k = 0; same && k < count; k++) same = strcmp(have[k].name, want[k].name) == 0 && memcmp(have[k].rgb, want[k].rgb, 3) == 0;
    *changed = !same;
    if (same) return text;
    char *mine = row_text(stem, own, want, count);
    if (mine == NULL) return free(text), say(why, why_cap, "%s", "out of memory"), NULL;
    char *out = malloc(len + strlen(mine) + 8);
    if (out == NULL) return free(text), free(mine), say(why, why_cap, "%s", "out of memory"), NULL;
    const size_t head = (size_t)(row - text);
    memcpy(out, text, head);
    strcpy(out + head, mine);
    strcpy(out + head + strlen(mine), row_end);
    free(text), free(mine);
    return out;
}

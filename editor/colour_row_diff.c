#include "colour_row_diff.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { ROW_MAX = 16, ENTRY_MAX = 96 };

typedef struct {
    char name[ENTRY_MAX];
    char hex[8];   // "#rrggbb"
} Entry;

// the entries of a row: each ends at its «#rrggbb»; what lies between two of them («, ») is not part of either
static int parse_row(const char *row, Entry *out, int max)
{
    int n = 0;
    const char *at = row;
    while (n < max && *at != '\0') {
        const char *hash = strchr(at, '#');
        if (hash == NULL) break;
        size_t len = (size_t)(hash - at);
        while (len > 0 && isspace((unsigned char)at[len - 1])) len--;
        if (len >= sizeof out[n].name) len = sizeof out[n].name - 1;
        memcpy(out[n].name, at, len);
        out[n].name[len] = '\0';
        size_t digits = 0;
        while (digits < 6 && isxdigit((unsigned char)hash[1 + digits])) digits++;
        out[n].hex[0] = '#';
        memcpy(out[n].hex + 1, hash + 1, digits);
        out[n].hex[1 + digits] = '\0';
        n++;
        at = hash + 1 + digits;
        while (*at == ',' || isspace((unsigned char)*at)) at++;
    }
    return n;
}

static size_t put(char *out, size_t cap, size_t used, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
static size_t put(char *out, size_t cap, size_t used, const char *fmt, ...)
{
    if (used + 1 >= cap) return used;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(out + used, cap - used, fmt, ap);
    va_end(ap);
    return n < 0 ? used : (used + (size_t)n < cap ? used + (size_t)n : cap - 1);
}

size_t colour_row_diff(const char *before, const char *after, char *out, size_t cap)
{
    if (cap == 0) return 0;
    out[0] = '\0';
    Entry was[ROW_MAX], now[ROW_MAX];
    const int nb = parse_row(before, was, ROW_MAX), na = parse_row(after, now, ROW_MAX);
    size_t used = 0;
    if (nb != na) used = put(out, cap, used, "  colors: %d -> %d (the game's table row goes to %d)\n", nb, na, na);
    else used = put(out, cap, used, "  colors: %d (the same number)\n", na);
    char list[512] = "";
    size_t l = 0;
    const int common = nb < na ? nb : na;
    for (int i = 0; i < common; i++) {
        const int same_name = strcmp(was[i].name, now[i].name) == 0, same_hex = strcmp(was[i].hex, now[i].hex) == 0;
        if (same_name && same_hex) continue;
        if (same_name) l = put(list, sizeof list, l, "%s%s %s -> %s", l > 0 ? "; " : "", was[i].name, was[i].hex, now[i].hex);
        else l = put(list, sizeof list, l, "%s%s %s -> %s %s", l > 0 ? "; " : "", was[i].name, was[i].hex, now[i].name, now[i].hex);
    }
    if (l > 0) used = put(out, cap, used, "  changed: %s\n", list);
    l = 0, list[0] = '\0';
    for (int i = nb; i < na; i++) l = put(list, sizeof list, l, "%s%s %s", l > 0 ? ", " : "", now[i].name, now[i].hex);
    if (l > 0) used = put(out, cap, used, "  added: %s\n", list);
    l = 0, list[0] = '\0';
    for (int i = na; i < nb; i++) l = put(list, sizeof list, l, "%s%s %s", l > 0 ? ", " : "", was[i].name, was[i].hex);
    if (l > 0) used = put(out, cap, used, "  removed: %s\n", list);
    return used;
}

/* See mod_list.h. */
#include "formats/mod_list.h"

#include <stdio.h>
#include <string.h>

int mod_list_id_ok(const char *id)
{
    return id != NULL && (mod_ini_id_ok(id) || strcmp(id, "legacy") == 0);
}

int mod_list_index(const ModList *l, const char *id)
{
    for (int i = 0; i < l->count; i++)
        if (strcmp(l->ids[i], id) == 0) return i;
    return -1;
}

/* one item, `s` .. `e` without the spaces round it, added to `out`; 1 when it was left out */
static int take(const char *s, const char *e, ModList *out)
{
    while (s < e && (*s == ' ' || *s == '\t')) s++;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
    if (s == e) return 0;   /* an empty item is nothing */
    const size_t n = (size_t)(e - s);
    char id[MOD_INI_ID_MAX + 1];
    if (n > MOD_INI_ID_MAX) return 1;
    memcpy(id, s, n);
    id[n] = '\0';
    if (!mod_list_id_ok(id) || mod_list_index(out, id) >= 0 || out->count == MOD_LIST_MAX) return 1;
    memcpy(out->ids[out->count++], id, n + 1);
    return 0;
}

int mod_list_parse(const char *text, ModList *out)
{
    memset(out, 0, sizeof *out);
    if (text == NULL) return 0;
    int left_out = 0;
    for (const char *p = text;;) {
        const char *comma = strchr(p, ',');
        const char *end = comma != NULL ? comma : p + strlen(p);
        left_out += take(p, end, out);
        if (comma == NULL) return left_out;
        p = comma + 1;
    }
}

int mod_list_format(const ModList *l, char *out, size_t cap)
{
    size_t at = 0;
    if (cap == 0) return 0;
    out[0] = '\0';
    for (int i = 0; i < l->count; i++) {
        const int n = snprintf(out + at, cap - at, "%s%s", i > 0 ? "," : "", l->ids[i]);
        if (n < 0 || (size_t)n >= cap - at) return out[0] = '\0', 0;
        at += (size_t)n;
    }
    return 1;
}

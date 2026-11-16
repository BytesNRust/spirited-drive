/* See content_rows.h. */
#include "game/frontend/content_rows.h"

#include "formats/stock_content.h"

#include <stdio.h>
#include <string.h>

typedef char Id[MOD_INI_ID_MAX + 1];

static const int TAB_TYPE[CONTENT_TABS] = { MOD_TYPE_CAR, MOD_TYPE_TRACK, MOD_TYPE_TWEAK };

/* the list of `e` a tab edits, its length and its room */
static const Id *list_of(const Edition *e, int tab, int *n)
{
    *n = tab == CONTENT_CARS ? e->cars : tab == CONTENT_TRACKS ? e->tracks : e->tweaks;
    return tab == CONTENT_CARS ? e->car : tab == CONTENT_TRACKS ? e->track : e->tweak;
}

static Id *list_edit(Edition *e, int tab, int **n, int *cap)
{
    *n = tab == CONTENT_CARS ? &e->cars : tab == CONTENT_TRACKS ? &e->tracks : &e->tweaks;
    *cap = tab == CONTENT_CARS ? EDITION_LIST_MAX : tab == CONTENT_TRACKS ? EDITION_TRACKS_MAX : EDITION_TWEAKS_MAX;
    return tab == CONTENT_CARS ? e->car : tab == CONTENT_TRACKS ? e->track : e->tweak;
}

/* the stock piece of the tab whose id is `id`: its English name; NULL when none (the hidden track included: never listed) */
static const char *stock_name(int tab, const char *id)
{
    if (tab == CONTENT_CARS) return stock_car_name(stock_car_of_id(id));
    if (tab == CONTENT_TRACKS) return stock_track_name(stock_track_of_id(id));
    return NULL;
}

/* 1 when `id` is the hidden track's: an edition may name it, the screen never lists it */
static int hidden(int tab, const char *id)
{
    return tab == CONTENT_TRACKS && stock_track_of_id(id) >= 0 && stock_name(tab, id) == NULL;
}

/* the place of the mod `id` of the tab's type in `mods` (`n` long), or -1 */
static int mod_find(const ModInfo *mods, int n, int tab, const char *id)
{
    for (int i = 0; i < n; i++)
        if (mods[i].type == TAB_TYPE[tab] && strcmp(mods[i].id, id) == 0) return i;
    return -1;
}

/* 1 when some removed mod (of any type) is `id`: its backup is there */
static int removed_has(const ContentMods *m, const char *id)
{
    for (int i = 0; i < m->removed_count; i++)
        if (strcmp(m->removed[i].id, id) == 0) return 1;
    return 0;
}

static int named(const Edition *e, int tab, const char *id)
{
    int n;
    const Id *l = list_of(e, tab, &n);
    for (int i = 0; i < n; i++)
        if (strcmp(l[i], id) == 0) return 1;
    return 0;
}

static ContentRow *add(ContentRows *out, const char *id, const char *name, const char *version, int kind, int on)
{
    if (out->count >= CONTENT_ROWS_MAX) return NULL;
    ContentRow *r = &out->row[out->count++];
    memset(r, 0, sizeof *r);
    snprintf(r->id, sizeof r->id, "%s", id);
    snprintf(r->name, sizeof r->name, "%s", name);
    snprintf(r->version, sizeof r->version, "%s", version);
    r->kind = kind, r->on = on;
    r->switchable = !out->locked && kind != CONTENT_ROW_REMOVED;
    r->movable = !out->locked && on;
    return r;
}

/* the edition's own pieces, in its order and on */
static void named_rows(ContentRows *out, const Edition *e, int tab, const ContentMods *m)
{
    int n;
    const Id *l = list_of(e, tab, &n);
    for (int i = 0; i < n; i++) {
        const char *stock = stock_name(tab, l[i]);
        const int k = mod_find(m->installed, m->installed_count, tab, l[i]);
        ContentRow *r = NULL;
        if (hidden(tab, l[i])) continue;
        if (stock != NULL) r = add(out, l[i], stock, "", CONTENT_ROW_GAME, 1);
        else if (k >= 0) r = add(out, l[i], m->installed[k].name, m->installed[k].version, CONTENT_ROW_MOD, 1);
        else if ((r = add(out, l[i], l[i], "", CONTENT_ROW_MISSING, 1)) != NULL) r->backup = removed_has(m, l[i]);
        if (r != NULL && k >= 0 && m->backups != NULL) r->backup = m->backups[k];
    }
}

/* the pieces the edition does not name (all of them for the standard edition): stock ones on in the standard edition, the rest off */
static void other_rows(ContentRows *out, const Edition *e, int tab, const ContentMods *m)
{
    const int stock_count = tab == CONTENT_CARS ? STOCK_CAR_WARRIOR + 1 : tab == CONTENT_TRACKS ? STOCK_TRACK_COUNT : 0;
    for (int s = 0; s < stock_count; s++) {
        const char *id = tab == CONTENT_CARS ? stock_car_id(s) : stock_track_id(s);
        const char *name = id != NULL ? stock_name(tab, id) : NULL;
        if (name != NULL && (e == NULL || !named(e, tab, id))) add(out, id, name, "", CONTENT_ROW_GAME, e == NULL);
    }
    for (int i = 0; i < m->installed_count; i++) {
        const ModInfo *mi = &m->installed[i];
        if (mi->type != TAB_TYPE[tab] || (e != NULL && named(e, tab, mi->id))) continue;
        ContentRow *r = add(out, mi->id, mi->name, mi->version, CONTENT_ROW_MOD, 0);
        if (r != NULL && m->backups != NULL) r->backup = m->backups[i];
    }
    for (int i = 0; i < m->removed_count; i++) {
        const ModInfo *mi = &m->removed[i];
        if (mi->type != TAB_TYPE[tab] || (e != NULL && named(e, tab, mi->id)) ||
            mod_find(m->installed, m->installed_count, tab, mi->id) >= 0) continue;
        ContentRow *r = add(out, mi->id, mi->name, mi->version, CONTENT_ROW_REMOVED, 0);
        if (r != NULL) r->backup = 1;
    }
}

void content_rows_build(ContentRows *out, const Edition *e, int tab, const ContentMods *mods)
{
    out->count = 0;
    out->locked = e == NULL;
    if (tab < 0 || tab >= CONTENT_TABS) return;
    if (e != NULL) named_rows(out, e, tab, mods);
    other_rows(out, e, tab, mods);
}

int content_rows_all(const ContentRows *rows)
{
    int on = 0, off = 0;
    for (int i = 0; i < rows->count; i++)
        if (rows->row[i].switchable) rows->row[i].on ? on++ : off++;
    return on > 0 && off == 0 ? CONTENT_ALL_ON : on > 0 ? CONTENT_ALL_MIXED : CONTENT_ALL_OFF;
}

/* `id` out of the list */
static void drop(Id *l, int *n, const char *id)
{
    int k = 0;
    for (int i = 0; i < *n; i++)
        if (strcmp(l[i], id) != 0) memmove(l[k++], l[i], sizeof l[0]);
    *n = k;
}

/* `id` at the end of the list; 0 when it is full */
static int append(Id *l, int *n, int cap, const char *id)
{
    if (*n >= cap) return 0;
    snprintf(l[(*n)++], sizeof l[0], "%s", id);
    return 1;
}

/* 1 when an edit of `in`'s `tab` may be made (not the standard edition, a tab), `out` then a copy of `in` */
static int edit_start(const Edition *in, Edition *out, int tab)
{
    if (in == NULL || tab < 0 || tab >= CONTENT_TABS) return 0;
    *out = *in;
    return 1;
}

int content_rows_switch(const Edition *in, Edition *out, int tab, const ContentRows *rows, int index)
{
    if (!edit_start(in, out, tab) || rows->locked || index < 0 || index >= rows->count || !rows->row[index].switchable) return 0;
    int *n, cap;
    Id *l = list_edit(out, tab, &n, &cap);
    const ContentRow *r = &rows->row[index];
    if (r->on) return drop(l, n, r->id), 1;
    if (append(l, n, cap, r->id)) return 1;
    *out = *in;
    return 0;
}

int content_rows_switch_all(const Edition *in, Edition *out, int tab, const ContentRows *rows)
{
    if (!edit_start(in, out, tab) || rows->locked) return 0;
    int *n, cap, changed = 0;
    Id *l = list_edit(out, tab, &n, &cap);
    const int off = content_rows_all(rows) == CONTENT_ALL_ON;
    for (int i = 0; i < rows->count; i++) {
        const ContentRow *r = &rows->row[i];
        if (!r->switchable || r->on != off) continue;
        if (off) drop(l, n, r->id), changed = 1;
        else changed |= append(l, n, cap, r->id);
    }
    return changed;
}

/* the place in the list of the row that is on `k` rows after the first one that is on (the list's order, its hidden ids passed over) */
static int on_place(const Id *l, int n, int tab, int k)
{
    for (int i = 0; i < n; i++)
        if (!hidden(tab, l[i]) && k-- == 0) return i;
    return -1;
}

int content_rows_move(const Edition *in, Edition *out, int tab, const ContentRows *rows, int index, int step)
{
    if (!edit_start(in, out, tab) || rows->locked || index < 0 || index >= rows->count || !rows->row[index].movable ||
        (step != 1 && step != -1)) return 0;
    int ordinal = 0;   /* the row's place among the rows that are on (they come first, in the list's order) */
    for (int i = 0; i < index; i++) ordinal += rows->row[i].on;
    int *n, cap;
    Id *l = list_edit(out, tab, &n, &cap);
    const int from = on_place((const Id *)l, *n, tab, ordinal), to = on_place((const Id *)l, *n, tab, ordinal + step);
    if (from < 0 || to < 0 || ordinal + step < 0) return 0;
    Id held;
    memcpy(held, l[from], sizeof held);
    memcpy(l[from], l[to], sizeof held);
    memcpy(l[to], held, sizeof held);
    return 1;
}

/* 1 when the list of `tab` names a piece that can be raced: a stock one, or an installed mod of its type */
static int has_playable(const Edition *e, int tab, const ContentMods *m)
{
    int n;
    const Id *l = list_of(e, tab, &n);
    for (int i = 0; i < n; i++)
        if (stock_name(tab, l[i]) != NULL || mod_find(m->installed, m->installed_count, tab, l[i]) >= 0) return 1;
    return 0;
}

int content_rules_ok(const Edition *e, const ContentMods *mods)
{
    if (e == NULL) return CONTENT_RULES_OK;
    return (has_playable(e, CONTENT_CARS, mods) ? 0 : CONTENT_RULES_NO_CAR) | (has_playable(e, CONTENT_TRACKS, mods) ? 0 : CONTENT_RULES_NO_TRACK);
}

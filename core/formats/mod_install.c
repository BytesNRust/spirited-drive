/* See mod_install.h. */
#include "formats/mod_install.h"

#include <stdio.h>
#include <string.h>

static int date_ok(const char *d)
{
    if (d == NULL || strlen(d) != MOD_INSTALL_DATE) return 0;
    for (int i = 0; i < MOD_INSTALL_DATE; i++)
        if (i == 8 ? d[i] != '-' : (d[i] < '0' || d[i] > '9')) return 0;
    return 1;
}

/* the version as one level of a folder's name: A-Z a-z 0-9 '.' kept, anything else '_' (never '-', which parts the name) */
static void safe_version(const char *v, char *out, size_t cap)
{
    size_t n = 0;
    for (; v[n] != '\0' && n + 1 < cap; n++) {
        const char c = v[n];
        out[n] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ? c : '_';
    }
    out[n] = '\0';
    if (strcmp(out, ".") == 0 || strcmp(out, "..") == 0) out[0] = '_';
}

void mod_install_plan(const ModCheck *check, const ModInfo *installed, int folder_without_card, const char *date, ModInstallPlan *out)
{
    memset(out, 0, sizeof *out);
    const char *id = check->info.id;
    out->kind = MOD_INSTALL_REFUSED;
    if (!mod_list_id_ok(id)) {
        snprintf(out->why, sizeof out->why, "the id \"%.40s\" is not one a mod may have", id);
        return;
    }
    if (!date_ok(date)) {
        snprintf(out->why, sizeof out->why, "the date \"%.20s\" is not YYYYMMDD-HHMMSS", date != NULL ? date : "");
        return;
    }
    snprintf(out->staging, sizeof out->staging, "mods/.staging/%s", id);
    snprintf(out->target, sizeof out->target, "mods/%s", id);
    if (folder_without_card) {
        snprintf(out->why, sizeof out->why, "%s has no mod.ini: it was not installed by the game, and is not replaced", out->target);
        return;
    }
    if (installed == NULL) {
        out->kind = MOD_INSTALL_NEW;
        return;
    }
    char version[MOD_INI_VERSION_MAX + 1];
    safe_version(installed->version, version, sizeof version);
    snprintf(out->old_version, sizeof out->old_version, "%s", installed->version);
    snprintf(out->backup, sizeof out->backup, "mods_backup/%s-%s-%s", id, version, date);
    out->kind = strcmp(installed->version, check->info.version) == 0 ? MOD_INSTALL_SAME : MOD_INSTALL_UPDATE;
}

/* the date of `name` when it is a backup of `id` ("<id>-<version with no '-'>-<date>"), else NULL */
static const char *backup_date(const char *id, const char *name)
{
    const size_t n = strlen(id);
    if (name == NULL || strncmp(name, id, n) != 0 || name[n] != '-') return NULL;
    const char *version = name + n + 1;
    const char *dash = strchr(version, '-');
    if (dash == NULL || dash == version || !date_ok(dash + 1)) return NULL;
    return dash + 1;
}

int mod_backups_to_drop(const char *id, const char *const names[], int count, int keep, int drop[], int cap)
{
    enum { MANY = 256 };
    int mine[MANY], n = 0;
    for (int i = 0; i < count && n < MANY; i++)
        if (backup_date(id, names[i]) != NULL) mine[n++] = i;
    for (int i = 1; i < n; i++)   /* oldest first */
        for (int j = i; j > 0 && strcmp(backup_date(id, names[mine[j]]), backup_date(id, names[mine[j - 1]])) < 0; j--) {
            const int t = mine[j];
            mine[j] = mine[j - 1], mine[j - 1] = t;
        }
    int out = 0;
    for (int i = 0; i < n - (keep > 0 ? keep : 0) && out < cap; i++) drop[out++] = mine[i];
    return out;
}

void mod_list_take_out(ModList *l, const char *id)
{
    const int at = mod_list_id_ok(id) ? mod_list_index(l, id) : -1;
    if (at < 0) return;
    for (int i = at; i + 1 < l->count; i++) memcpy(l->ids[i], l->ids[i + 1], sizeof l->ids[i]);
    l->count--;
}

void mod_list_put_first(ModList *l, const char *id)
{
    if (!mod_list_id_ok(id)) return;
    mod_list_take_out(l, id);
    if (l->count == MOD_LIST_MAX) l->count--;   /* full: the last one drops off */
    for (int i = l->count; i > 0; i--) memcpy(l->ids[i], l->ids[i - 1], sizeof l->ids[i]);
    snprintf(l->ids[0], sizeof l->ids[0], "%s", id);
    l->count++;
}

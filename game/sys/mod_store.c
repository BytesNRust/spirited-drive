/* See mod_store.h. */
#include "game/sys/mod_store.h"

#include "formats/mod_check.h"
#include "formats/mod_install.h"
#include "formats/zip.h"
#include "game/platform/platform.h"
#include "game/sys/edition_file.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

enum { REL_MAX = 600, EDITION_IDS_MAX = 256 };   /* EDITION_IDS_MAX: the editions mod_store_used_in reads at most */

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

/* ---- reading */

/* the zip at `path`, at most ZIP_MAX_SIZE bytes; NULL with `why` set */
static uint8_t *read_zip(const char *path, size_t *size, char *why, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return refuse(why, cap, "%s cannot be read", path), NULL;
    long n = -1;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    uint8_t *data = NULL;
    if (n < 0 || (unsigned long)n > ZIP_MAX_SIZE) refuse(why, cap, "%s is not a zip of up to %u MB", path, (unsigned)(ZIP_MAX_SIZE >> 20));
    else if (fseek(f, 0, SEEK_SET) != 0 || (data = malloc(n > 0 ? (size_t)n : 1)) == NULL || fread(data, 1, (size_t)n, f) != (size_t)n)
        free(data), data = NULL, refuse(why, cap, "%s cannot be read", path);
    fclose(f);
    *size = data != NULL ? (size_t)n : 0;
    return data;
}

/* the card of the mod installed at `rel` into `out`: 1, or 0 when it has none (or one that does not read) */
static int card_at(const char *rel, ModInfo *out)
{
    char path[REL_MAX];
    snprintf(path, sizeof path, "%s/mod.ini", rel);
    size_t size = 0;
    uint8_t *text = platform_user_read_file_at(path, &size);
    if (text == NULL) return 0;
    char why[160];
    const int ok = mod_ini_parse(text, size, out, why, sizeof why);
    platform_free_file(text);
    return ok;
}

static void date_of(time_t t, char out[MOD_INSTALL_DATE + 1])
{
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    snprintf(out, MOD_INSTALL_DATE + 1, "%04d%02d%02d-%02d%02d%02d", (tm.tm_year + 1900) % 10000, (tm.tm_mon + 1) % 100, tm.tm_mday % 100,
             tm.tm_hour % 100, tm.tm_min % 100, tm.tm_sec % 100);
}

/* port (dep59 step 4): installing only makes a mod available; the folders in effect (mod_set.h) are worked out again, for the edition
 * in use may name it */
static void folders_follow(void) { port_settings_forget(); }

/* ---- backups */

/* the backups of `id` past the last MOD_BACKUPS_KEPT removed */
static void trim_backups(const char *id, int keep)
{
    int count = 0;
    char **names = platform_user_list_dirs("mods_backup", &count);
    int drop[256];
    const int n = mod_backups_to_drop(id, (const char *const *)names, count, keep, drop, 256);
    for (int i = 0; i < n; i++) {
        char rel[REL_MAX];
        snprintf(rel, sizeof rel, "mods_backup/%s", names[drop[i]]);
        if (!platform_user_remove_tree(rel)) port_log("mods: the old backup %s could not be removed\n", rel);
    }
    platform_free_names(names);
}

/* `rel` (an installed mod's folder, its card `card`) moved to a new backup of its own; its path in `out` */
static int back_up(const char *rel, const ModInfo *card, char *out, size_t out_cap, char *why, size_t cap)
{
    ModCheck as_new;
    memset(&as_new, 0, sizeof as_new);
    as_new.info = *card;
    ModInstallPlan p;
    const time_t now = time(NULL);
    for (int s = 0; s < 120; s++) {   /* a name of its own: two in one second take the next */
        char date[MOD_INSTALL_DATE + 1];
        date_of(now + s, date);
        mod_install_plan(&as_new, card, 0, date, &p);
        if (p.kind == MOD_INSTALL_REFUSED) return refuse(why, cap, "%s", p.why);
        if (!platform_user_exists(p.backup)) break;
    }
    if (!platform_user_mkdirs("mods_backup")) return refuse(why, cap, "the folder mods_backup cannot be made");
    if (!platform_user_rename(rel, p.backup)) return refuse(why, cap, "%s cannot be moved to %s", rel, p.backup);
    snprintf(out, out_cap, "%s", p.backup);
    return 1;
}

/* ---- install */

static int unpack(const uint8_t *zip, size_t size, const ModCheck *c, const char *staging, char *why, size_t cap)
{
    ZipArchive z;
    if (zip_open(zip, size, &z) != ZIP_OK) return refuse(why, cap, "the zip cannot be read again");
    int ok = platform_user_mkdirs(staging) ? 1 : refuse(why, cap, "the folder %s cannot be made", staging);
    for (size_t i = 0; i < c->entry_count && ok; i++) {
        char rel[REL_MAX], dir[REL_MAX];
        const char *path = strcasecmp(c->entries[i].path, "mod.ini") == 0 ? "mod.ini" : c->entries[i].path;   /* the card as the game reads it */
        snprintf(rel, sizeof rel, "%s/%s", staging, path);
        snprintf(dir, sizeof dir, "%s", rel);
        *strrchr(dir, '/') = '\0';
        uint8_t *data = NULL;
        size_t n = 0;
        const ZipResult r = zip_extract(&z, c->entries[i].index, &data, &n);   /* its CRC checked */
        if (r != ZIP_OK) ok = refuse(why, cap, "%s: %s", c->entries[i].path, zip_result_text(r));
        else if (!platform_user_mkdirs(dir) || !platform_user_write_file_at(rel, data, n)) ok = refuse(why, cap, "%s cannot be written", rel);
        free(data);
    }
    zip_close(&z);
    return ok;
}

/* the plan for `c` against what mods/<id> holds today, a backup name of its own; 0 with `why` when refused */
static int plan_for(const ModCheck *c, ModInfo *installed, int *has_card, ModInstallPlan *p, char *why, size_t cap)
{
    char target[REL_MAX];
    snprintf(target, sizeof target, "mods/%s", mod_ini_id_ok(c->info.id) ? c->info.id : "-");
    if (platform_user_exists("mods") && !platform_user_is_dir("mods")) return refuse(why, cap, "mods in the player's folder is not a folder");
    const int there = platform_user_exists(target);
    if (there && !platform_user_is_dir(target)) return refuse(why, cap, "%s is not a folder: it is not replaced", target);
    *has_card = there && card_at(target, installed);
    const time_t now = time(NULL);
    for (int s = 0; s < 120; s++) {
        char date[MOD_INSTALL_DATE + 1];
        date_of(now + s, date);
        mod_install_plan(c, *has_card ? installed : NULL, there && !*has_card, date, p);
        if (p->kind == MOD_INSTALL_REFUSED) return refuse(why, cap, "%s", p->why);
        if (p->backup[0] == '\0' || !platform_user_exists(p->backup)) return 1;
    }
    return refuse(why, cap, "no free name for the backup of %s", target);
}

int mod_store_install(const char *zip_path, ModInfo *info_out, char *old_out, size_t old_cap, char *why, size_t why_cap)
{
    if (info_out != NULL) memset(info_out, 0, sizeof *info_out);
    if (old_out != NULL && old_cap > 0) old_out[0] = '\0';
    size_t size = 0;
    uint8_t *zip = read_zip(zip_path, &size, why, why_cap);
    if (zip == NULL) return 0;
    ModCheck c;
    if (!mod_check_zip(zip, size, &c, why, why_cap)) return free(zip), 0;
    ModInfo installed;
    int has_card = 0;
    ModInstallPlan p = { 0 };
    int ok = plan_for(&c, &installed, &has_card, &p, why, why_cap);
    const int planned = ok;   /* only a plan made names a staging folder to clean */
    if (ok && (!platform_user_mkdirs("mods/.staging") || !platform_user_remove_tree(p.staging)))   /* one left by a run that stopped */
        ok = refuse(why, why_cap, "the folder mods/.staging cannot be made ready");
    ok = ok && unpack(zip, size, &c, p.staging, why, why_cap);
    free(zip);
    const int update = ok && p.backup[0] != '\0';
    if (update) {
        ok = platform_user_mkdirs("mods_backup") && platform_user_rename(p.target, p.backup);
        if (!ok) refuse(why, why_cap, "%s cannot be moved to %s", p.target, p.backup);
    }
    if (ok && !platform_user_rename(p.staging, p.target)) {
        ok = refuse(why, why_cap, "%s cannot be put in place", p.target);
        if (update && !platform_user_rename(p.backup, p.target)) port_log("mods: %s could not be put back from %s\n", p.target, p.backup);
    }
    if (planned && !platform_user_remove_tree(p.staging)) port_log("mods: %s could not be removed\n", p.staging);
    if (ok) {
        if (update) trim_backups(c.info.id, MOD_BACKUPS_KEPT);
        folders_follow();
        if (info_out != NULL) *info_out = c.info;
        if (old_out != NULL && update) snprintf(old_out, old_cap, "%s", p.old_version);
        port_log("mods: %s %s %s\n", update ? "updated" : "installed", c.info.id, c.info.version);
    }
    mod_check_free(&c);
    return ok;
}

/* ---- uninstall, restore, backups */

int mod_store_uninstall(const char *id, char *why, size_t why_cap)
{
    if (!mod_ini_id_ok(id)) return refuse(why, why_cap, "\"%.40s\" is not a mod's id", id);
    char target[REL_MAX], backup[REL_MAX];
    snprintf(target, sizeof target, "mods/%s", id);
    ModInfo card;
    if (!platform_user_is_dir(target)) return refuse(why, why_cap, "%s is not installed", id);
    if (!card_at(target, &card)) return refuse(why, why_cap, "%s has no mod.ini: it was not installed by the game, and is left alone", target);
    if (strcmp(card.id, id) != 0) return refuse(why, why_cap, "%s says it is %s: it was not installed by the game, and is left alone", target, card.id);
    if (!back_up(target, &card, backup, sizeof backup, why, why_cap)) return 0;
    trim_backups(id, MOD_BACKUPS_KEPT);
    folders_follow();   /* (the editions that name it are left as they are: they skip it) */
    port_log("mods: uninstalled %s, kept in %s\n", id, backup);
    return 1;
}

/* the newest backup of `id` into `out`; 0 when it has none */
static int newest_backup(const char *id, char *out, size_t cap)
{
    int count = 0;
    char **names = platform_user_list_dirs("mods_backup", &count);
    int all[256];
    const int total = mod_backups_to_drop(id, (const char *const *)names, count, 0, all, 256);   /* every one, oldest first */
    if (total > 0) snprintf(out, cap, "mods_backup/%s", names[all[total - 1]]);
    platform_free_names(names);
    return total > 0;
}

int mod_store_restore(const char *id, char *why, size_t why_cap)
{
    if (!mod_ini_id_ok(id)) return refuse(why, why_cap, "\"%.40s\" is not a mod's id", id);
    char from[REL_MAX], target[REL_MAX], backup[REL_MAX];
    if (!newest_backup(id, from, sizeof from)) return refuse(why, why_cap, "%s has no backup", id);
    snprintf(target, sizeof target, "mods/%s", id);
    backup[0] = '\0';
    if (platform_user_exists(target)) {
        ModInfo card;
        if (!platform_user_is_dir(target) || !card_at(target, &card) || strcmp(card.id, id) != 0)
            return refuse(why, why_cap, "%s is there and was not installed by the game: it is not replaced", target);
        if (!back_up(target, &card, backup, sizeof backup, why, why_cap)) return 0;
    }
    if (!platform_user_mkdirs("mods") || !platform_user_rename(from, target)) {
        if (backup[0] != '\0' && !platform_user_rename(backup, target)) port_log("mods: %s could not be put back from %s\n", target, backup);
        return refuse(why, why_cap, "%s cannot be put back", from);
    }
    trim_backups(id, MOD_BACKUPS_KEPT);
    folders_follow();
    port_log("mods: restored %s from %s\n", id, from);
    return 1;
}

int mod_store_drop_backups(const char *id, char *why, size_t why_cap)
{
    if (!mod_ini_id_ok(id)) return refuse(why, why_cap, "\"%.40s\" is not a mod's id", id);
    trim_backups(id, 0);
    char left[REL_MAX];
    return newest_backup(id, left, sizeof left) ? refuse(why, why_cap, "%s could not be removed", left) : 1;
}

/* ---- list */

static int backups_of(const char *id, char **names, int count)
{
    int drop[256];
    return mod_backups_to_drop(id, (const char *const *)names, count, 0, drop, 256);
}

/* 1 when the edition `e` names `id` among its tweaks or its cars */
static int named(const Edition *e, const char *id)
{
    for (int i = 0; i < e->tweaks; i++)
        if (strcmp(e->tweak[i], id) == 0) return 1;
    for (int i = 0; i < e->cars; i++)
        if (strcmp(e->car[i], id) == 0) return 1;
    return 0;
}

int mod_store_list(ModStoreEntry *out, int cap)
{
    int count = 0, backups = 0, n = 0;
    char **names = platform_user_list_dirs("mods", &count);
    char **saved = platform_user_list_dirs("mods_backup", &backups);
    static Edition e;
    const int in_use = edition_active(&e) == 1;
    for (int k = 0; k < count && n < cap; k++) {   /* by id (the folders are sorted) */
        const char *id = names[k];
        if (!mod_ini_id_ok(id)) continue;
        char rel[REL_MAX];
        snprintf(rel, sizeof rel, "mods/%s", id);
        ModStoreEntry entry;
        memset(&entry, 0, sizeof entry);
        if (!platform_user_is_dir(rel) || !card_at(rel, &entry.info) || strcmp(entry.info.id, id) != 0) continue;
        entry.active = in_use && named(&e, id);
        entry.backups = backups_of(id, saved, backups);
        out[n++] = entry;
    }
    platform_free_names(names);
    platform_free_names(saved);
    return n;
}

int mod_store_removed(ModStoreEntry *out, int cap)
{
    int backups = 0, n = 0;
    char **saved = platform_user_list_dirs("mods_backup", &backups);
    for (int k = 0; k < backups && n < cap; k++) {   /* each id once, from any of its backups (the folders are sorted) */
        char rel[REL_MAX], newest[REL_MAX];
        ModInfo card;
        snprintf(rel, sizeof rel, "mods_backup/%s", saved[k]);
        if (!platform_user_is_dir(rel) || !card_at(rel, &card) || !mod_ini_id_ok(card.id)) continue;
        int listed = 0;
        for (int i = 0; i < n && !listed; i++) listed = strcmp(out[i].info.id, card.id) == 0;
        snprintf(rel, sizeof rel, "mods/%s", card.id);
        if (listed || platform_user_is_dir(rel) || !newest_backup(card.id, newest, sizeof newest)) continue;
        ModStoreEntry entry;
        memset(&entry, 0, sizeof entry);
        if (!card_at(newest, &entry.info) || strcmp(entry.info.id, card.id) != 0) continue;
        entry.backups = backups_of(card.id, saved, backups);
        out[n++] = entry;
    }
    platform_free_names(saved);
    return n;
}

int mod_store_used_in(const char *id)
{
    static char ids[EDITION_IDS_MAX][EDITION_ID_MAX + 1];
    static Edition e;
    if (id == NULL || !mod_ini_id_ok(id)) return 0;
    const int count = edition_list(ids, EDITION_IDS_MAX);
    int used = 0;
    for (int k = 0; k < count; k++) {
        char why[160];
        if (!edition_load(ids[k], &e, why, sizeof why)) continue;
        int names = named(&e, id);
        for (int i = 0; i < e.tracks && !names; i++) names = strcmp(e.track[i], id) == 0;
        used += names;
    }
    return used;
}

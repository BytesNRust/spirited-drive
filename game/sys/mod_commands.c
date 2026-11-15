/* See mod_commands.h. */
#include "game/sys/mod_commands.h"

#include "game/sys/mod_store.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <string.h>

enum { LISTED_MAX = 256 };

static int failed(const char *why)
{
    port_fatal("tnfs: %s\n", why);
    return 1;
}

/* the installed mod `id`'s card into `out`; 0 when it is not listed */
static int installed(const char *id, ModStoreEntry *out)
{
    static ModStoreEntry all[LISTED_MAX];
    const int n = mod_store_list(all, LISTED_MAX);
    for (int i = 0; i < n; i++)
        if (strcmp(all[i].info.id, id) == 0) return *out = all[i], 1;
    return 0;
}

int mod_commands_install(const char *zip_path)
{
    char id[MOD_INI_ID_MAX + 1], old[MOD_INI_VERSION_MAX + 1], why[300];
    if (!mod_store_install(zip_path, id, sizeof id, old, sizeof old, why, sizeof why)) return failed(why);
    ModStoreEntry e;
    const char *version = installed(id, &e) ? e.info.version : "?";
    if (old[0] != '\0') printf("updated %s %s -> %s\n", id, old, version);
    else printf("installed %s %s\n", id, version);
    return 0;
}

int mod_commands_uninstall(const char *id)
{
    char why[300];
    if (!mod_store_uninstall(id, why, sizeof why)) return failed(why);
    printf("uninstalled %s (backup kept)\n", id);
    return 0;
}

int mod_commands_list(void)
{
    static ModStoreEntry all[LISTED_MAX];
    static const char *const TYPES[] = { "tweak", "car", "track" };
    const int n = mod_store_list(all, LISTED_MAX);
    for (int i = 0; i < n; i++) {
        const ModInfo *m = &all[i].info;
        printf("%s\t%s\t%s\t%s\t%s\n", m->id, m->version, m->type >= 0 && m->type <= 2 ? TYPES[m->type] : "?", all[i].active ? "on" : "off",
               m->name);
    }
    return 0;
}

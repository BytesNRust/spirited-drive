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

int mod_commands_install(const char *zip_path)
{
    char old[MOD_INI_VERSION_MAX + 1], why[300];
    ModInfo info;
    ModStoreOutcome how;
    if (!mod_store_install_outcome(zip_path, &info, old, sizeof old, &how, why, sizeof why)) return failed(why);
    if (how == MOD_STORE_UPDATED) printf("updated %s %s -> %s\n", info.id, old, info.version);
    else if (how == MOD_STORE_SAME) printf("reinstalled %s %s (same version)\n", info.id, info.version);
    else if (how == MOD_STORE_SAME_CHANGED)
        printf("reinstalled %s %s (same version); its files changed but its version did not: raise version= in mod.ini\n", info.id, info.version);
    else printf("installed %s %s\n", info.id, info.version);
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

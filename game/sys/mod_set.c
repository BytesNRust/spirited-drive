/* See mod_set.h. */
#include "game/sys/mod_set.h"

#include "game/platform/platform.h"
#include "game/sys/data_path.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum { MOD_SET_MAX = MOD_LIST_MAX + 1, DIR_MAX = 1100 };   /* the list, and --mods above it */

typedef struct {
    int count;
    char dirs[MOD_SET_MAX][DIR_MAX];
} Folders;

static struct {
    bool started;
    char data_root[1024];
    char cli[1024];
    Folders now;
    uint32_t generation;
} set = { .generation = 1 };

static bool is_dir(const char *path)
{
    struct stat st;
    return path[0] != '\0' && stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

/* <player folder>/mods/ with its '/', or "" when there is no player folder */
static void mods_root(char *out, size_t cap)
{
    char user[1024];
    out[0] = '\0';
    if (platform_user_dir(user, sizeof user)) snprintf(out, cap, "%smods/", user);
}

/* the folders that should be in effect: in Restomod, --mods then the list (a listed mod with no folder left out); in Stock and Restored
 * none, --mods neither. What is left out is said in the log when `say`. */
static Folders wanted(bool say)
{
    Folders f = { 0 };
    const PortSettings s = port_settings_load();
    if (s.profile != PROFILE_RESTOMOD) {
        if (say && set.cli[0] != '\0') port_log("mods: --mods %s is ignored: %s loads no mods (Restomod only)\n", set.cli, port_profile_key((Profile)s.profile));
        return f;
    }
    if (set.cli[0] != '\0') snprintf(f.dirs[f.count++], DIR_MAX, "%s", set.cli);
    ModList list;
    mod_list_parse(s.mods, &list);
    char root[1024];
    mods_root(root, sizeof root);
    for (int i = 0; i < list.count && root[0] != '\0'; i++) {
        snprintf(f.dirs[f.count], DIR_MAX, "%s%s", root, list.ids[i]);
        if (is_dir(f.dirs[f.count])) f.count++;
        else if (say) port_log("mods: %s is in the list but %s is not a folder: left out\n", list.ids[i], f.dirs[f.count]);
    }
    return f;
}

static bool same(const Folders *a, const Folders *b)
{
    if (a->count != b->count) return false;
    for (int i = 0; i < a->count; i++)
        if (strcmp(a->dirs[i], b->dirs[i]) != 0) return false;
    return true;
}

/* `f` given to the overlay and the port's data; the generation moves on */
static void put_in_effect(const Folders *f)
{
    const char *dirs[MOD_SET_MAX];
    for (int i = 0; i < f->count; i++) dirs[i] = f->dirs[i];
    if (!platform_set_mods(set.data_root, dirs, f->count)) {   /* a folder gone since it was looked at: none, rather than some */
        platform_set_mods(set.data_root, NULL, 0);
        set.now.count = 0;
    } else {
        set.now = *f;
    }
    data_path_set_mods(dirs, set.now.count);
    set.generation = set.generation == UINT32_MAX ? 1 : set.generation + 1;
    char line[600] = "";
    for (int i = 0; i < set.now.count; i++) {
        const char *name = strrchr(set.now.dirs[i], '/');
        snprintf(line + strlen(line), sizeof line - strlen(line), "%s%s", i > 0 ? ", " : "", name != NULL && name[1] != '\0' ? name + 1 : set.now.dirs[i]);
    }
    port_log("mods: in effect: %s\n", set.now.count > 0 ? line : "none");
}

static void follow(void)
{
    if (!set.started) return;
    const Folders f = wanted(false);
    if (!same(&f, &set.now)) put_in_effect((wanted(true), &f));
}

bool mod_set_start(const char *data_root, const char *cli_dir)
{
    if (cli_dir != NULL && cli_dir[0] != '\0' && port_settings_profile() == PROFILE_RESTOMOD && !is_dir(cli_dir)) {   /* elsewhere ignored */
        port_fatal("mods: --mods %s does not exist or is not a folder\n", cli_dir);
        return false;
    }
    snprintf(set.data_root, sizeof set.data_root, "%s", data_root != NULL ? data_root : "");
    snprintf(set.cli, sizeof set.cli, "%s", cli_dir != NULL ? cli_dir : "");
    set.started = true;
    port_settings_on_forget(follow);
    const Folders f = wanted(true);
    put_in_effect(&f);   /* always, so a new start is told to the overlay even with the same folders */
    return true;
}

void mod_set_stop(void)
{
    port_settings_on_forget(NULL);
    set.started = false;
    set.now.count = 0;
    platform_set_mods(set.data_root, NULL, 0);
    data_path_set_mods(NULL, 0);
    set.generation = set.generation == UINT32_MAX ? 1 : set.generation + 1;
}

uint32_t mod_set_generation(void) { return set.generation; }

int mod_set_dirs(const char *dirs[], int cap)
{
    int n = 0;
    for (; n < set.now.count && n < cap; n++) dirs[n] = set.now.dirs[n];
    return n;
}

bool mod_set_save_list(const ModList *l)
{
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    const PortExtras e = port_extras_of_profile(PROFILE_RESTOMOD);
    if (!mod_list_format(l, s.mods, sizeof s.mods)) return false;
    return port_mode_save(&s, &e);   /* its write moves the settings on, and the folders follow */
}

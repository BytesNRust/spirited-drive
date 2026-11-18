/* See mod_set.h. */
#include "game/sys/mod_set.h"

#include "formats/mod_check.h"
#include "formats/mod_ini.h"
#include "formats/stock_content.h"
#include "game/platform/platform.h"
#include "game/sys/data_path.h"
#include "game/sys/edition_file.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum { MOD_SET_MAX = 1 + EDITION_TWEAKS_MAX + EDITION_LIST_MAX, DIR_MAX = 1100, FOLDERS_MAX = PLATFORM_MODS_MAX };   /* --mods, an edition's tweaks, cars and tracks: the platform takes no more (the rest left out, said) */

typedef struct {
    int count;
    char dirs[MOD_SET_MAX][DIR_MAX];
} Folders;

static struct {
    bool started;
    char data_root[1024];
    char cli[1024];
    Folders now;
} set;

/* port (dep59 step 2): what each folder in effect is, by its card (none: a tweak), so that it serves only what its kind may carry
 * (mod_file_rule); --mods serves everything */
typedef struct {
    char id[MOD_INI_ID_MAX + 1];   /* the card's, else the folder's name */
    int type;
    char stem[MOD_INI_STEM_MAX + 1];
    bool cli;
} Piece;
enum { SAID_MAX = 64, SAID_LEN = 300 };
static struct {
    Piece pieces[MOD_SET_MAX];
    char said[SAID_MAX][SAID_LEN];   /* "<id>/<file>" left out and said in this generation */
    int said_count;
} serving;

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

/* the folders that should be in effect: in Restomod, --mods, then the edition in use's (edition_file.h): its tweaks in their order, then
 * its added cars' folders, then its added tracks' (a piece with no folder left out; a stock car or track has none); in Stock and Restored none, --mods neither. What is
 * left out is said in the log when `say`. */
/* (into `out`: a Folders is large, kept off the stack) */
static const Folders *wanted(bool say, Folders *out)
{
    Folders *const f = out;
    f->count = 0;
    const PortSettings s = port_settings_load();
    if (s.profile != PROFILE_RESTOMOD) {
        if (say && set.cli[0] != '\0') port_log("mods: --mods %s is ignored: %s loads no mods (Restomod only)\n", set.cli, port_profile_key((Profile)s.profile));
        return f;
    }
    if (set.cli[0] != '\0') snprintf(f->dirs[f->count++], DIR_MAX, "%s", set.cli);
    static Edition e;
    if (edition_active(&e) != 1) return f;
    char root[1024];
    mods_root(root, sizeof root);
    for (int i = 0; i < e.tweaks + e.cars + e.tracks && root[0] != '\0' && f->count < FOLDERS_MAX; i++) {
        const int tweak = i < e.tweaks, car = !tweak && i < e.tweaks + e.cars;
        const char *const id = tweak ? e.tweak[i] : car ? e.car[i - e.tweaks] : e.track[i - e.tweaks - e.cars];
        if (car && (stock_car_of_id(id) >= 0 || !edition_car_present(id))) {
            if (say && stock_car_of_id(id) < 0) port_log("mods: the car %s of the edition is not installed: left out\n", id);
            continue;
        }
        if (!tweak && !car && (stock_track_of_id(id) >= 0 || !edition_track_present(id))) {   /* port (dep59 step 5, T8): its added tracks' folders last */
            if (say && stock_track_of_id(id) < 0) port_log("mods: the track %s of the edition is not installed: left out\n", id);
            continue;
        }
        snprintf(f->dirs[f->count], DIR_MAX, "%s%s", root, id);
        if (is_dir(f->dirs[f->count])) f->count++;
        else if (say) port_log("mods: %s of the edition is not installed (%s is not a folder): left out\n", id, f->dirs[f->count]);
    }
    if (say && f->count >= FOLDERS_MAX) port_log("mods: at most %d folders are in effect: the edition's last ones may be left out\n", FOLDERS_MAX);
    return f;
}

static bool same(const Folders *a, const Folders *b)
{
    if (a->count != b->count) return false;
    for (int i = 0; i < a->count; i++)
        if (strcmp(a->dirs[i], b->dirs[i]) != 0) return false;
    return true;
}

/* the piece in folder `dir`: its card's id, type and stem; a folder with no card, or one that does not parse, is a tweak by its name */
static Piece piece_of(const char *dir, bool cli)
{
    Piece p = { "", MOD_TYPE_TWEAK, "", cli };
    const char *name = strrchr(dir, '/');
    snprintf(p.id, sizeof p.id, "%s", name != NULL && name[1] != '\0' ? name + 1 : dir);
    char path[DIR_MAX + 16];
    snprintf(path, sizeof path, "%s/mod.ini", dir);
    FILE *file = fopen(path, "rb");
    if (file == NULL) return p;
    static uint8_t text[MOD_INI_MAX_SIZE + 1];
    const size_t n = fread(text, 1, sizeof text, file);
    fclose(file);
    ModInfo card;
    char why[160];
    if (n > MOD_INI_MAX_SIZE || !mod_ini_parse(text, n, &card, why, sizeof why)) {
        port_log("mods: %s: its mod.ini is not read (%s): taken as a tweak\n", p.id, n > MOD_INI_MAX_SIZE ? "too long" : why);
        return p;
    }
    snprintf(p.id, sizeof p.id, "%s", card.id);
    snprintf(p.stem, sizeof p.stem, "%s", card.stem);
    p.type = card.type;
    return p;
}

/* an installed folder's file left out, said once in each generation */
static void say_left_out(const Piece *p, const char *rel, ModFileRule rule)
{
    char key[SAID_LEN];
    snprintf(key, sizeof key, "%s/%s", p->id, rel);
    for (int i = 0; i < serving.said_count; i++)
        if (strcasecmp(serving.said[i], key) == 0) return;
    if (serving.said_count < SAID_MAX) snprintf(serving.said[serving.said_count++], SAID_LEN, "%s", key);
    port_log("mods: %s: %s is left out: %s\n", p->id, rel,
             rule != MOD_FILE_RACE ? "it is not one of this piece's files"
             : p->cli ? (strncasecmp(rel, "SIMDATA/CAR", 11) == 0 ? "a changed car is a car piece now: tnfs-studio car new"
                                                                    : "a changed track is a track piece now: tnfs-studio track new --piece")
                      : "it changes the race, which an installed tweak may not");
}

static bool serve(int index, const char *rel)
{
    if (index < 0 || index >= set.now.count) return true;
    const Piece *p = &serving.pieces[index];
    const ModFileRule rule = mod_file_rule(p->type, p->stem, rel);
    if (rule == MOD_FILE_OK) return true;
    if (p->cli && rule != MOD_FILE_RACE) return true;   /* --mods serves what does not change the race */
    /* the live light editor (light_edit=1) works on a track's places file, track/<TRACK>.txt, in the folder given as --mods: that file is served to it */
    if (p->cli && port_light_edit() && strncasecmp(rel, "track/", 6) == 0) {
        if (!mod_set_places_served()) {
            mod_set_places_mark();
            port_log("mods: --mods serves %s to the light editor: this run keeps no records and no replays\n", rel);
        }
        return true;
    }
    say_left_out(p, rel, rule);
    return false;
}

static int serve_data(int index, const char *relative) { return serve(index, relative); }

/* `f` given to the overlay and the port's data; the generation moves on */
static void put_in_effect(const Folders *f)
{
    for (int i = 0; i < f->count; i++) serving.pieces[i] = piece_of(f->dirs[i], i == 0 && set.cli[0] != '\0');
    serving.said_count = 0;
    platform_set_mod_serve(serve);
    data_path_set_serve(serve_data);
    const char *dirs[MOD_SET_MAX];
    for (int i = 0; i < f->count; i++) dirs[i] = f->dirs[i];
    if (!platform_set_mods(set.data_root, dirs, f->count)) {   /* a folder gone since it was looked at: none, rather than some */
        platform_set_mods(set.data_root, NULL, 0);
        set.now.count = 0;
    } else {
        set.now = *f;
    }
    data_path_set_mods(dirs, set.now.count);
    mod_set_generation_move();
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
    static Folders f;
    if (!same(wanted(false, &f), &set.now)) put_in_effect(wanted(true, &f));
}

bool mod_set_start(const char *data_root, const char *cli_dir)
{
    if (cli_dir != NULL && cli_dir[0] != '\0' && port_settings_profile() == PROFILE_RESTOMOD && !is_dir(cli_dir)) {   /* elsewhere ignored */
        port_fatal("mods: --mods %s does not exist or is not a folder\n", cli_dir);
        return false;
    }
    snprintf(set.data_root, sizeof set.data_root, "%s", data_root != NULL ? data_root : "");
    snprintf(set.cli, sizeof set.cli, "%s", cli_dir != NULL ? cli_dir : "");
    mod_set_places_clear();   /* a new run */
    set.started = true;
    port_settings_on_forget(follow);
    static Folders f;
    wanted(true, &f);
    put_in_effect(&f);   /* always, so a new start is told to the overlay even with the same folders */
    return true;
}

void mod_set_stop(void)
{
    port_settings_on_forget(NULL);
    set.started = false;
    set.now.count = 0;
    mod_set_places_clear();
    platform_set_mods(set.data_root, NULL, 0);
    data_path_set_mods(NULL, 0);
    mod_set_generation_move();
}


int mod_set_dirs(const char *dirs[], int cap)
{
    int n = 0;
    for (; n < set.now.count && n < cap; n++) dirs[n] = set.now.dirs[n];
    return n;
}



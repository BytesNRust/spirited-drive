/* port (dep59 step 4): see edition_file.h. */
#include "game/sys/edition_file.h"

#include "formats/mod_ini.h"
#include "formats/stock_content.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TRACK_HIDDEN = 9, REL_MAX = EDITION_ID_MAX + 32 };

int edition_load(const char *id, Edition *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (!edition_id_ok(id)) return snprintf(why, why_cap, "\"%.40s\" is not an edition's id", id), 0;
    char rel[REL_MAX];
    snprintf(rel, sizeof rel, "editions/%s.ini", id);
    size_t size = 0;
    uint8_t *data = platform_user_read_file_at(rel, &size);
    if (data == NULL) return snprintf(why, why_cap, "%s is not there", rel), 0;
    const int ok = edition_parse(data, size, out, why, why_cap);
    platform_free_file(data);
    return ok;
}

int edition_car_present(const char *id)
{
    if (stock_car_of_id(id) >= 0) return 1;
    if (id == NULL || !mod_ini_id_ok(id)) return 0;
    char rel[REL_MAX];
    snprintf(rel, sizeof rel, "mods/%s/mod.ini", id);
    size_t size = 0;
    uint8_t *data = platform_user_read_file_at(rel, &size);
    if (data == NULL) return 0;
    ModInfo info;
    char why[160];
    const int car = mod_ini_parse(data, size, &info, why, sizeof why) && info.type == MOD_TYPE_CAR && strcmp(info.id, id) == 0;
    platform_free_file(data);
    return car;
}

int edition_track_present(const char *id)
{
    const int track = stock_track_of_id(id);
    return track >= 0 && track != TRACK_HIDDEN;
}

int edition_usable(const Edition *e, char *why, size_t why_cap)
{
    int cars = 0, tracks = 0;
    for (int i = 0; i < e->cars; i++) cars += edition_car_present(e->car[i]);
    for (int i = 0; i < e->tracks; i++) tracks += edition_track_present(e->track[i]);
    if (cars == 0) return snprintf(why, why_cap, "no car of it is there"), 0;
    if (tracks == 0) return snprintf(why, why_cap, "no track of it is there"), 0;
    return 1;
}

/* the line for (id, reason) once: the same one again is not logged */
static void say_once(const char *id, const char *why)
{
    static char said[EDITION_ID_MAX + 200];
    char line[sizeof said];
    snprintf(line, sizeof line, "%s: %s", id, why);
    if (strcmp(line, said) == 0) return;
    snprintf(said, sizeof said, "%s", line);
    port_log("edition: %s cannot be used (%s): the standard edition is\n", id, why);
}

int edition_active(Edition *out)
{
    memset(out, 0, sizeof *out);
    const PortSettings s = port_settings_load();
    if (s.profile != PROFILE_RESTOMOD || s.edition[0] == '\0') return 0;
    char why[200];
    if (!edition_load(s.edition, out, why, sizeof why) || !edition_usable(out, why, sizeof why)) {
        say_once(s.edition, why);
        memset(out, 0, sizeof *out);
        return -1;
    }
    return 1;
}

static int by_name(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

int edition_list(char ids[][EDITION_ID_MAX + 1], int cap)
{
    char dir[1024];
    if (!platform_user_dir(dir, sizeof dir)) return 0;
    if (strlen(dir) + sizeof "editions" >= sizeof dir) return 0;
    strcat(dir, "editions");
    int count = 0, n = 0;
    char **names = platform_list_files(dir, "*.ini", &count);
    for (int i = 0; i < count && n < cap; i++) {
        const size_t len = strlen(names[i]);
        if (len <= 4 || len - 4 > EDITION_ID_MAX || strcmp(names[i] + len - 4, ".ini") != 0) continue;   /* (the pattern ignores case) */
        char id[EDITION_ID_MAX + 1], rel[REL_MAX];
        memcpy(id, names[i], len - 4), id[len - 4] = '\0';
        snprintf(rel, sizeof rel, "editions/%s", names[i]);
        if (!edition_id_ok(id) || platform_user_is_dir(rel)) continue;
        snprintf(ids[n++], EDITION_ID_MAX + 1, "%s", id);
    }
    platform_free_names(names);
    qsort(ids, (size_t)n, sizeof ids[0], by_name);
    return n;
}

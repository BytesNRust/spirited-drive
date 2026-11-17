/* port (dep59 step 4): see edition_file.h. */
#include "game/sys/edition_file.h"

#include "formats/mod_ini.h"
#include "formats/stock_content.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/sys/track_registry.h"

#include <stdio.h>
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

/* port (dep59 step 5, T7b): an added track of an installed mod: the card at mods/<id>/mod.ini names a track of that id (as edition_car_present's) */
static int track_mod_installed(const char *id)
{
    if (id == NULL || !mod_ini_id_ok(id)) return 0;
    char rel[REL_MAX];
    snprintf(rel, sizeof rel, "mods/%s/mod.ini", id);
    size_t size = 0;
    uint8_t *data = platform_user_read_file_at(rel, &size);
    if (data == NULL) return 0;
    ModInfo info;
    char why[160];
    const int track = mod_ini_parse(data, size, &info, why, sizeof why) && info.type == MOD_TYPE_TRACK && strcmp(info.id, id) == 0;
    platform_free_file(data);
    return track;
}

int edition_track_present(const char *id)
{
    const int track = stock_track_of_id(id);
    if (track < 0) return track_registry_find(id) >= 0 || track_mod_installed(id);   /* an added track: registered (T7b) or installed */
    return track != TRACK_HIDDEN;
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

const Edition *edition_in_use(Edition *buf) { return edition_active(buf) == 1 ? buf : NULL; }

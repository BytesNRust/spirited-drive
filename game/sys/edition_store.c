/* port (dep59 step 4b, T2): see edition_store.h. */
#include "game/sys/edition_store.h"

#include "formats/stock_content.h"
#include "game/sys/edition.h"
#include "game/sys/edition_file.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

enum { REL_MAX = EDITION_ID_MAX + 32, STANDARD_CARS = 8, STANDARD_TRACKS = 8 };

static void say(char *why, size_t cap, const char *text)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s", text);
}

static void rel_of(const char *id, const char *tail, char out[REL_MAX])
{
    snprintf(out, REL_MAX, "editions/%s.ini%s", id, tail);
}

static int there(const char *id)
{
    char rel[REL_MAX];
    rel_of(id, "", rel);
    return platform_user_exists(rel);
}

void edition_store_new_id(const char *name, char out[EDITION_ID_MAX + 1])
{
    char stem[EDITION_ID_MAX + 1];
    size_t n = 0;
    for (const char *p = name; *p != '\0' && n < EDITION_ID_MAX - 3; p++) {   /* room kept for "-NN" */
        const char c = *p >= 'A' && *p <= 'Z' ? (char)(*p - 'A' + 'a') : *p;
        const int keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (keep) stem[n++] = c;
        else if (n > 0 && stem[n - 1] != '-') stem[n++] = '-';
    }
    while (n > 0 && stem[n - 1] == '-') n--;
    stem[n] = '\0';
    if (!edition_id_ok(stem)) snprintf(stem, sizeof stem, "edition");
    snprintf(out, EDITION_ID_MAX + 1, "%s", stem);
    for (int k = 2; there(out) && k < 100; k++) snprintf(out, EDITION_ID_MAX + 1, "%s-%d", stem, k);
}

/* the text written whole beside the file, then put in its place */
static int write_whole(const char *id, const char *text, size_t size, char *why, size_t why_cap)
{
    char rel[REL_MAX], fresh[REL_MAX];
    rel_of(id, "", rel), rel_of(id, ".new", fresh);
    if (!platform_user_mkdirs("editions") || !platform_user_write_file_at(fresh, (const uint8_t *)text, size)) {
        say(why, why_cap, "The edition could not be saved");
        return 0;
    }
    if ((platform_user_exists(rel) && !platform_user_remove_tree(rel)) || !platform_user_rename(fresh, rel)) {
        port_log("edition_store: %s could not be put in place of %s\n", fresh, rel);
        say(why, why_cap, "The edition could not be saved");
        return 0;
    }
    return 1;
}

int edition_store_save(const char *id, const Edition *e, char *why, size_t why_cap)
{
    if (id == NULL || !edition_id_ok(id)) {
        say(why, why_cap, "That is not an edition's id");
        return 0;
    }
    if (!edition_name_ok(e->name, why, why_cap)) return 0;
    static char text[EDITION_MAX_SIZE];
    const int size = edition_format(e, text, sizeof text);
    if (size <= 0) {
        say(why, why_cap, "The edition is too long to save");
        return 0;
    }
    if (!write_whole(id, text, (size_t)size, why, why_cap)) return 0;
    port_settings_forget();   /* the edition in use read again: nothing kept from before the save */
    port_log("edition_store: saved %s\n", id);
    return 1;
}

/* `e` saved under a new id from its name, into `id_out` */
static int save_new(const Edition *e, char id_out[EDITION_ID_MAX + 1], char *why, size_t why_cap)
{
    if (!edition_name_ok(e->name, why, why_cap)) return 0;
    char id[EDITION_ID_MAX + 1];
    edition_store_new_id(e->name, id);
    if (there(id)) {
        say(why, why_cap, "Too many editions with that name");
        return 0;
    }
    if (!edition_store_save(id, e, why, why_cap)) return 0;
    snprintf(id_out, EDITION_ID_MAX + 1, "%s", id);
    return 1;
}

int edition_store_create(const char *name, char id_out[EDITION_ID_MAX + 1], char *why, size_t why_cap)
{
    static Edition e;
    memset(&e, 0, sizeof e);
    if (strlen(name) > EDITION_NAME_MAX) return edition_name_ok(name, why, why_cap);   /* its reason */
    snprintf(e.name, sizeof e.name, "%s", name);
    for (int i = 0; i < STANDARD_CARS; i++) snprintf(e.car[e.cars++], sizeof e.car[0], "%s", stock_car_id(i));
    for (int i = 0; i < STANDARD_TRACKS; i++) snprintf(e.track[e.tracks++], sizeof e.track[0], "%s", stock_track_id(i));
    return save_new(&e, id_out, why, why_cap);
}

int edition_store_duplicate(const char *from, const char *name, char id_out[EDITION_ID_MAX + 1], char *why, size_t why_cap)
{
    static Edition e;
    if (!edition_load(from, &e, why, why_cap)) return 0;
    if (strlen(name) > EDITION_NAME_MAX) return edition_name_ok(name, why, why_cap);
    snprintf(e.name, sizeof e.name, "%s", name);
    return save_new(&e, id_out, why, why_cap);
}

int edition_store_rename(const char *id, const char *name, char *why, size_t why_cap)
{
    static Edition e;
    if (!edition_load(id, &e, why, why_cap)) return 0;
    if (strlen(name) > EDITION_NAME_MAX) return edition_name_ok(name, why, why_cap);
    snprintf(e.name, sizeof e.name, "%s", name);
    return edition_store_save(id, &e, why, why_cap);
}

int edition_store_delete(const char *id, char *why, size_t why_cap)
{
    char rel[REL_MAX];
    if (id == NULL || !edition_id_ok(id) || !there(id)) {
        say(why, why_cap, "There is no such edition");
        return 0;
    }
    if (strcmp(port_settings_of_profile(PROFILE_RESTOMOD).edition, id) == 0 && edition_set(NULL) < 0) {
        say(why, why_cap, "The standard edition could not be put back");
        return 0;
    }
    rel_of(id, "", rel);
    if (!platform_user_remove_tree(rel)) {
        say(why, why_cap, "The edition could not be deleted");
        return 0;
    }
    port_settings_forget();
    port_log("edition_store: deleted %s\n", id);
    return 1;
}

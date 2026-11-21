/* The Car window's showcase photos: a picture the person chose is copied into the car's folder under a safe name and named on the card's photos= line (the card
 * edit's undo covers the line). The files Studio made stay in the folder until Save, which removes those the card no longer names: an unnamed PNG with a photo's
 * name would make the piece fail its check (PIECE.PHOTO), and undo must be able to bring a removed photo back until then. */
#include "app.h"
#include "car_photos.h"
#include "formats/mod_ini.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { NAME = MOD_INI_PHOTO_NAME_MAX + 1, TAKEN_MAX = 64 };

void app_photos_dir(const App *a, char *out, size_t cap)
{
    car_project_photos_dir(&a->car, out, cap);
}

static void say(App *a, const char *fmt, const char *what)
{
    snprintf(a->status, sizeof a->status, fmt, what);
}

static int list_of(const App *a, char (*names)[NAME])
{
    return car_photos_split(a->edit.now.value[CE_PHOTOS], names, MOD_INI_PHOTOS_MAX);
}

static int set_list(App *a, char (*names)[NAME], int n)
{
    char joined[CE_VALUE_MAX];
    car_photos_join(names, n, joined, sizeof joined);
    return car_edit_set(&a->edit, CE_PHOTOS, joined);
}

int app_add_photo(App *a, const char *source)
{
    char names[MOD_INI_PHOTOS_MAX][NAME], taken[TAKEN_MAX][NAME], name[NAME], path[4200], why[200];
    if (a->kind != APP_KIND_CAR || a->car.masks_only) return say(a, "%s", "a showcase photo needs a car with a card"), 0;
    char dir[APP_PATH];
    app_photos_dir(a, dir, sizeof dir);
    int n = list_of(a, names);
    if (n >= MOD_INI_PHOTOS_MAX) return say(a, "the showcase keeps %s photos: remove one first", "8"), 0;
    if (a->photos_made_n >= APP_PHOTOS_MADE) return say(a, "%s", "Save first: then more photos can be added"), 0;
    int taken_n = 0;
    for (; taken_n < n; taken_n++) memcpy(taken[taken_n], names[taken_n], NAME);
    for (int tries = 0; tries < TAKEN_MAX; tries++) {   /* a name that is a file of the folder already is taken too: never overwrite the author's picture */
        if (!car_photos_safe_name(source, taken, taken_n, name, sizeof name)) return say(a, "no file name can be made for %s", source), 0;
        snprintf(path, sizeof path, "%s/%s", dir, name);
        char staged[4210];
        snprintf(staged, sizeof staged, "%s.new", path);
        if (access(path, F_OK) != 0 && access(staged, F_OK) != 0) break;
        if (taken_n == TAKEN_MAX) return say(a, "no free file name for %s", source), 0;
        memcpy(taken[taken_n++], name, NAME);
        name[0] = '\0';
    }
    if (name[0] == '\0') return say(a, "no free file name for %s", source), 0;
    char stage[NAME + 8];
    snprintf(stage, sizeof stage, "%s.new", name);   /* waits as name.new until Save: an unsaved photo is no photo of the piece */
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) return say(a, "the car's photos folder cannot be made: %s", strerror(errno)), 0;   // a kit's photos/ appears with its first photo
    if (!car_photos_import(source, dir, stage, why, sizeof why)) return say(a, "%s", why), 0;
    memcpy(a->photos_made[a->photos_made_n++], name, NAME);
    memcpy(names[n++], name, NAME);
    set_list(a, names, n);
    say(a, "added %s to the showcase photos: Save to keep it", name);
    return 1;
}

int app_photo_remove(App *a, int index)
{
    char names[MOD_INI_PHOTOS_MAX][NAME];
    int n = list_of(a, names);
    if (index < 0 || index >= n) return 0;
    for (int i = index; i + 1 < n; i++) memcpy(names[i], names[i + 1], NAME);
    set_list(a, names, n - 1);
    return 1;
}

int app_photo_move(App *a, int index, int by)
{
    char names[MOD_INI_PHOTOS_MAX][NAME], keep[NAME];
    const int n = list_of(a, names), to = index + by;
    if (index < 0 || index >= n || to < 0 || to >= n || by == 0) return 0;
    memcpy(keep, names[index], NAME);
    memcpy(names[index], names[to], NAME);
    memcpy(names[to], keep, NAME);
    set_list(a, names, n);
    return 1;
}

/* the card's photos= and photo_small= as just saved: what the card names, which the sweep must not remove */
static void named_by_card(const App *a, char *out, size_t cap)
{
    static char text[MOD_INI_MAX_SIZE + 1];
    ModInfo info;
    char why[200];
    snprintf(out, cap, "%s", a->edit.now.value[CE_PHOTOS]);
    FILE *f = fopen(a->edit.path, "rb");
    if (f == NULL) return;
    const size_t n = fread(text, 1, MOD_INI_MAX_SIZE, f);
    fclose(f);
    if (mod_ini_parse((const uint8_t *)text, n, &info, why, sizeof why) && info.photo_small[0] != '\0') {
        const size_t used = strlen(out);
        snprintf(out + used, cap - used, "%s%s", used > 0 ? "," : "", info.photo_small);
    }
}

void app_photos_swept(App *a)
{
    char dir[APP_PATH], listed[CE_VALUE_MAX + MOD_INI_PHOTO_NAME_MAX + 2];
    app_photos_dir(a, dir, sizeof dir);
    named_by_card(a, listed, sizeof listed);
    if (car_photos_commit(dir, a->edit.now.value[CE_PHOTOS]) < 0) return;
    car_photos_sweep(dir, a->photos_made, &a->photos_made_n, listed);
}

int app_photos_missing(const App *a, char *why, size_t cap)
{
    char names[MOD_INI_PHOTOS_MAX][NAME], dir[APP_PATH];
    app_photos_dir(a, dir, sizeof dir);
    const int n = list_of(a, names);
    for (int i = 0; i < n; i++) {
        uint32_t w, h;
        if (car_photos_size(dir, names[i], &w, &h)) continue;
        snprintf(why, cap, "%s is on the card's photos list but not in the car's folder: remove it from the list", names[i]);
        return 1;
    }
    return 0;
}

int app_photo_file_ok(const App *a, const char *name)
{
    char dir[APP_PATH], path[4200];
    app_photos_dir(a, dir, sizeof dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    return access(path, F_OK) == 0;
}

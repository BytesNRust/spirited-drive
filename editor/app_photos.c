/* The Car window's showcase photos: a picture the person chose is copied into the car's folder under a safe name and named on the card's photos= line (the card
 * edit's undo covers the line). The files Studio made stay in the folder until Save, which removes those the card no longer names: an unnamed PNG with a photo's
 * name would make the piece fail its check (PIECE.PHOTO), and undo must be able to bring a removed photo back until then. */
#include "app.h"
#include "car_photos.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { NAME = MOD_INI_PHOTO_NAME_MAX + 1, TAKEN_MAX = 64 };

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
    int n = list_of(a, names);
    if (n >= MOD_INI_PHOTOS_MAX) return say(a, "the showcase keeps %s photos: remove one first", "8"), 0;
    if (a->photos_made_n >= APP_PHOTOS_MADE) return say(a, "%s", "Save first: then more photos can be added"), 0;
    int taken_n = 0;
    for (; taken_n < n; taken_n++) memcpy(taken[taken_n], names[taken_n], NAME);
    for (int tries = 0; tries < TAKEN_MAX; tries++) {   /* a name that is a file of the folder already is taken too: never overwrite the author's picture */
        if (!car_photos_safe_name(source, taken, taken_n, name, sizeof name)) return say(a, "no file name can be made for %s", source), 0;
        snprintf(path, sizeof path, "%s/%s", a->car.piece, name);
        if (access(path, F_OK) != 0) break;
        if (taken_n == TAKEN_MAX) return say(a, "no free file name for %s", source), 0;
        memcpy(taken[taken_n++], name, NAME);
        name[0] = '\0';
    }
    if (name[0] == '\0') return say(a, "no free file name for %s", source), 0;
    if (!car_photos_import(source, a->car.piece, name, why, sizeof why)) return say(a, "%s", why), 0;
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

void app_photos_swept(App *a)
{
    const char *listed = a->edit.now.value[CE_PHOTOS];
    car_photos_sweep(a->car.piece, a->photos_made, a->photos_made_n, listed);
    char names[MOD_INI_PHOTOS_MAX][NAME];
    const int n = car_photos_split(listed, names, MOD_INI_PHOTOS_MAX);
    int kept = 0;
    for (int i = 0; i < a->photos_made_n; i++) {   /* what Studio made and the card still names stays Studio's */
        for (int k = 0; k < n; k++)
            if (strcmp(names[k], a->photos_made[i]) == 0) {
                if (kept != i) memcpy(a->photos_made[kept], a->photos_made[i], NAME);
                kept++;
                break;
            }
    }
    a->photos_made_n = kept;
}

int app_photo_file_ok(const App *a, const char *name)
{
    char path[4200];
    snprintf(path, sizeof path, "%s/%s", a->car.piece, name);
    return access(path, F_OK) == 0;
}

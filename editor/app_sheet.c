#include "app_sheet.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

enum { CAR_COLS = 2, TRAFFIC_COLS = 4 };

static void say(App *a, const char *text)
{
    snprintf(a->status, sizeof a->status, "%s", text);
}

int app_sheet_busy(const App *a)
{
    return a->sheet_active;
}

static const char *track_stem(const App *a)
{
    const RaceTrack *t = car_race_track(a->race_track);
    return t != NULL ? t->stem : "";
}

/* the cells of the open car's sheet */
static int build_car(App *a, const ExportCar *car, char *why, size_t cap)
{
    const RaceTrack *t = car_race_track(a->race_track);
    if (car->kind != EXPORT_DRIVABLE && (t == NULL || !t->stage)) return snprintf(why, cap, "pick a stage track: a circuit has no traffic"), 0;
    sheet_init(&a->sheet, CAR_COLS);
    sheet_add(&a->sheet, "Day", car, a->car.piece, track_stem(a), 0);
    sheet_add(&a->sheet, "Night", car, a->car.piece, track_stem(a), 1);
    return 1;
}

/* the cells of the traffic sheet: each non-drivable car's project beside the open one */
static int build_traffic(App *a, char *why, size_t cap)
{
    char parent[APP_PATH];
    snprintf(parent, sizeof parent, "%s", a->car.root);
    char *up = dirname(parent);
    sheet_init(&a->sheet, TRAFFIC_COLS);
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        const ExportCar *car = car_export_car(i);
        if (car->kind == EXPORT_DRIVABLE) continue;
        char name[96], dir[APP_PATH];
        car_export_folder_name(i, name, sizeof name);
        snprintf(dir, sizeof dir, "%s/%s", up, name);
        CarProject found;
        char reason[APP_MSG];
        const int idx = sheet_add(&a->sheet, car->label, car, dir, car_race_track(car_race_default_track(car)) != NULL ? car_race_track(car_race_default_track(car))->stem : "",
                                  a->race_night);
        if (idx < 0) break;
        if (!car_project_detect(dir, &found, reason, sizeof reason) || !found.masks_only || car_race_original_of(&found) != car) {
            snprintf(reason, sizeof reason, "no project %s beside this one: Export original car makes it", name);
            sheet_set_failed(&a->sheet, idx, reason);
        }
    }
    if (a->sheet.count == 0) return snprintf(why, cap, "no traffic car to photograph"), 0;
    return 1;
}

int app_sheet_start(App *a, int traffic)
{
    char why[APP_MSG];
    const ExportCar *car = a->kind == APP_KIND_CAR ? car_race_original_of(&a->car) : NULL;
    if (a->sheet_active || car_race_busy(&a->race)) return say(a, "wait for the run in progress, or stop it"), 0;
    if (car == NULL) return say(a, "a sheet is of the game's own cars: open a project made with Export original car"), 0;
    sheet_free(&a->sheet);
    if (!(traffic ? build_traffic(a, why, sizeof why) : build_car(a, car, why, sizeof why))) return say(a, why), 0;
    a->sheet_cur = -1;
    a->sheet_stop = 0;
    a->sheet_active = 1;
    a->sheet_serial++;
    a->sheet_saved[0] = '\0';
    say(a, traffic ? "taking the traffic sheet..." : "taking the sheet...");
    return 1;
}

static void finish(App *a)
{
    int done = 0;
    for (int i = 0; i < a->sheet.count; ++i) done += a->sheet.cell[i].state == SHEET_DONE;
    a->sheet_active = 0;
    a->sheet_cur = -1;
    a->sheet_serial++;
    snprintf(a->status, sizeof a->status, "sheet: %d of %d photos%s", done, a->sheet.count, a->sheet_stop ? " (stopped)" : "");
}

/* the photo job ended: its picture or its reason into the cell it was for */
static void take(App *a)
{
    const RaceJob *j = &a->race;
    if (j->ok && j->photo != NULL) sheet_set_photo(&a->sheet, a->sheet_cur, j->photo, j->photo_w, j->photo_h);
    else sheet_set_failed(&a->sheet, a->sheet_cur, j->summary[0] != '\0' ? j->summary : "no photo");
    a->sheet_cur = -1;
    a->sheet_serial++;
}

int app_sheet_poll(App *a)
{
    if (!a->sheet_active || car_race_busy(&a->race)) return a->sheet_active;
    if (a->sheet_cur >= 0) take(a);
    for (;;) {
        const int i = a->sheet_stop ? -1 : sheet_next(&a->sheet);
        if (i < 0) {
            for (int k = 0; k < a->sheet.count; ++k)
                if (a->sheet.cell[k].state == SHEET_PENDING) sheet_set_failed(&a->sheet, k, "not taken: stopped");
            finish(a);
            return 0;
        }
        const SheetCell *c = &a->sheet.cell[i];
        char why[APP_MSG];
        if (car_race_start_photo(&a->race, a->game, a->export_src.cd, c->project, c->car, c->track, c->night, &a->race_look, why, sizeof why)) {
            a->sheet_cur = i;
            snprintf(a->status, sizeof a->status, "sheet: photo %d of %d (%s)...", i + 1, a->sheet.count, c->label);
            return 1;
        }
        sheet_set_failed(&a->sheet, i, why);
        a->sheet_serial++;
    }
}

void app_sheet_stop(App *a)
{
    if (!a->sheet_active) return;
    a->sheet_stop = 1;
    if (car_race_busy(&a->race)) car_race_stop(&a->race);
}

int app_sheet_save(App *a)
{
    char dir[APP_PATH], path[APP_PATH + 64], why[APP_MSG];
    if (a->sheet.count == 0 || a->sheet_active) return say(a, a->sheet_active ? "the sheet is still being taken" : "no sheet yet: take one first"), 0;
    if (snprintf(dir, sizeof dir, "%s/sheets", a->car.root) >= (int)sizeof dir) return say(a, "the project's path is too long"), 0;
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) return snprintf(a->status, sizeof a->status, "cannot make %s: %s", dir, strerror(errno)), 0;
    const time_t now = time(NULL);
    struct tm tm;
    char stamp[32];
    localtime_r(&now, &tm);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    snprintf(path, sizeof path, "%s/sheet-%s.png", dir, stamp);
    if (!sheet_write_png(&a->sheet, path, why, sizeof why)) return say(a, why), 0;
    snprintf(a->sheet_saved, sizeof a->sheet_saved, "%s", path);
    snprintf(a->reveal, sizeof a->reveal, "%s", dir);
    snprintf(a->status, sizeof a->status, "sheet saved: %s", path);
    return 1;
}

void app_sheet_describe(const App *a, char *out, size_t cap)
{
    static const char *const STATE[] = { "pending", "done", "failed" };
    size_t used = (size_t)snprintf(out, cap, "sheet %d:", a->sheet.count);
    for (int i = 0; i < a->sheet.count && used < cap; ++i)
        used += (size_t)snprintf(out + used, cap - used, " %s=%s", a->sheet.cell[i].label, STATE[a->sheet.cell[i].state]);
}

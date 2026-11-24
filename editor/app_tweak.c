/* Edit tweak, step 1, at the App level (app.h's app_tweak_*): the tweak folder of the open original car or track in <workspace>/tweaks/<id>/, and Apply to the game repo. No window yet: the steps
 * tweak-make and tweak-apply drive it. */
#include "app.h"
#include "car_race.h"
#include "tweak.h"

#include <stdio.h>
#include <string.h>

static int car_target(const App *a, char *dir, size_t cap, char *why, size_t why_cap)
{
    const ExportCar *car = a->kind == APP_KIND_CAR ? car_race_original_of(&a->car) : NULL;
    char id[64];
    if (car == NULL) return snprintf(why, why_cap, "only one of the game's own cars has a tweak"), 0;
    if (a->workspace[0] == '\0') return snprintf(why, why_cap, "choose a workspace first"), 0;
    tweak_id(car->folder, id, sizeof id);
    snprintf(dir, cap, "%s/tweaks/%s", a->workspace, id);
    return 1;
}

int app_tweak_make(App *a)
{
    char dir[APP_PATH], why[APP_MSG] = "", masks[APP_PATH] = "", dash[APP_PATH] = "", psd[APP_PATH] = "";
    if (a->kind == APP_KIND_CAR) {
        const ExportCar *car = car_race_original_of(&a->car);
        if (!car_target(a, dir, sizeof dir, why, sizeof why)) return snprintf(a->status, sizeof a->status, "tweak: %s", why), 0;
        if (a->car.has_masks_dir) car_project_masks_dir(&a->car, masks, sizeof masks);
        car_project_dash_dir(&a->car, dash, sizeof dash);
        car_project_edit_dir(&a->car, psd, sizeof psd);
        const int colours = car->kind == EXPORT_DRIVABLE && !a->car.masks_only;
        if (!tweak_make_car(dir, car, masks, dash, psd, a->edit.now.paints, colours ? a->edit.now.paint_count : 0, why, sizeof why))
            return snprintf(a->status, sizeof a->status, "tweak: %s", why), 0;
        return snprintf(a->status, sizeof a->status, "tweak: %s", dir), 1;
    }
    char stem[16], light[APP_PATH + 64], gen[APP_PATH + 64];
    if (a->original_dir[0] == '\0' || !original_track_stem_of(a->original_dir, stem, sizeof stem)) return snprintf(a->status, sizeof a->status, "tweak: open one of the game's own cars or tracks first"), 0;
    if (a->workspace[0] == '\0') return snprintf(a->status, sizeof a->status, "tweak: choose a workspace first"), 0;
    char id[64];
    tweak_id(stem, id, sizeof id);
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, id);
    snprintf(light, sizeof light, "%s/light/track_light/%s.txt", a->original_dir, stem);
    snprintf(gen, sizeof gen, "%s/art-generated/track_masks", a->original_dir);
    snprintf(psd, sizeof psd, "%s/art-source/masks", a->original_dir);
    if (!tweak_make_track(dir, stem, light, gen, psd, why, sizeof why)) return snprintf(a->status, sizeof a->status, "tweak: %s", why), 0;
    return snprintf(a->status, sizeof a->status, "tweak: %s", dir), 1;
}

int app_tweak_apply(App *a, const char *id)
{
    TweakApplied done;
    char why[APP_MSG] = "", dir[APP_PATH];
    if (a->workspace[0] == '\0') return snprintf(a->status, sizeof a->status, "apply to game repo: choose a workspace first"), 0;
    snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, id);
    if (app_repo_data(a)[0] == '\0') return snprintf(a->status, sizeof a->status, "apply to game repo: the game repo's data/ is not set (File > Game repo folder...)"), 0;
    if (!tweak_apply(dir, app_repo_data(a), &done, why, sizeof why)) return snprintf(a->status, sizeof a->status, "apply to game repo: %s", why), 0;
    return snprintf(a->status, sizeof a->status, "applied to the game repo: %d car mask%s, %d dash file%s, %d light file%s, %d track mask%s, colors %s",
        done.masks, done.masks == 1 ? "" : "s", done.dash, done.dash == 1 ? "" : "s", done.light, done.light == 1 ? "" : "s", done.track_masks, done.track_masks == 1 ? "" : "s", done.colors ? "written" : "unchanged"), 1;
}

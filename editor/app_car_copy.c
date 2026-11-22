/* Copy to game repo for one of the game's own cars (app.h's app_car_copy_*): the car's masks folder (art-generated/car_masks of its kit) against the repo's
 * data/car_masks/<folder>/, the changed and the new PNGs copied; nothing is committed. */
#include "app.h"
#include "car_race.h"
#include "mask_changes.h"

#include <stdio.h>
#include <string.h>

/* the car's masks folder and the repo's for it; 0 with the reason in `why` (not one of the game's cars, no masks yet, no repo) */
static int folders(const App *a, char *project, size_t pcap, char *repo, size_t rcap, const ExportCar **car, char *why, size_t why_cap)
{
    *car = a->kind == APP_KIND_CAR ? car_race_original_of(&a->car) : NULL;
    if (*car == NULL) return snprintf(why, why_cap, "only one of the game's own cars has masks in the game repo"), 0;
    if (app_repo_data(a)[0] == '\0') return snprintf(why, why_cap, "the game repo's data/ is not set (File > Game repo folder...)"), 0;
    if (!a->car.has_masks_dir) return snprintf(why, why_cap, "the car has no masks folder yet"), 0;
    car_project_masks_dir(&a->car, project, pcap);
    snprintf(repo, rcap, "%s/car_masks/%s", app_repo_data(a), (*car)->folder);
    return 1;
}

int app_repo_copy_unset(const App *a)
{
    if (app_repo_data(a)[0] != '\0') return 0;
    return a->kind == APP_KIND_CAR ? car_race_original_of(&a->car) != NULL : a->kind == APP_KIND_LIGHT && a->original_dir[0] != '\0';
}

int app_car_copy_can(const App *a)
{
    char project[APP_PATH], repo[APP_PATH], why[APP_MSG];
    const ExportCar *car;
    return folders(a, project, sizeof project, repo, sizeof repo, &car, why, sizeof why);
}

int app_car_copy_ask(App *a)
{
    char project[APP_PATH], repo[APP_PATH], why[APP_MSG] = "";
    const ExportCar *car;
    static MaskChanges changes;
    if (!folders(a, project, sizeof project, repo, sizeof repo, &car, why, sizeof why) || !mask_changes_compare(project, repo, &changes, why, sizeof why))
        return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    if (changes.changed + changes.added == 0) return snprintf(a->status, sizeof a->status, "copy to game repo: every mask is the game repo's own, nothing to copy"), 0;
    mask_changes_text(&changes, car->folder, a->publish_text, sizeof a->publish_text);
    a->publish_car = 1;
    a->publish_dialog = 1;
    return 1;
}

int app_car_copy(App *a)
{
    char project[APP_PATH], repo[APP_PATH], why[APP_MSG] = "";
    const ExportCar *car;
    static MaskChanges changes;
    int written = 0;
    a->publish_dialog = 0, a->publish_car = 0;
    if (!folders(a, project, sizeof project, repo, sizeof repo, &car, why, sizeof why) || !mask_changes_compare(project, repo, &changes, why, sizeof why) ||
        !mask_changes_publish(project, repo, &changes, &written, why, sizeof why))
        return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    snprintf(a->status, sizeof a->status, "copied to the game repo: %d mask%s written into %s", written, written == 1 ? "" : "s", repo);
    return 1;
}

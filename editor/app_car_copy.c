/* Copy to game repo for one of the game's own cars (app.h's app_car_copy_*): the car's masks folder (art-generated/car_masks of its kit) against the repo's data/car_masks/<folder>/, the changed and
 * new PNGs copied, and the card's Colors written into the car's row of TABLE in the repo's core/formats/car_paint.c (car_table_rewrite.h) when they are not what the row has; nothing is committed. */
#include "app.h"
#include "car_table_rewrite.h"
#include "formats/car_paint.h"
#include "car_race.h"
#include "mask_changes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const ExportCar *car;
    char project[APP_PATH], repo[APP_PATH];   /* the masks' folders; "" when the car has no masks folder yet */
    const char *stem;                         /* the colours' file: a drivable car's, NULL for the others (traffic, the police) and a masks project */
} Plan;

/* what the copy is made of; 0 with the reason in `why` (not one of the game's cars, no repo) */
static int plan_of(const App *a, Plan *p, char *why, size_t why_cap)
{
    memset(p, 0, sizeof *p);
    p->car = a->kind == APP_KIND_CAR ? car_race_original_of(&a->car) : NULL;
    if (p->car == NULL) return snprintf(why, why_cap, "only one of the game's own cars goes to the game repo"), 0;
    if (app_repo_data(a)[0] == '\0') return snprintf(why, why_cap, "the game repo's data/ is not set (File > Game repo folder...)"), 0;
    if (a->car.has_masks_dir) {
        car_project_masks_dir(&a->car, p->project, sizeof p->project);
        snprintf(p->repo, sizeof p->repo, "%s/car_masks/%s", app_repo_data(a), p->car->folder);
    }
    if (p->car->kind == EXPORT_DRIVABLE && !a->car.masks_only) p->stem = p->car->folder;   // the table's row name is the folder (TSupra, LDiabl, P911 ...)
    if (p->project[0] == '\0' && p->stem == NULL) return snprintf(why, why_cap, "the car has no masks folder yet"), 0;
    return 1;
}

int app_repo_copy_unset(const App *a)
{
    if (app_repo_data(a)[0] != '\0') return 0;
    return a->kind == APP_KIND_CAR ? car_race_original_of(&a->car) != NULL : a->kind == APP_KIND_LIGHT && a->original_dir[0] != '\0';
}

int app_car_copy_can(const App *a)
{
    Plan p;
    char why[APP_MSG];
    return plan_of(a, &p, why, sizeof why);
}

enum { COLORS_SAME = 0, COLORS_CHANGED = 1 };

/* the repo's car_paint.c: the parent of the data/ folder, then core/formats/car_paint.c */
static void table_path(const App *a, char *out, size_t cap)
{
    char root[APP_PATH];
    snprintf(root, sizeof root, "%s", app_repo_data(a));
    size_t n = strlen(root);
    while (n > 1 && root[n - 1] == '/') root[--n] = '\0';
    char *slash = strrchr(root, '/');
    if (slash != NULL) *slash = '\0';
    snprintf(out, cap, "%s/core/formats/car_paint.c", root);
}

static char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    char *text = NULL;
    size_t cap = 0, used = 0;
    for (;;) {
        if (used + 4096 + 1 > cap) {
            char *bigger = realloc(text, cap = cap * 2 + 8192);
            if (bigger == NULL) return free(text), fclose(f), NULL;
            text = bigger;
        }
        const size_t got = fread(text + used, 1, 4096, f);
        used += got;
        if (got == 0) break;
    }
    fclose(f);
    text[used] = '\0';
    *len = used;
    return text;
}

/* the card's colors against the repo's row of the table: COLORS_*, the rewritten file in *out (malloc'd) when changed, the row before and after in `before` and `after`; -1 with the reason */
static int colors_state(const App *a, const Plan *p, char **out, char *before, char *after, char *why, size_t why_cap)
{
    char path[APP_PATH + 40];
    size_t len = 0;
    *out = NULL;
    if (p->stem == NULL) return COLORS_SAME;
    if (car_edit_modified(&a->edit)) return snprintf(why, why_cap, "save the card first (Cmd+S): the colors copied are the saved ones"), -1;
    table_path(a, path, sizeof path);
    char *src = slurp(path, &len);
    if (src == NULL) {   // not a game repo's data/ (the program's own, a folder to try things in): fine while the colors are the ones the game has
        const ModPaint *card = a->edit.now.paints;
        int same = a->edit.now.paint_count == car_paint_count(p->car->model);
        for (int k = 0; same && k < a->edit.now.paint_count; k++) same = strcmp(card[k].name, car_paint(p->car->model, k)->name) == 0 && memcmp(card[k].rgb, car_paint(p->car->model, k)->rgb, 3) == 0;
        return same ? COLORS_SAME : (snprintf(why, why_cap, "the colors are not the game's and %s is not there: the game repo's data/ must sit in the repo (core/formats/car_paint.c is rewritten)", path), -1);
    }
    int changed = 0;
    *out = car_table_rewrite(src, len, p->stem, a->edit.now.paints, a->edit.now.paint_count, &changed, before, after, why, why_cap);
    free(src);
    if (*out == NULL) return -1;
    if (!changed) return free(*out), *out = NULL, COLORS_SAME;
    return COLORS_CHANGED;
}

int app_car_copy_ask(App *a)
{
    Plan p;
    char why[APP_MSG] = "", before[CAR_TABLE_LIST], after[CAR_TABLE_LIST], *table = NULL;
    static MaskChanges changes;
    if (!plan_of(a, &p, why, sizeof why)) return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    changes.count = changes.changed = changes.added = changes.unchanged = changes.missing = 0;
    if (p.project[0] != '\0' && !mask_changes_compare(p.project, p.repo, &changes, why, sizeof why)) return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    const int state = colors_state(a, &p, &table, before, after, why, sizeof why);
    free(table);
    if (state < 0) return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    if (changes.changed + changes.added == 0 && state == COLORS_SAME) return snprintf(a->status, sizeof a->status, "copy to game repo: every mask%s is the game repo's own, nothing to copy", p.stem != NULL ? " and color" : ""), 0;
    a->publish_text[0] = '\0';
    if (changes.changed + changes.added > 0) mask_changes_text(&changes, p.car->folder, a->publish_text, sizeof a->publish_text);
    if (state == COLORS_CHANGED) {
        const size_t used = strlen(a->publish_text);
        snprintf(a->publish_text + used, sizeof a->publish_text - used, "%score/formats/car_paint.c, the row of %s:\n  was: %s\n  now: %s\n", used > 0 ? "\n" : "", p.stem, before, after);
    }
    a->publish_car = 1;
    a->publish_dialog = 1;
    return 1;
}

int app_car_copy(App *a)
{
    Plan p;
    char why[APP_MSG] = "";
    static MaskChanges changes;
    char *table = NULL, path[APP_PATH + 40] = "", before[CAR_TABLE_LIST], after[CAR_TABLE_LIST];
    int written = 0, colours = 0;
    a->publish_dialog = 0, a->publish_car = 0;
    if (!plan_of(a, &p, why, sizeof why)) return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    changes.count = changes.changed = changes.added = changes.unchanged = changes.missing = 0;
    if (p.project[0] != '\0' && (!mask_changes_compare(p.project, p.repo, &changes, why, sizeof why) || !mask_changes_publish(p.project, p.repo, &changes, &written, why, sizeof why)))
        return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    const int state = colors_state(a, &p, &table, before, after, why, sizeof why);
    if (state < 0) return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    if (state == COLORS_CHANGED) {
        char scratch[APP_PATH + 48];
        table_path(a, path, sizeof path);
        snprintf(scratch, sizeof scratch, "%s.new", path);
        FILE *f = fopen(scratch, "wb");
        const int ok = f != NULL && fputs(table, f) >= 0;
        if (f != NULL && fclose(f) != 0) { remove(scratch); free(table); return snprintf(a->status, sizeof a->status, "copy to game repo: cannot write %s", path), 0; }
        if (!ok || rename(scratch, path) != 0) { remove(scratch); free(table); return snprintf(a->status, sizeof a->status, "copy to game repo: cannot write %s", path), 0; }
        colours = 1;
    }
    free(table);
    char where[APP_PATH + 64] = "";
    if (colours) snprintf(where, sizeof where, ", and the colors into %s", path);
    if (written > 0 || p.project[0] != '\0') snprintf(a->status, sizeof a->status, "copied to the game repo: %d mask%s written into %s%s", written, written == 1 ? "" : "s", p.repo, where);
    else snprintf(a->status, sizeof a->status, "copied to the game repo: the colors written into %s", path);
    return 1;
}

#include "app_dash.h"

#include "mask_changes.h"
#include "psd_manifest.h"
#include "psd_stale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { STALE_EVERY = 150 };   /* frames between looks at the files' times (about 2.5 s: on a slow disk a look is dear, and the first one is at the first frame) */

static void say(App *a, const char *text)
{
    snprintf(a->status, sizeof a->status, "%s", text);
}

static const ExportCar *drivable(const App *a)
{
    const ExportCar *car = a->kind == APP_KIND_CAR && !a->car.masks_only ? car_race_original_of(&a->car) : NULL;
    return car != NULL && car->kind == EXPORT_DRIVABLE ? car : NULL;
}

/* A kit made by an older Studio (the migration copied the game's masks in with no manifest) has PNGs the manifest has no line for: "unknown", which Convert PSD keeps when they are not what a PSD
 * makes. One that is the game's own picture (the export's copy, nothing edited) is Studio's to replace: its hash is written down, so a PSD painted after it is converted. Done once for each project
 * folder opened (a PNG that differs from the game's is somebody's edit and stays unknown). */
static void adopt_the_games(App *a, const char *masks, const char *dash)
{
    static char done[APP_PATH];
    const ExportCar *oc = car_race_original_of(&a->car);
    if (oc == NULL || a->export_src.data[0] == '\0' || strcmp(done, a->dir) == 0) return;
    snprintf(done, sizeof done, "%s", a->dir);
    char manifest[APP_PATH], game[APP_PATH];
    car_project_manifest(&a->car, manifest, sizeof manifest);
    Manifest *m = calloc(1, sizeof *m);
    MaskChanges *c = calloc(1, sizeof *c);
    if (m == NULL || c == NULL || !manifest_load(manifest, m)) { free(m), free(c); return; }
    int adopted = 0;
    for (int pass = 0; pass < 2; ++pass) {
        snprintf(game, sizeof game, "%s/%s/%s", a->export_src.data, pass == 0 ? "car_masks" : "dash_masks", oc->folder);
        char why[160];
        if (!(pass == 0 ? mask_changes_compare(masks, game, c, why, sizeof why) : mask_changes_compare_dash(dash, game, c, why, sizeof why))) continue;
        for (int i = 0; i < c->count; ++i)
            if (c->item[i].kind == MC_UNCHANGED && manifest_state(m, pass == 0 ? masks : dash, c->item[i].file) == GEN_UNKNOWN)
                adopted += manifest_record(m, pass == 0 ? masks : dash, c->item[i].file);
    }
    if (adopted > 0) manifest_save(manifest, m);
    free(m), free(c);
}

void app_dash_stale_refresh(App *a)
{
    char dir[APP_PATH];
    StaleReport r;
    a->dash_stale[0] = '\0';
    a->dash_stale_ticks = 1;   /* 0 is "never looked" */
    a->stale_ok = 0;
    if (a->kind != APP_KIND_CAR || (!app_car_is_kit(a) && car_race_original_of(&a->car) == NULL) || !app_psd_psd_dir(a, dir, sizeof dir)) return;
    if (app_car_is_kit(a)) {   // a kit: its PSDs in art-source/masks, the PNGs flat in art-generated
        char masks[APP_PATH], dash[APP_PATH], manifest[APP_PATH];
        app_psd_dirs(a, masks, dash, manifest, sizeof masks);
        if (!psd_stale_check_flat(dir, masks, dash, drivable(a) != NULL, &r)) return;
        adopt_the_games(a, masks, dash);
        Manifest *known = calloc(1, sizeof *known);   // which PNGs were touched since Studio made them
        if (known != NULL && manifest_load_cached(manifest, known))
            for (int i = 0; i < r.count; ++i) {
                const GenState state = manifest_state(known, r.item[i].dash ? dash : masks, r.item[i].png);
                r.item[i].edited = state == GEN_EDITED;
                r.item[i].kept = state == GEN_UNKNOWN && r.item[i].state == STALE_NEWER;
            }
        free(known);
    } else if (!psd_stale_check(dir, a->car.piece, a->car.folder, drivable(a) != NULL, &r)) return;
    psd_stale_text(&r, a->dash_stale, sizeof a->dash_stale);
    a->stale = r;
    a->stale_ok = 1;
    app_psd_auto(a, &r);   // a PSD saved since its mask was made: the masks are made again
}

int app_dash_possible(const App *a, char *why, size_t why_cap)
{
    char dir[APP_PATH];
    if (drivable(a) == NULL) return snprintf(why, why_cap, "Check dash is for the cars you drive: open one exported with Export original car"), 0;
    if (!app_psd_psd_dir(a, dir, sizeof dir)) return snprintf(why, why_cap, "No folder named %s in the PSD folder: the dash comes from its dash.psd", a->car.folder), 0;
    if (app_psd_busy(a) || car_race_busy(&a->race) || a->sheet_active) return snprintf(why, why_cap, "Wait for the run in progress"), 0;
    return 1;
}

int app_dash_check_start(App *a)
{
    char why[APP_MSG];
    if (!app_dash_possible(a, why, sizeof why)) return say(a, why), 0;
    if (!app_psd_start(a)) return 0;   // it says why
    a->dash_check = 1;
    say(a, "Check dash: converting the PSDs, then the cockpit photo at night");
    return 1;
}

/* the conversion ended well: the cockpit photo at night with the car's own gauge colours, the Race window's own choices put back at once (the job keeps its own copy) */
static void take_photo(App *a)
{
    const RaceLook look = a->race_look;
    const int night = a->race_night;
    a->race_look.cockpit = 1, a->race_look.gauge_set = 1, a->race_look.gauge_light = 5;
    a->race_night = 1;
    app_race(a, 1);   // the status says how it went
    a->race_look = look;
    a->race_night = night;
}

int app_dash_poll(App *a)
{
    if (a->kind == APP_KIND_CAR && (a->dash_stale_ticks == 0 || ++a->dash_stale_ticks >= STALE_EVERY)) app_dash_stale_refresh(a);   // at once at the first frame, then now and then
    if (a->dash_check != 1 || app_psd_busy(a)) return a->dash_check == 1;
    a->dash_check = 0;
    if (a->psd.ok) {
        take_photo(a);
        app_dash_stale_refresh(a);
    }   // else app_psd_poll's status says what failed
    return 0;
}

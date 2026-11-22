#include "app_dash.h"

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
        Manifest *known = calloc(1, sizeof *known);   // which PNGs were touched since Studio made them
        if (known != NULL && manifest_load_cached(manifest, known))
            for (int i = 0; i < r.count; ++i) r.item[i].edited = manifest_state(known, r.item[i].dash ? dash : masks, r.item[i].png) == GEN_EDITED;
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

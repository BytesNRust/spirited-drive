/* Convert PSD in the app (app.h's app_psd_*): the glue between a car project and psd_job.h. */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int join(char *out, size_t cap, const char *a, const char *b)
{
    const int n = snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
}

static int has_scripts(const char *tools)
{
    char p[APP_PATH];
    return join(p, sizeof p, tools, "car_mask_from_psd.py") && access(p, R_OK) == 0;
}

void app_psd_setup(App *a, const char *base, const char *root)
{
    a->tools[0] = a->psd_root[0] = '\0';
    const char *env = getenv("TNFS_TOOLS");
    char cand[APP_PATH];
    if (env != NULL && has_scripts(env)) snprintf(a->tools, sizeof a->tools, "%s", env);
    static const char *const ABOVE[] = { "tools", "../tools", "../../tools", "../../../tools" };   /* beside the program, or a build folder inside the source tree */
    for (size_t i = 0; base != NULL && a->tools[0] == '\0' && i < sizeof ABOVE / sizeof ABOVE[0]; i++) {
        char dir[APP_PATH];
        if (join(dir, sizeof dir, base, ABOVE[i]) && has_scripts(dir) && realpath(dir, cand) != NULL) snprintf(a->tools, sizeof a->tools, "%s", cand);
    }
    const char *home = getenv("HOME"), *chosen = root != NULL ? root : getenv("TNFS_PSD_ROOT");
    if (chosen != NULL && chosen[0] != '\0') snprintf(a->psd_root, sizeof a->psd_root, "%s", chosen);
    else if (home != NULL && home[0] != '\0') join(a->psd_root, sizeof a->psd_root, home, "workspace/bytes-n-rust/art/tnfs-masks");
}

int app_psd_psd_dir(const App *a, char *out, size_t cap)
{
    return a->kind == APP_KIND_CAR && a->car.folder[0] != '\0' && a->psd_root[0] != '\0' && psd_job_find(a->psd_root, a->car.folder, out, cap);
}

int app_psd_busy(const App *a)
{
    return psd_job_busy(&a->psd);
}

int app_psd_start(App *a)
{
    char dir[APP_PATH], why[APP_MSG];
    if (a->kind != APP_KIND_CAR) return snprintf(a->status, sizeof a->status, "open a car first"), 0;
    if (psd_job_busy(&a->psd)) return snprintf(a->status, sizeof a->status, "Convert PSD is already running"), 0;
    if (app_mask_modified(a)) return snprintf(a->status, sizeof a->status, "the open mask has changes that are not saved: Save or Discard them first, or Convert PSD would replace them"), 0;
    if (a->tools[0] == '\0') return snprintf(a->status, sizeof a->status, "Convert PSD needs the repository's tools folder (car_mask_from_psd.py): not found beside this program (TNFS_TOOLS)"), 0;
    if (!app_psd_psd_dir(a, dir, sizeof dir))
        return snprintf(a->status, sizeof a->status, "no PSD folder named %s in %s", a->car.folder, a->psd_root[0] != '\0' ? a->psd_root : "(no PSD folder set: --psd-root)"), 0;
    if (!psd_job_start(&a->psd, a->tools, dir, a->car.folder, a->car.piece, why, sizeof why)) return snprintf(a->status, sizeof a->status, "Convert PSD did not start: %s", why), 0;
    snprintf(a->status, sizeof a->status, "Converting the PSDs of %s: the masks are replaced when every script has ended well", a->car.folder);
    return 1;
}

int app_psd_poll(App *a)
{
    if (!psd_job_busy(&a->psd)) return 0;
    if (psd_job_poll(&a->psd)) return 1;
    snprintf(a->status, sizeof a->status, "%s", a->psd.text);
    if (a->psd.ok) {
        app_car_reread(a);   /* the lists, and the masks read again (a mask that was open is closed: it was just remade) */
    }
    return 0;
}

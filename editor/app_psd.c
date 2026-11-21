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

void app_psd_stop(App *a)
{
    psd_job_stop(&a->psd);
}

void app_psd_setup(App *a, const char *base, const char *root)
{
    a->tools[0] = a->psd_root[0] = '\0';
    const char *env = getenv("TNFS_TOOLS");
    char cand[APP_PATH];
    if (env != NULL && has_scripts(env)) snprintf(a->tools, sizeof a->tools, "%s", env);
    static const char *const ABOVE[] = { "tools", "../Resources/tools", "../tools", "../../tools", "../../../tools" };   /* beside the program, or a build folder inside the source tree */
    for (size_t i = 0; base != NULL && a->tools[0] == '\0' && i < sizeof ABOVE / sizeof ABOVE[0]; i++) {
        char dir[APP_PATH];
        if (join(dir, sizeof dir, base, ABOVE[i]) && has_scripts(dir) && realpath(dir, cand) != NULL) snprintf(a->tools, sizeof a->tools, "%s", cand);
    }
    py_probe_free(&a->py);
    py_probe_init(&a->py, getenv("TNFS_PYTHON"));   /* looked for at the first poll with a car open */
    const char *home = getenv("HOME"), *chosen = root != NULL ? root : getenv("TNFS_PSD_ROOT");
    if (chosen != NULL && chosen[0] != '\0') snprintf(a->psd_root, sizeof a->psd_root, "%s", chosen);
    else if (home != NULL && home[0] != '\0') join(a->psd_root, sizeof a->psd_root, home, "workspace/bytes-n-rust/art/tnfs-masks");
}

int app_psd_psd_dir(const App *a, char *out, size_t cap)
{
    return a->kind == APP_KIND_CAR && a->car.folder[0] != '\0' && a->psd_root[0] != '\0' && psd_job_find(a->psd_root, a->car.folder, out, cap);
}

int app_psd_python_ready(const App *a)
{
    return a->py.state == 2 && a->py.found;
}

void app_psd_python_note(const App *a, char *out, size_t cap)
{
    if (a->py.state != 2) snprintf(out, cap, "Looking for a python with psd-tools and Pillow...");
    else if (a->py.found) snprintf(out, cap, "Runs the scripts with %s", a->py.python[0] != '\0' ? a->py.python : "python3 of PATH");
    else snprintf(out, cap, "No python with psd-tools and Pillow (tried: %s).\nInstall them: pip3 install psd-tools Pillow, or set TNFS_PYTHON to the full path of the python that has them", a->py.tried[0] != '\0' ? a->py.tried : "none");
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
    if (a->tools[0] == '\0') return snprintf(a->status, sizeof a->status, "Convert PSD needs the repository's tools folder (car_mask_from_psd.py): not found beside this program (TNFS_TOOLS)"), 0;
    if (!app_psd_psd_dir(a, dir, sizeof dir))
        return snprintf(a->status, sizeof a->status, "no PSD folder named %s in %s", a->car.folder, a->psd_root[0] != '\0' ? a->psd_root : "(no PSD folder set: --psd-root)"), 0;
    if (a->py.state != 2) return snprintf(a->status, sizeof a->status, "Convert PSD is still looking for a python with psd-tools: try again in a moment"), 0;
    if (!a->py.found) {
        app_psd_python_note(a, why, sizeof why);
        return snprintf(a->status, sizeof a->status, "Convert PSD: %s", why), 0;
    }
    if (!psd_job_start(&a->psd, a->py.python, a->tools, dir, a->car.folder, a->car.piece, why, sizeof why)) return snprintf(a->status, sizeof a->status, "Convert PSD did not start: %s", why), 0;
    snprintf(a->status, sizeof a->status, "Converting the PSDs of %s: the masks are replaced when every script has ended well", a->car.folder);
    return 1;
}

int app_psd_poll(App *a)
{
    if (a->kind == APP_KIND_CAR && a->py.state != 2) py_probe_poll(&a->py);   /* once per session, in the background */
    if (!psd_job_busy(&a->psd)) return 0;
    if (psd_job_poll(&a->psd)) return 1;
    snprintf(a->status, sizeof a->status, "%s", a->psd.text);
    if (a->psd.ok) {
        app_car_reread(a);   /* the lists, and the masks read again (a mask that was open is closed: it was just remade) */
    }
    return 0;
}

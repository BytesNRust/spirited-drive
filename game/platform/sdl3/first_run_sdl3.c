/* port: see platform/first_run.h. */
#include "game/platform/first_run.h"

#include "game/platform/platform.h"
#include "game/sys/data_check.h"
#include "game/sys/data_copy.h"
#include "game/sys/game_name.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

bool first_run_user_copy(char *out, size_t out_size)
{
    char dir[1024];
    if (!platform_user_dir(dir, sizeof dir)) return false;   /* --user DIR, $TNFS_PREF_DIR or the OS's: the same one as the settings and the log */
    snprintf(out, out_size, "%sgamedata", dir);
    return true;
}

/* a message box with up to three buttons: the index of the one pressed (the last when it is closed), left to right in the order given */
static int ask(const char *title, const char *text, const char *const labels[], int count)
{
    SDL_MessageBoxButtonData buttons[3];
    for (int i = 0; i < count; i++) {
        buttons[i] = (SDL_MessageBoxButtonData){ i == 0 ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT : (i == count - 1 ? SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT : 0), i, labels[i] };
    }
    const SDL_MessageBoxData box = { SDL_MESSAGEBOX_INFORMATION, NULL, title, text, count, buttons, NULL };
    int pressed = count - 1;
    if (!SDL_ShowMessageBox(&box, &pressed)) return count - 1;
    return pressed;
}

/* port (profiles): the three profiles as buttons, Restomod '26 first (Return and Escape both take it), with what each is; the choice is kept in port.cfg */
static void choose_profile(void)
{
    static const Profile ORDER[3] = { PROFILE_RESTOMOD, PROFILE_RESTORED, PROFILE_STOCK };
    char text[1500];
    int n = snprintf(text, sizeof text, "Choose how the game looks and plays. You can change it any time (Options).\n\n");
    for (int i = 0; i < 3 && n > 0 && (size_t)n < sizeof text; i++)
        n += snprintf(text + n, sizeof text - (size_t)n, "%s\n%s\n%s\n\n", port_profile_title(ORDER[i]), port_profile_line1(ORDER[i]), port_profile_line2(ORDER[i]));
    SDL_MessageBoxButtonData buttons[3];
    for (int i = 0; i < 3; i++)
        buttons[i] = (SDL_MessageBoxButtonData){ i == 0 ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT : 0, i, port_profile_title(ORDER[i]) };
    const SDL_MessageBoxData box = { SDL_MESSAGEBOX_INFORMATION, NULL, "Pick your profile", text, 3, buttons, NULL };
    int pressed = 0;
    if (!SDL_ShowMessageBox(&box, &pressed) || pressed < 0 || pressed > 2) pressed = 0;
    if (!port_profile_select(ORDER[pressed])) port_log("first_run: could not keep the profile %s\n", port_profile_key(ORDER[pressed]));
}

typedef struct {
    int done;
    char path[1024];
} Picked;

static void picked_folder(void *user, const char *const *list, int filter)
{
    (void)filter;
    Picked *p = user;
    p->path[0] = '\0';
    if (list != NULL && list[0] != NULL) snprintf(p->path, sizeof p->path, "%s", list[0]);
    p->done = 1;
}

/* the folder the player picks ("" when none), the dialog's result waited for */
static bool pick_folder(char *out, size_t out_size)
{
    Picked p = { 0, { 0 } };
    SDL_ShowOpenFolderDialog(picked_folder, &p, NULL, NULL, false);
    while (!p.done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        SDL_Delay(30);
    }
    snprintf(out, out_size, "%s", p.path);
    return p.path[0] != '\0';
}

/* port (ISO): the .iso file the player picks ("" when none), one filter, no multiple selection */
static bool pick_iso(char *out, size_t out_size)
{
    static const SDL_DialogFileFilter FILTERS[] = { { "CD image", "iso" } };
    Picked p = { 0, { 0 } };
    SDL_ShowOpenFileDialog(picked_folder, &p, NULL, FILTERS, 1, NULL, false);
    while (!p.done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        SDL_Delay(30);
    }
    snprintf(out, out_size, "%s", p.path);
    return p.path[0] != '\0';
}

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int64_t total;
} Bar;

static int draw_bar(void *user, int64_t bytes)
{
    Bar *b = user;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) return 0;
    }
    const float done = b->total > 0 ? (float)bytes / (float)b->total : 0.0f;
    SDL_SetRenderDrawColor(b->renderer, 24, 24, 24, 255);
    SDL_RenderClear(b->renderer);
    const SDL_FRect frame = { 20, 40, 440, 24 }, fill = { 22, 42, 436.0f * (done > 1.0f ? 1.0f : done), 20 };
    SDL_SetRenderDrawColor(b->renderer, 120, 120, 120, 255);
    SDL_RenderRect(b->renderer, &frame);
    SDL_SetRenderDrawColor(b->renderer, 220, 60, 40, 255);
    SDL_RenderFillRect(b->renderer, &fill);
    SDL_RenderPresent(b->renderer);
    return 1;
}

typedef enum { WIZARD_FOLDER, WIZARD_ISO } WizardSource;

/* the copy of a folder, or of the folder `root` inside an image, with the bar */
static bool copy_with_bar(WizardSource kind, const char *src, const char *root, const char *dest)
{
    Bar bar = { NULL, NULL, kind == WIZARD_ISO ? data_copy_iso_size(src, root) : data_copy_size(src) };
    const bool window = SDL_CreateWindowAndRenderer(GAME_NAME ": copying the game's files", 480, 104, 0, &bar.window, &bar.renderer);
    if (!window) port_log("first run: no window for the progress bar (%s): copying without\n", SDL_GetError());
    DataCopyProgress progress = window ? draw_bar : NULL;
    const bool ok = (kind == WIZARD_ISO ? data_copy_iso(src, root, dest, progress, &bar) : data_copy_tree(src, dest, progress, &bar)) != 0;
    if (window) SDL_DestroyRenderer(bar.renderer), SDL_DestroyWindow(bar.window);
    return ok;
}

/* A file chosen as an ISO image that cannot be read as one: asks. 1 it reads, 0 choose again, -1 quit. */
static int iso_readable_or_ask(const char *chosen)
{
    char why[200] = { 0 };
    if (data_iso_readable(chosen, why, sizeof why)) return 1;
    static const char *const AGAIN[] = { "Choose again", "Quit" };
    char text[1500];
    snprintf(text, sizeof text, "%s\n\nIt could not be read as an ISO image of a CD (%s). Choose the .iso of The Need for Speed SE (PC), or the CD or a folder.", chosen, why);
    port_log("first run: %s: %s\n", chosen, why);
    return ask("This is not a CD image", text, AGAIN, 2) == 0 ? 0 : -1;
}

/* A choice that is not the expected game's files: asks what to do with it; the answer (0 choose another, 1 use it anyway when it can be, else quit). */
static int ask_unexpected(const char *chosen, const DataCheck *c, WizardSource kind, const char *what)
{
    char text[640];
    snprintf(text, sizeof text, "%s\n\n%d of the %d key files are as in The Need for Speed SE (PC); %d are missing and %d are different.\n\n"
                                "It may be another edition, a modified copy, or the wrong %s.",
             chosen, c->matched, c->total, c->missing, c->different, what);
    static const char *const OTHER_FOLDER[] = { "Choose another folder", "Use it anyway", "Quit" };
    static const char *const OTHER_IMAGE[] = { "Choose another image", "Use it anyway", "Quit" };
    static const char *const OTHER_NONE_FOLDER[] = { "Choose another folder", "Quit" };
    static const char *const OTHER_NONE_IMAGE[] = { "Choose another image", "Quit" };
    const char *const title = kind == WIZARD_ISO ? "This is not the expected image" : "This is not the expected folder";
    const int found = c->found;   /* the game's own files are there, only some differ: using it is possible */
    return found ? ask(title, text, kind == WIZARD_ISO ? OTHER_IMAGE : OTHER_FOLDER, 3)
                 : ask(title, text, kind == WIZARD_ISO ? OTHER_NONE_IMAGE : OTHER_NONE_FOLDER, 2);
}

/* The chosen files copied into the game's folder, with the profile asked once; false (and told) when the copy fails. */
static bool install_chosen(WizardSource kind, const char *chosen, const char *dest)
{
    DataCheck c;
    (void)(kind == WIZARD_ISO ? data_check_iso(chosen, &c) : data_check(chosen, &c));   /* the folder that holds the files (one level down, perhaps) */
    const char *const root = kind == WIZARD_ISO ? c.root : (c.root[0] != '\0' ? c.root : chosen);
    const bool ok = copy_with_bar(kind, kind == WIZARD_ISO ? chosen : root, kind == WIZARD_ISO ? c.root : "", dest);
    if (ok && c.matched != c.total) (void)data_mark_accepted(dest, &c);   /* Use it anyway: the next start must not ask again */
    if (ok && !port_settings_profile_chosen()) choose_profile();   /* a file that already names a profile (a reinstall) keeps it */
    if (!ok) {
        static const char *const OK[] = { "OK" };
        char text[1500];
        snprintf(text, sizeof text, "The game's files could not be copied: %s. Nothing was kept; the next start asks again.", data_copy_error());
        ask("The copy failed", text, OK, 1);
    }
    return ok;
}

bool first_run_setup(const char *dest)
{
    const char *const display = SDL_getenv("SDL_VIDEO_DRIVER");
    if (display != NULL && strcmp(display, "dummy") == 0) return false;   /* a headless run has nobody to ask */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        port_fatal("first run: no display: %s\n", SDL_GetError());
        return false;
    }
    static const char *const START[] = { "Choose the CD or a folder", "Choose an ISO image", "Quit" };
    const int start = ask(GAME_NAME, "This game needs the files of your own copy of The Need for Speed Special Edition (PC, 1996).\n\n"
                                           "Choose where they are: the CD, a copy of it on your disk, or an ISO image of it. They are copied once into this game's own folder, "
                                           "which takes a minute.", START, 3);
    if (start != 0 && start != 1) {
        SDL_Quit();
        return false;
    }
    const WizardSource kind = start == 1 ? WIZARD_ISO : WIZARD_FOLDER;
    const char *const what = kind == WIZARD_ISO ? "image" : "folder";
    char chosen[1024] = { 0 };
    bool done = false;
    while (!done) {
        if (!(kind == WIZARD_ISO ? pick_iso(chosen, sizeof chosen) : pick_folder(chosen, sizeof chosen))) break;
        if (kind == WIZARD_ISO) {   /* a file that is not an ISO image: renamed, cut short, empty */
            const int readable = iso_readable_or_ask(chosen);
            if (readable == 0) continue;
            if (readable < 0) break;
        }
        DataCheck c;
        if (kind == WIZARD_ISO ? data_check_iso(chosen, &c) : data_check(chosen, &c)) {
            done = true;
            break;
        }
        const int found = c.found;   /* the game's own files are there, only some differ: using it is possible */
        const int answer = ask_unexpected(chosen, &c, kind, what);
        if (answer == 1 && found) {
            done = true;
            break;
        }
        if (answer != 0) break;
    }
    bool ok = false;
    if (done) ok = install_chosen(kind, chosen, dest);
    SDL_Quit();
    return ok;
}

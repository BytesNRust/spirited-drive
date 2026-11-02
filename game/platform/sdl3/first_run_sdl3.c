/* port: see platform/first_run.h. */
#include "game/platform/first_run.h"

#include "game/platform/platform.h"
#include "game/sys/data_check.h"
#include "game/sys/data_copy.h"
#include "game/sys/port_log.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

bool first_run_user_copy(char *out, size_t out_size)
{
    char dir[1024];
    if (!platform_user_dir(dir, sizeof dir)) return false;   /* the --mods folder's user/, $TNFS_PREF_DIR or the OS's: the same one as the settings and the log */
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

static bool copy_with_bar(const char *src, const char *dest)
{
    Bar bar = { NULL, NULL, data_copy_size(src) };
    if (!SDL_CreateWindowAndRenderer("TNFS Restomod: copying the game's files", 480, 104, 0, &bar.window, &bar.renderer)) {
        port_log("first run: no window for the progress bar (%s): copying without\n", SDL_GetError());
        return data_copy_tree(src, dest, NULL, NULL) != 0;
    }
    const bool ok = data_copy_tree(src, dest, draw_bar, &bar) != 0;
    SDL_DestroyRenderer(bar.renderer);
    SDL_DestroyWindow(bar.window);
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
    static const char *const START[] = { "Choose the folder", "Quit" };
    if (ask("TNFS Restomod", "This game needs the files of your own copy of The Need for Speed Special Edition (PC, 1996).\n\n"
                              "Choose the folder that has them: the CD, or a copy of it on your disk. They are copied once into this game's own folder, "
                              "which takes a minute.", START, 2) != 0) {
        SDL_Quit();
        return false;
    }
    char chosen[1024] = { 0 };
    bool done = false;
    while (!done) {
        if (!pick_folder(chosen, sizeof chosen)) break;
        DataCheck c;
        if (data_check(chosen, &c)) {
            done = true;
            break;
        }
        char text[640];
        snprintf(text, sizeof text, "%s\n\n%d of the %d key files are as in The Need for Speed SE (PC); %d are missing and %d are different.\n\n"
                                    "It may be another edition, a modified copy, or the wrong folder.",
                 chosen, c.matched, c.total, c.missing, c.different);
        static const char *const OTHER[] = { "Choose another folder", "Use it anyway", "Quit" };
        static const char *const OTHER_NONE[] = { "Choose another folder", "Quit" };
        const int found = c.root[0] != '\0';   /* the game's own files are there, only some differ: using it is possible */
        const int answer = found ? ask("This is not the expected folder", text, OTHER, 3) : ask("This is not the expected folder", text, OTHER_NONE, 2);
        if (answer == 1 && found) {
            done = true;
            break;
        }
        if (answer != 0) break;
    }
    bool ok = false;
    if (done) {
        DataCheck c;
        (void)data_check(chosen, &c);   /* the folder that holds the files (one level down, perhaps) */
        const char *const root = c.root[0] != '\0' ? c.root : chosen;
        ok = copy_with_bar(root, dest);
        if (ok && c.matched != c.total) (void)data_mark_accepted(dest, &c);   /* Use it anyway: the next start must not ask again */
        if (!ok) {
            static const char *const OK[] = { "OK" };
            {
                char text[1500];
                snprintf(text, sizeof text, "The game's files could not be copied: %s. Nothing was kept; the next start asks again.", data_copy_error());
                ask("The copy failed", text, OK, 1);
            }
        }
    }
    SDL_Quit();
    return ok;
}

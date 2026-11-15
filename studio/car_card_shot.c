/* port (dep59 step 3): a car piece's pictures taken from the game's renderer (studio/car_card.h, `car card` without --generic). The
 * game races the piece alone (`--drive --racers 1 --node --screenshot`) at render scale 1 (640 x 480), from the follow camera's view
 * kind 3 (TNFS_VIEW), behind the car and a little above: the car sits at the same place on screen on every track and node, between
 * the rev counter (to x 160) and the map (from x 518), at x 320 and, with the road's slope, y 295 to 410 (TR1 node 200) or 299 to 399
 * (AL1 node 120). The pictures are crops of that frame centred on x 320, y 352; the track, the node and the time of day choose what
 * is behind it. The game's own output goes to /dev/null; its errors are the user's. */
#include "studio/car_card.h"

#include "formats/mod_ini.h"
#include "formats/png.h"
#include "studio/fsio.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

enum { FRAME_W = 640, FRAME_H = 480, CAR_X = 320, CAR_Y = 352, RUN_SECONDS = 120, VIEW_KIND = 3 };

typedef struct { const char *file; int w, h; } Picture;
static const Picture PICTURES[] = { { "card.png", CAR_CARD_W, CAR_CARD_H }, { "rival.png", 284, 117 }, { "gallery.png", 320, 200 } };
enum { PICTURE_COUNT = sizeof PICTURES / sizeof PICTURES[0] };

/* the game's user folder: Restomod, no extra liveries, render scale 1, the time of day */
static int user_folder(const char *dir, const char *time_name, ExchangeError *err)
{
    char path[PATH_MAX], cfg[160];
    snprintf(path, sizeof path, "%s/port.cfg", dir);
    if (!fsio_write_file(path, (const uint8_t *)"profile=restomod\n", 17, 1, err)) return 0;
    const int n = snprintf(cfg, sizeof cfg, "extra_liveries=0\nrender_scale=1\nlight_time=%s\n", time_name);
    snprintf(path, sizeof path, "%s/restomod.cfg", dir);
    return fsio_write_file(path, (const uint8_t *)cfg, (size_t)n, 1, err);
}

/* the piece copied below `dir` (the game reads the copy, never the author's folder) */
static int copy_piece(const char *piece, const char *dir, ExchangeError *err)
{
    FileSet set;
    fileset_init(&set);
    const int ok = fsio_read_tree(piece, &set, err) && fsio_write_tree(dir, &set, 1, err);
    fileset_free(&set);
    return ok;
}

/* the environment of the game's run: the caller's, then the dummy drivers and the view kind */
static char **run_environment(void)
{
    static char *extra[] = { "SDL_VIDEO_DRIVER=dummy", "SDL_RENDER_DRIVER=software", "SDL_AUDIO_DRIVER=dummy", "SDL_AUDIODRIVER=dummy",
                             "TNFS_VIEW=3" };
    size_t n = 0;
    while (environ[n] != NULL) n++;
    char **env = calloc(n + sizeof extra / sizeof extra[0] + 1, sizeof *env);
    if (env == NULL) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (strncmp(environ[i], "TNFS_", 5) != 0 && strncmp(environ[i], "SDL_", 4) != 0) env[k++] = environ[i];
    for (size_t i = 0; i < sizeof extra / sizeof extra[0]; i++) env[k++] = extra[i];
    return env;
}

/* the game's exit status, or -1 when it ran past RUN_SECONDS (then killed) or could not start */
static int run_game(char *const argv[], ExchangeError *err)
{
    char **env = run_environment();
    pid_t pid;
    if (env == NULL) { exchange_fail(err, "out of memory"); return -1; }
    posix_spawn_file_actions_t files;
    posix_spawn_file_actions_init(&files);
    posix_spawn_file_actions_addopen(&files, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    const int spawned = posix_spawn(&pid, argv[0], &files, NULL, argv, env);
    posix_spawn_file_actions_destroy(&files);
    free(env);
    if (spawned != 0) { exchange_fail(err, "cannot run %s: %s", argv[0], strerror(spawned)); return -1; }
    int status = 0;
    for (int waited = 0; waited < RUN_SECONDS * 10; waited++) {
        const pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        if (r < 0 && errno != EINTR) { exchange_fail(err, "lost the game's run: %s", strerror(errno)); return -1; }
        nanosleep(&(struct timespec){ 0, 100000000 }, NULL);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    exchange_fail(err, "the game ran past %d seconds", RUN_SECONDS);
    return -1;
}

/* the screenshot's RGB pixels (malloc'd), FRAME_W x FRAME_H */
static uint8_t *read_frame(const char *path, ExchangeError *err)
{
    size_t size = 0;
    uint8_t *ppm = fsio_read_file(path, &size, err);
    if (ppm == NULL) return NULL;
    int w = 0, h = 0, max = 0, used = 0;
    const int parsed = size > 16 && sscanf((const char *)ppm, "P6 %d %d %d%n", &w, &h, &max, &used) == 3;
    const size_t at = (size_t)used + 1;
    if (!parsed || w != FRAME_W || h != FRAME_H || max != 255 || size < at + (size_t)w * h * 3) {
        exchange_fail(err, "the game's screenshot is not a %dx%d picture (got %dx%d)", FRAME_W, FRAME_H, w, h);
        free(ppm);
        return NULL;
    }
    memmove(ppm, ppm + at, (size_t)w * h * 3);
    return ppm;
}

/* the crop of `p`'s size centred on the car, as PNG bytes (malloc'd) */
static int crop(const uint8_t *rgb, const Picture *p, uint8_t **png, size_t *len, ExchangeError *err)
{
    uint8_t *rgba = malloc((size_t)p->w * p->h * 4);
    if (rgba == NULL) { exchange_fail(err, "out of memory"); return 0; }
    const int x0 = CAR_X - p->w / 2, y0 = CAR_Y - p->h / 2;
    for (int y = 0; y < p->h; y++)
        for (int x = 0; x < p->w; x++) {
            const uint8_t *s = rgb + 3 * ((size_t)(y0 + y) * FRAME_W + (size_t)(x0 + x));
            uint8_t *d = rgba + 4 * ((size_t)y * p->w + (size_t)x);
            d[0] = s[0], d[1] = s[1], d[2] = s[2], d[3] = 255;
        }
    const PngResult r = png_write_rgba(rgba, (uint32_t)p->w, (uint32_t)p->h, png, len);
    free(rgba);
    if (r != PNG_OK) { exchange_fail(err, "%s: %s", p->file, png_result_str(r)); return 0; }
    return 1;
}

/* `file` of the piece may be written: absent, card.png still the generic one car new wrote, or --force */
static int may_write(const CarCardShot *s, const char *name, const char *file, ExchangeError *err)
{
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", s->piece, file);
    if (s->force || !fsio_exists(path)) return 1;
    if (strcmp(file, "card.png") == 0) {
        size_t have_len = 0, generic_len = 0;
        uint8_t *have = fsio_read_file(path, &have_len, err), *generic = NULL;
        const int same = have != NULL && car_card_generic_png(name, &generic, &generic_len, err) && have_len == generic_len &&
                         memcmp(have, generic, have_len) == 0;
        free(have), free(generic);
        if (same) return 1;
    }
    exchange_fail(err, "%s already exists: --force to replace it", path);
    return 0;
}

/* the three pictures from the frame, each checked before any is written */
static int write_pictures(const CarCardShot *s, const ModInfo *info, const uint8_t *rgb, ExchangeError *err)
{
    uint8_t *png[PICTURE_COUNT] = { NULL };
    size_t len[PICTURE_COUNT] = { 0 };
    int ok = 1;
    for (int i = 0; ok && i < PICTURE_COUNT; i++) ok = may_write(s, info->name, PICTURES[i].file, err) && crop(rgb, &PICTURES[i], &png[i], &len[i], err);
    for (int i = 0; ok && i < PICTURE_COUNT; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", s->piece, PICTURES[i].file);
        ok = fsio_write_atomic(path, png[i], len[i], err);
        if (ok) printf("car card: %s (%dx%d)\n", path, PICTURES[i].w, PICTURES[i].h);
    }
    for (int i = 0; i < PICTURE_COUNT; i++) free(png[i]);
    return ok;
}

static int shoot(const CarCardShot *s, const ModInfo *info, const char *tmp, ExchangeError *err)
{
    char user[PATH_MAX], mods[PATH_MAX], shot[PATH_MAX], node[16];
    snprintf(user, sizeof user, "%s/user", tmp), snprintf(mods, sizeof mods, "%s/mods", tmp), snprintf(shot, sizeof shot, "%s/frame.ppm", tmp);
    snprintf(node, sizeof node, "%d", s->node);
    if (!user_folder(user, s->time_name, err) || !copy_piece(s->piece, mods, err)) return 0;
    char *argv[] = { (char *)s->tnfs, "--data", (char *)s->data, "--user", user, "--mods", mods, "--drive", (char *)s->track, "--car",
                     (char *)info->id, "--racers", "1", "--node", node, "--screenshot", shot, NULL };
    const int status = run_game(argv, err);
    if (status < 0) return 0;
    if (status != 0) { exchange_fail(err, "the game did not race the car (exit %d): try `tnfs --mods %s --car %s` to see why", status, s->piece, info->id); return 0; }
    uint8_t *rgb = read_frame(shot, err);
    if (rgb == NULL) return 0;
    const int ok = write_pictures(s, info, rgb, err);
    free(rgb);
    return ok;
}

int car_card_shot(const CarCardShot *s, ExchangeError *err)
{
    ModInfo info;
    if (!car_card_read_info(s->piece, &info, err)) return 0;
    char tmp[PATH_MAX];
    const char *base = getenv("TMPDIR");
    snprintf(tmp, sizeof tmp, "%s/tnfs-card-XXXXXX", base != NULL && base[0] != '\0' ? base : "/tmp");
    if (mkdtemp(tmp) == NULL) { exchange_fail(err, "cannot make a scratch folder: %s", strerror(errno)); return 0; }
    const int ok = shoot(s, &info, tmp, err);
    fsio_remove_tree(tmp);
    return ok;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The update check over SDL3: a thread asks our API (api.bytesnrust.com; the question counts the start) through the system's curl (shipped with macOS, Windows 10+ and
 * nearly every Linux), so the game needs no TLS library. The command line is a constant: nothing from the network
 * or the player reaches the shell. Any failure (no curl, offline, rate limit) just ends as UPDATE_NONE. */
#include "game/platform/update.h"

#include "core/update/update_check.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define UPDATE_POPEN _popen
#define UPDATE_PCLOSE _pclose
#else
#define UPDATE_POPEN popen
#define UPDATE_PCLOSE pclose
#endif

#define UPDATE_ANSWER_MAX 65536

static SDL_AtomicInt state;          /* an UpdateState */
static SDL_AtomicInt started;
static char current[32];
static char latest_found[32];        /* written by the thread before `state` turns UPDATE_NEWER */

#if !defined(__EMSCRIPTEN__)
static int fetch_latest_json(char *buf, size_t cap)
{
    char cmd[512];
    if (!update_api_command(cmd, sizeof cmd, NULL, current)) return 0;
    FILE *pipe = UPDATE_POPEN(cmd, "r");
    if (!pipe) return 0;
    size_t n = 0, got;
    while (n + 1 < cap && (got = fread(buf + n, 1, cap - 1 - n, pipe)) > 0) n += got;
    buf[n] = '\0';
    /* a long answer (long release notes): keep only the start, where tag_name is, but read the rest so curl
     * ends normally instead of failing on a closed pipe */
    char drain[4096];
    while (fread(drain, 1, sizeof drain, pipe) > 0) {
    }
    int status = UPDATE_PCLOSE(pipe);
    return status == 0 && n > 0;
}

static int SDLCALL update_thread(void *unused)
{
    (void)unused;
    static char answer[UPDATE_ANSWER_MAX];
    char tag[32];
    int result = UPDATE_NONE;   /* offline, 503 {"latest":null} before the first release, HTML, garbage: no notice */
    if (fetch_latest_json(answer, sizeof answer) && update_parse_api_latest(answer, tag, sizeof tag)
        && update_version_compare(tag, current) > 0) {
        memcpy(latest_found, tag, sizeof latest_found);
        result = UPDATE_NEWER;
    }
    SDL_SetAtomicInt(&state, result);
    return 0;
}
#endif

#if !defined(__EMSCRIPTEN__)
typedef struct {
    char event[24];
    char version[40];
} StatsPost;

/* one anonymous event, on its own thread; the answer (204) is thrown away */
static int SDLCALL stats_thread(void *arg)
{
    StatsPost *post = arg;
    char cmd[512];
    if (update_api_command(cmd, sizeof cmd, post->event, post->version)) {
        FILE *pipe = UPDATE_POPEN(cmd, "r");
        if (pipe) {
            char drain[256];
            while (fread(drain, 1, sizeof drain, pipe) > 0) {
            }
            UPDATE_PCLOSE(pipe);
        }
    }
    SDL_free(post);
    return 0;
}
#endif

void platform_stats_post(const char *event, const char *version)
{
#if !defined(__EMSCRIPTEN__)
    if (!event || !version || strlen(event) >= sizeof(((StatsPost *)0)->event) || strlen(version) >= sizeof(((StatsPost *)0)->version))
        return;
    StatsPost *post = SDL_calloc(1, sizeof *post);
    if (!post) return;
    memcpy(post->event, event, strlen(event) + 1);
    memcpy(post->version, version, strlen(version) + 1);
    SDL_Thread *thread = SDL_CreateThread(stats_thread, "stats-event", post);
    if (!thread) {
        SDL_free(post);
        return;
    }
    SDL_DetachThread(thread);
#else
    (void)event, (void)version;
#endif
}

void platform_update_check_start(const char *current_version)
{
    if (!SDL_CompareAndSwapAtomicInt(&started, 0, 1)) return;
    if (!current_version || update_is_dev_build(current_version) || strlen(current_version) >= sizeof current) {
        SDL_SetAtomicInt(&state, UPDATE_NONE);
        return;
    }
    memcpy(current, current_version, strlen(current_version) + 1);
#if defined(__EMSCRIPTEN__)
    SDL_SetAtomicInt(&state, UPDATE_NONE);     /* the web build is always the served one */
#else
    SDL_Thread *thread = SDL_CreateThread(update_thread, "update-check", NULL);
    if (!thread) {
        SDL_SetAtomicInt(&state, UPDATE_NONE);
        return;
    }
    SDL_DetachThread(thread);
#endif
}

UpdateState platform_update_check_poll(char *latest, size_t cap)
{
    int s = SDL_GetAtomicInt(&state);
    if (s == UPDATE_NEWER && latest && cap > 0) {
        SDL_MemoryBarrierAcquire();
        size_t n = strlen(latest_found);
        if (n + 1 > cap) n = cap - 1;
        memcpy(latest, latest_found, n);
        latest[n] = '\0';
    }
    return (UpdateState)s;
}

int platform_open_releases_page(void)
{
    return SDL_OpenURL(UPDATE_RELEASES_PAGE) ? 1 : 0;
}

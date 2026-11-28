/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The update check's pure half: the newest release's tag out of GitHub's JSON answer, and which of two versions is
 * newer. No I/O here; the fetch lives in the platform layer (game/platform/update.h). */
#ifndef TNFS_UPDATE_CHECK_H
#define TNFS_UPDATE_CHECK_H

#include <stddef.h>

/* Where releases are published and announced. */
#define UPDATE_RELEASES_API  "https://api.github.com/repos/BytesNRust/spirited-drive/releases/latest"
#define UPDATE_RELEASES_PAGE "https://github.com/BytesNRust/spirited-drive/releases/latest"

/* Our own API (the Worker at api.bytesnrust.com): the update check asks it (that request counts the start) and three
 * one-shot anonymous events are POSTed to it. Versions go without a "+build" part.
 * UPDATE_API_NAME is the API's path and the requests' user agent, in one place (the Worker also answers the old "tnfs-restomod", for builds before it). */
#define UPDATE_API_NAME "spirited-drive"
#define UPDATE_API_BASE "https://api.bytesnrust.com/" UPDATE_API_NAME "/v1"

/* The system this build is for, sent with each request as p=<system> (the Worker keeps daily totals per system). */
#if defined(_WIN32)
#define UPDATE_API_PLATFORM "windows"
#elif defined(__APPLE__)
#define UPDATE_API_PLATFORM "macos"
#else
#define UPDATE_API_PLATFORM "linux"
#endif

/* the system's curl, never one earlier on PATH (Windows 10 1803 and later ship System32's) */
#if defined(_WIN32)
#define UPDATE_CURL "curl.exe"
#define UPDATE_NULL_DEV "NUL"
#define UPDATE_STDERR_TO_NULL ""   /* no shell there: the runner (game/platform/sdl3/process_pipe.c) points standard error at NUL */
#else
#define UPDATE_CURL "/usr/bin/curl"
#define UPDATE_NULL_DEV "/dev/null"
#define UPDATE_STDERR_TO_NULL " 2>" UPDATE_NULL_DEV
#endif

/* The "latest" of the API's answer {"latest":"0.9.1","page":"..."}: MAJOR.MINOR.PATCH[-pre], at most 32 characters, no
 * leading 'v'. 1 and `out` filled when found, else 0 and `out` empty ({"latest":null}, HTML, garbage, a version with
 * anything else in it). */
int update_parse_api_latest(const char *json, char *out, size_t cap);

/* `version` as the API takes it: everything from '+' on dropped; it must be MAJOR.MINOR.PATCH[-pre] of letters, digits,
 * '.' and '-' only, at most 32 characters. 1 and `out` filled, else 0 and `out` empty. */
int update_api_version(const char *version, char *out, size_t cap);

/* The curl command line (a constant shape: nothing but the three event names and a checked version goes into it).
 * event NULL: GET /latest?v=<version>&p=<system>, the answer on stdout. Else event is "first_start", "data_ready" or "first_race":
 * POST /event?e=<event>&v=<version>&p=<system>, the answer thrown away (system: UPDATE_API_PLATFORM). --max-time 5 and https only. 1 when it fits in `cap`. */
int update_api_command(char *out, size_t cap, const char *event, const char *version);

/* update_api_command against another base ("http://127.0.0.1:8123/spirited-drive/v1"; letters, digits and : / . _ - only, else 0) and, with allow_http, over plain
 * http as well: for the update test builds' own server and proxy (game/platform/sdl3/update_sdl3.c reads TNFS_UPDATE_API_BASE only when built with
 * TNFS_UPDATE_TEST). update_api_command is this with UPDATE_API_BASE and https only. */
int update_api_command_at(char *out, size_t cap, const char *base, int allow_http, const char *event, const char *version);

/* The "tag_name" of a GitHub release JSON, without a leading 'v' ("v0.9.1" -> "0.9.1").
 * 1 and `out` filled (NUL terminated) when found and it fits in `cap`, else 0 and `out` left empty. */
int update_parse_latest_tag(const char *json, char *out, size_t cap);

/* Semantic versions MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]: <0 when a is older than b, 0 same, >0 newer.
 * A pre-release sorts before its release (0.9.0-beta.2 < 0.9.0); build metadata is ignored.
 * Returns 0 when either one cannot be read. */
int update_version_compare(const char *a, const char *b);

/* 1 when `version` is a local development build (a "-dev" pre-release): those never ask for updates. */
int update_is_dev_build(const char *version);

/* Windows: `command` (an update_api_command line, which starts with UPDATE_CURL) with the program named by its full path in `system_dir` (a folder like C:\Windows\System32 gives "C:\Windows\System32\curl.exe" in quotes, then the arguments), so the
 * curl run is the system's and not one found beside the game, in the current folder or earlier on PATH. 1 when it fits `cap`; 0 when `command` does not start with curl.exe, `system_dir` is empty or has a quote, or it does not fit. */
int update_windows_command(const char *command, const char *system_dir, char *out, size_t cap);

/* what the log says of a curl that did not exit with 0: `status` as process_pipe_close gives it (its exit status, or -1 when it did not end normally) */
void update_curl_reason(int status, char *out, size_t cap);

#endif

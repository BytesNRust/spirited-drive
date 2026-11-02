/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The update check's pure half: the newest release's tag out of GitHub's JSON answer, and which of two versions is
 * newer. No I/O here; the fetch lives in the platform layer (game/platform/update.h). */
#ifndef TNFS_UPDATE_CHECK_H
#define TNFS_UPDATE_CHECK_H

#include <stddef.h>

/* Where releases are published and announced. */
#define UPDATE_RELEASES_API  "https://api.github.com/repos/BytesNRust/tnfs-restomod/releases/latest"
#define UPDATE_RELEASES_PAGE "https://github.com/BytesNRust/tnfs-restomod/releases/latest"

/* The "tag_name" of a GitHub release JSON, without a leading 'v' ("v0.9.1" -> "0.9.1").
 * 1 and `out` filled (NUL terminated) when found and it fits in `cap`, else 0 and `out` left empty. */
int update_parse_latest_tag(const char *json, char *out, size_t cap);

/* Semantic versions MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]: <0 when a is older than b, 0 same, >0 newer.
 * A pre-release sorts before its release (0.9.0-beta.2 < 0.9.0); build metadata is ignored.
 * Returns 0 when either one cannot be read. */
int update_version_compare(const char *a, const char *b);

/* 1 when `version` is a local development build (a "-dev" pre-release): those never ask for updates. */
int update_is_dev_build(const char *version);

#endif

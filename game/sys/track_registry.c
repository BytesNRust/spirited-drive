/* See track_registry.h. */
#include "game/sys/track_registry.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static TrackPiece pieces[TRACK_ADDED_MAX];
static int used[TRACK_ADDED_MAX];

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list ap;
        va_start(ap, format);
        vsnprintf(why, cap, format, ap);
        va_end(ap);
    }
    return -1;
}

void track_registry_clear(void)
{
    memset(used, 0, sizeof used);
    memset(pieces, 0, sizeof pieces);
}

int track_registry_count(void)
{
    int n = 0;
    for (int k = 0; k < TRACK_ADDED_MAX; k++) n += used[k];
    return n;
}

int track_registry_is_added(int track)
{
    const int k = track - TRACK_ADDED_FIRST;
    return k >= 0 && k < TRACK_ADDED_MAX && used[k];
}

const TrackPiece *track_registry_piece(int track)
{
    return track_registry_is_added(track) ? &pieces[track - TRACK_ADDED_FIRST] : NULL;
}

int track_registry_tracks(int16_t out[], int cap)
{
    int n = 0;
    for (int k = 0; k < TRACK_ADDED_MAX && n < cap; k++)
        if (used[k]) out[n++] = (int16_t)(TRACK_ADDED_FIRST + k);
    return n;
}

int track_registry_find(const char *id)
{
    if (id == NULL) return -1;
    for (int k = 0; k < TRACK_ADDED_MAX; k++)
        if (used[k] && strcmp(pieces[k].id, id) == 0) return TRACK_ADDED_FIRST + k;
    return -1;
}

/* 1 when an added track other than slot `self` (-1: any) has `stem`, whatever the case */
static int stem_added(const char *stem, int self)
{
    for (int k = 0; k < TRACK_ADDED_MAX; k++)
        if (k != self && used[k] && strcasecmp(stem, pieces[k].stem) == 0) return 1;
    return 0;
}

/* -1 with the reason when `piece` may not be track `self`'s (-1: a new track's), else 0 */
static int piece_ok(const TrackPiece *piece, int self, char *why, size_t why_cap)
{
    if (!track_piece_stem_ok(piece->stem))
        return refuse(why, why_cap, "the stem \"%s\" is not 1 to %d of A-Z and 0-9, or begins like one of the CD's (AL, CL, CY, TR)", piece->stem, TRACK_PIECE_STEM_MAX);
    if (stem_added(piece->stem, self)) return refuse(why, why_cap, "the stem %s is another added track's", piece->stem);
    if (piece->spec.kind != TRACK_KIND_STAGE) return refuse(why, why_cap, "this version adds A-to-B tracks only");
    if (piece->fingerprint == 0) return refuse(why, why_cap, "the piece has no fingerprint");
    return 0;
}

int track_registry_add(const TrackPiece *piece, char *why, size_t why_cap)
{
    if (piece == NULL || piece->id[0] == '\0') return refuse(why, why_cap, "a track piece has no id");
    if (track_registry_find(piece->id) >= 0) return refuse(why, why_cap, "the id %s is a track already", piece->id);
    if (piece_ok(piece, -1, why, why_cap) < 0) return -1;
    for (int k = 0; k < TRACK_ADDED_MAX; k++) {
        if (used[k]) continue;
        pieces[k] = *piece;
        used[k] = 1;
        return TRACK_ADDED_FIRST + k;
    }
    return refuse(why, why_cap, "there are %d added tracks already, the most there may be", TRACK_ADDED_MAX);
}

int track_registry_update(const TrackPiece *piece, char *why, size_t why_cap)
{
    const int track = piece != NULL ? track_registry_find(piece->id) : -1;
    if (track < 0) return refuse(why, why_cap, "no added track has the id %s", piece != NULL ? piece->id : "");
    if (piece_ok(piece, track - TRACK_ADDED_FIRST, why, why_cap) < 0) return -1;
    pieces[track - TRACK_ADDED_FIRST] = *piece;
    return track;
}

int track_registry_remove(const char *id)
{
    const int track = track_registry_find(id);
    if (track < 0) return 0;
    used[track - TRACK_ADDED_FIRST] = 0;
    memset(&pieces[track - TRACK_ADDED_FIRST], 0, sizeof pieces[0]);
    return 1;
}

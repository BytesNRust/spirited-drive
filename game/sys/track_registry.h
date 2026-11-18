/* port (dep59 step 5): the tracks a mod adds (notes/dep59-step5-tracks-plan.md), beside the CD's. An added track has a menu number for this run
 * only, from TRACK_ADDED_FIRST (the plan's TrackPick: 0..9 the CD's, 10..15 the port's entries, 16..79 these); it keeps it while it stays
 * registered, and a number set free goes to a new track only, so a number in another's hand never comes to name another track. Outside the run
 * a track is named by its id. Same shape as car_registry.h. */
#ifndef TNFS_TRACK_REGISTRY_H
#define TNFS_TRACK_REGISTRY_H

#include "formats/mod_ini.h"
#include "formats/track_piece.h"

#include <stddef.h>
#include <stdint.h>

enum { TRACK_ADDED_FIRST = 16, TRACK_ADDED_MAX = 64 };

typedef struct {
    char id[MOD_INI_ID_MAX + 1], name[MOD_INI_NAME_MAX + 1], stem[MOD_INI_STEM_MAX + 1], author[MOD_INI_AUTHOR_MAX + 1];
    char description[MOD_INI_DESCRIPTION_MAX + 1];   /* mod.ini description as written: the page shows it */
    char length[MOD_INI_FIGURE_MAX + 1];   /* mod.ini length= as written ("14.2 km"), "" when not given */
    TrackPieceSpec spec;                   /* kind, segments, road, edge and hours: what the race reads of it */
    uint8_t tint[3];                       /* its page's background tone: mod.ini showcase_tint, else the sepia (showcase_tint.h); not in the fingerprint */
    int tinted;                            /* 1 when mod.ini gave showcase_tint: the page's background is turned to it, else it is the common sepia as it is */
    uint8_t print[32];                     /* the SHA-256 print of the same stream (piece_print.h), all zeros for none: what records are kept under */
    int card_name_off;                     /* mod.ini card_name=off: no name drawn over its card.png (its picture shows its own) */
    uint32_t fingerprint;                  /* track_piece_fingerprint, never 0: what decides how it races (a replay's check) */
    char dir[512];                         /* the folder it was read from, for its pictures */
} TrackPiece;

/* no added track */
void track_registry_clear(void);
/* `piece` registered (a stem track_piece_stem_ok accepts and no other added track's): its number, the lowest free one, or -1 with the reason
 * (English, for the log) in `why` */
int track_registry_add(const TrackPiece *piece, char *why, size_t why_cap);
/* the registered track of `piece`'s id made `piece`, keeping its number (a reload of a piece that stays: track_pieces.h); its number, or -1
 * with the reason in `why` (then it is as it was) */
int track_registry_update(const TrackPiece *piece, char *why, size_t why_cap);
/* the track of id `id` taken out, its number set free; 1 when there was one */
int track_registry_remove(const char *id);
int track_registry_count(void);
/* 1 for a registered added track's menu number */
int track_registry_is_added(int track);
/* an added track's piece; NULL for a number not registered */
const TrackPiece *track_registry_piece(int track);
/* the registered added tracks' numbers, lowest first, into `out` (up to `cap`); how many were put there */
int track_registry_tracks(int16_t out[], int cap);
/* the number of the track of id `id`, or -1 */
int track_registry_find(const char *id);

#endif

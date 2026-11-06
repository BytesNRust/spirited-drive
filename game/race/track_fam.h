/* The track's FAM, its art for the time of day and the variant (004510e8,
 * from 004511fc into an RRSM_Buffer slot). */
#ifndef TNFS_TRACK_FAM_H
#define TNFS_TRACK_FAM_H

#include <stddef.h>
#include <stdint.h>

enum { TRACK_FAM_SIZE = 0x162400, TRACK_FAM_NAME = 100 };

typedef struct {
    int32_t time_of_day;        /* 004fa0ec: 0..2, 1 midday (004278c0: 1 with a variant, else 004fbae0[track]) */
    int32_t variant_m;          /* 004fbad8 */
    int32_t variant_r;          /* 004fbadc */
    int32_t track;              /* 004fb7c2 (a dword): 3 has no "r" art */
    const char *stem;           /* 004fd0f4: the track's */
    const char *dir;            /* 00524508: "simdata/ntrackFm/" */
    const char *dir_midday;     /* 005244b8: "simdata/etrackFm/" ("gtrackFm" in German) */
    /* port: 1 takes the rally's art when variant_m and variant_r are both set (the mirror's geometry with the
     * rally's look), where 004510e8 gives the mirror's "m" the first look and the rally's "r" is never reached.
     * 0 is the exe. A variant on its own is the same either way. */
    int port_both_variants;
    /* port: 1 with the mode's file's lunar_springs: the rally on Rusty Springs reads the moon's FAM (lunar_springs.h). Last,
     * so the initialisers above keep their meaning. 0 is the exe. */
    int port_lunar;
    /* port: 1 reverses a circuit's file as it loads (tri_reverse.h); 0 is the exe. Last, as above. */
    int port_reverse;
} TrackFamInputs;

typedef struct {
    void *ctx;
    int32_t (*open)(void *ctx, const char *name);                           /* 00488404 (cdecl name, &handle, 0, 0) */
    void (*read)(void *ctx, int32_t file, void *buffer, int32_t size);      /* 004884d8 (cdecl) */
    void (*close)(void *ctx, int32_t file);                                 /* 004884c8 (cdecl) */
} TrackFamOps;

/* 004510e8's name: a time of day other than 1 "%s/%s_T%.2d.fam" in dir (the
 * dawn and dusk art); at midday with variant_m "%s/%s_M%.2d.fam", with
 * variant_r and a track other than 3 "%s/%s_R%.2d.fam", in dir; else
 * "%s/%s_%.3d.fam" in dir_midday. port_both_variants 1 reaches the rally's
 * "r" first when both variants are set. Port guard: cut at `size` (the
 * original's 100 bytes on its stack run on). */
void track_fam_name(const TrackFamInputs *in, int32_t number, char *out, size_t size);

/* 004510e8 (watcall number, buffer): the name opened, TRACK_FAM_SIZE bytes
 * read into buffer, closed. */
void track_fam_load(const TrackFamInputs *in, int32_t number, void *buffer, const TrackFamOps *ops);

/* port: 1 when `track` (0..8, the menus') takes its time of day: never Lost Vegas (8), and with a variant on only
 * when `variant_time` (the mode's file's; the original takes midday) and the art exists (Rusty Springs' rally, TR5, has
 * no evening FAM). */
int track_fam_time_offered(int track, int variant_m, int variant_r, int variant_time);
/* 004278c0's 004fa0ec: `time` (the track's, 0..2) without a variant, 1 (midday) with one; port: `time` with one too
 * when track_fam_time_offered says so. */
int track_fam_time_of(int track, int variant_m, int variant_r, int variant_time, int time);

#endif

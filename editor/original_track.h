/* An original track of the game as a project of the workspace (dep103): <workspace>/tracks/<STEM>/ with
 *   original.ini                   kind=original-track and stem=<STEM> (what marks the folder; an own track has project.ini)
 *   light/track_light/<STEM>{A,B,C,N}.txt, light/track/<STEM>.txt    the light folder the Track light panels edit
 *   art-source/masks/<name>.psd    the owner's mask PSDs, copied (never moved) from the art folder
 *   art-generated/track_masks/     written by Studio (phase B)
 * Adding copies from the game's data/ (and the art folder) and never overwrites a file that is there; publishing copies the light files back into data/ after a preview.
 * Pure C, POSIX files, no window. */
#ifndef TNFS_EDITOR_ORIGINAL_TRACK_H
#define TNFS_EDITOR_ORIGINAL_TRACK_H

#include <stddef.h>

enum { OT_STEMS = 16, OT_ITEMS = 16, OT_NAME = 64, OT_WHY = 200 };

/* the 16 CD tracks' stems in the game's order: "AL1".."TR7"; NULL past OT_STEMS */
const char *original_track_stem(int index);

/* <workspace>/tracks/<stem> into `out`; 0 when it does not fit or the stem is not one of the 16 */
int original_track_dir(const char *workspace, const char *stem, char *out, size_t cap);

/* whether `dir` is an original track's folder (its original.ini says so), and its stem */
int original_track_present(const char *dir);
int original_track_stem_of(const char *dir, char *stem, size_t cap);

/* <dir>/light, the light folder of an original track's folder */
int original_track_light_dir(const char *dir, char *out, size_t cap);

typedef struct {
    int tracks;        /* folders made or found */
    int light_files;   /* light and places files copied */
    int psds;          /* mask PSDs copied */
    int kept;          /* files that were there already: left as they are */
    int skipped;       /* PSD folder entries not copied: backups (an extra dot in the name) and anything that is not a .psd */
} OriginalAdd;

/* Adds one track: the folder, original.ini, the light files of `data_dir` (track_light/<STEM>?.txt, track/<STEM>.txt) and, when `psd_root` is not NULL and has <STEM>/, its PSDs.
 * `out` is added to, not reset. 1, or 0 with `why` (no data/ files for it, a folder that cannot be made). */
int original_track_add(const char *workspace, const char *stem, const char *data_dir, const char *psd_root, OriginalAdd *out, char *why, size_t cap);

/* All sixteen; a track whose light files are missing stops it with `why` (what was added before stays). */
int original_track_add_all(const char *workspace, const char *data_dir, const char *psd_root, OriginalAdd *out, char *why, size_t cap);

/* The acceptance check of a migration: folder `a` and folder `b` of the same track (one migrated, one made by hand through original_track_add) hold the same files with the same bytes,
 * except what is derived from the owner's PSDs: art-source/masks/ and art-generated/track_masks/. Returns how many files differ, are missing or are extra OUTSIDE those two; `report` gets
 * one line for each (up to its size) and the totals "N files compared, M in the mask folders". */
int original_track_compare(const char *a, const char *b, char *report, size_t cap);

/* Makes the track `stem` by hand in a scratch folder (original_track_add from `data_dir`, no PSDs) and compares it with the migrated `dir` as original_track_compare does; the scratch folder is removed.
 * Returns the differences found, or -1 with `why`. */
int original_track_acceptance(const char *dir, const char *stem, const char *data_dir, char *report, size_t cap, char *why, size_t why_cap);

typedef enum { OT_SAME = 0, OT_CHANGED, OT_NEW } OriginalState;

typedef struct {
    char file[OT_NAME];   /* relative to data/: "track_light/AL1B.txt" */
    OriginalState state;
    int lines_added, lines_removed;   /* changed: the lines the project's file has that data/'s has not, and the other way round; for a mask PNG ("track_masks/CL1/slot_06.png"): the texels it marks that data/'s does not, and the reverse; both -1 when its size is not data/'s */
} OriginalPublishItem;

#define OT_PUBLISH_MAX 264   /* the 5 light files and the masks (track_masks.h adds one item for each PNG, up to 256) */
typedef struct {
    OriginalPublishItem item[OT_PUBLISH_MAX];
    int count, same, changed, added;
} OriginalPublish;

/* What publishing would do: each light file of the project compared with data/'s (same, changed, or new when data/ has none). 1, or 0 with `why`. Read only. */
int original_track_publish_plan(const char *dir, const char *data_dir, OriginalPublish *out, char *why, size_t cap);

/* Copies the files of the plan that are changed or new into data/. `written` gets how many. 1, or 0 with `why` (the files before the failing one are written). */
int original_track_publish(const char *dir, const char *data_dir, int *written, char *why, size_t cap);

/* The preview as text, one line a file, for the dialog and the tests */
void original_track_publish_text(const OriginalPublish *p, const char *stem, char *out, size_t cap);

#endif

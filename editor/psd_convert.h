/* The PSDs of a car turned into the game's mask PNGs, with the C reader (formats/psd_mask.h): no python, nothing to install. One folder of PSDs in
 * (<tex>.psd, <tex>_lo.psd, dash.psd; a backup, a name with a second dot, is not a PSD of the car), PNGs out: <masks_out>/<folder>/<tex>_mask.png and, for the
 * dash, <dash_out>/<folder>/dash_mask.png and dash_color.png (the last only when the PSD has a colour layer). The PSDs are only read. POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_PSD_CONVERT_H
#define TNFS_TRACK_EDITOR_PSD_CONVERT_H

#include <stddef.h>

typedef struct {
    int masks;        /* body PNGs written */
    int dash_files;   /* dash PNGs written (0, 1 or 2) */
} PsdConverted;

/* Every PSD of `psd_dir` converted (`cancel`, may be NULL, is looked at before each one: when it is not 0 the conversion ends with "stopped"); 1 when all were, with the counts. 0 with `why` (English: the PSD's name and what is wrong with it) at the first that cannot be
 * read or written; what was written before it stays in the output folders (the caller's scratch). */
int psd_convert(const char *psd_dir, const char *folder, const char *masks_out, const char *dash_out, const int *cancel, PsdConverted *done, char *why, size_t why_cap);

#endif

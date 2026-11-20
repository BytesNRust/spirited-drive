/* Convert PSD (dep98 item 1): the owner's PSDs of an original car turned into the project's mask PNGs, in the background. It runs tools/car_mask_from_psd.py
 * (and tools/dash_mask_from_psd.py when the car's folder has a dash.psd) into a scratch folder, and only when both ended well copies the PNGs into
 * <piece>/car_masks/<folder>/ and <piece>/dash_masks/<folder>/ (atomic file by file): a failed conversion leaves the project as it was. Nothing is ever
 * written into the PSD folder. POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_PSD_JOB_H
#define TNFS_TRACK_EDITOR_PSD_JOB_H

#include "proc_run.h"

#include <stddef.h>

enum { PSD_PATH = 4096, PSD_TEXT = 400 };

typedef struct {
    ProcRun run;
    int stage;                  /* 0 idle, 1 the body masks, 2 the dash */
    int finished, ok;           /* it ended; and well */
    char tools[PSD_PATH];       /* the folder of the two scripts */
    char psd_dir[PSD_PATH];     /* the car's PSD folder (read only) */
    char folder[64];            /* the project's mask folder name: car_masks/<folder> */
    char piece[PSD_PATH];       /* the project's folder that holds car_masks/ */
    char scratch[PSD_PATH];     /* the temporary folder the scripts write into */
    int masks, dash_files;      /* the PNGs copied into the project */
    char text[PSD_TEXT];        /* what happened, for the status line */
} PsdJob;

void psd_job_init(PsdJob *job);
/* The PSD folder of `folder` inside `psd_root` (names match without case: TSupra, TSUPRA); 1 with it in `out`, else 0. */
int psd_job_find(const char *psd_root, const char *folder, char *out, size_t cap);
/* Starts the conversion; 1, or 0 with `why` (no scripts, a scratch folder that cannot be made, python missing). */
int psd_job_start(PsdJob *job, const char *tools, const char *psd_dir, const char *folder, const char *piece, char *why, size_t why_cap);
/* 1 while it runs; when it ends `finished`, `ok` and `text` are set and the project's files are copied (or not) */
int psd_job_poll(PsdJob *job);
int psd_job_busy(const PsdJob *job);
void psd_job_stop(PsdJob *job);
void psd_job_free(PsdJob *job);

#endif

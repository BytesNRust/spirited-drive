/* Convert PSD (dep98 item 1): a car's PSDs turned into the project's mask PNGs, in the background, with the C reader (psd_convert.h): no python. The PNGs are
 * made in a scratch folder (every PSD of the folder, the dash too when it has a dash.psd) and only when all were made copied into the project's masks folder
 * and dash masks folder (atomic file by file): a failed conversion leaves the project as it was. A kit's are flat (art-generated/car_masks, art-generated/dash_masks)
 * and every PNG written is hashed into its manifest (psd_manifest.h). A PNG whose hash is no longer the manifest's was edited by hand: it is kept, not replaced
 * (`kept`, `kept_names`), unless `force` (Regenerate from PSD); a legacy project's are <piece>/car_masks/<folder>/ and <piece>/dash_masks/<folder>/, with no manifest. Nothing is ever written into the PSD folder.
 * POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_PSD_JOB_H
#define TNFS_TRACK_EDITOR_PSD_JOB_H

#include <pthread.h>

#include <stddef.h>

enum { PSD_PATH = 4096, PSD_TEXT = 400 };

typedef struct {
    pthread_t thread;
    int started;                /* the thread was made and not joined yet */
    int cancel;                 /* set by psd_job_stop, looked at by the thread between two PSDs (atomic) */
    int running;                /* the thread has not ended: read under `lock` */
    pthread_mutex_t lock;
    int ready;                  /* `lock` is made */
    int conv_ok;                /* what the thread found, read after it is joined */
    char conv_why[PSD_TEXT];
    int finished, ok;           /* it ended; and well */
    char psd_dir[PSD_PATH];     /* the car's PSD folder (read only) */
    char folder[64];            /* the project's mask folder name: car_masks/<folder> */
    char masks_dst[PSD_PATH];   /* where the body masks go (the folder is made when it is not there) */
    char dash_dst[PSD_PATH];    /* where dash_mask.png and dash_color.png go */
    char manifest[PSD_PATH];    /* the manifest the PNGs written are hashed into; "" for none */
    char scratch[PSD_PATH];     /* the temporary folder the PNGs are written into */
    int masks, dash_files;      /* the PNGs copied into the project */
    int force;                  /* Regenerate from PSD: a PNG edited by hand is replaced too (else it is kept as it is) */
    int kept;                   /* the PNGs kept because they were edited by hand (psd_manifest.h) */
    char kept_names[PSD_TEXT];  /* their names, "frnt_mask.png, rear_mask.png" (cut if long) */
    char text[PSD_TEXT];        /* what happened, for the status line */
} PsdJob;

void psd_job_init(PsdJob *job);
/* The PSD folder of `folder` inside `psd_root` (names match without case: TSupra, TSUPRA); 1 with it in `out`, else 0. */
int psd_job_find(const char *psd_root, const char *folder, char *out, size_t cap);
/* Starts the conversion (psd_convert.h, in a thread); 1, or 0 with `why` (a scratch folder that cannot be made, one running already). */
int psd_job_start(PsdJob *job, const char *psd_dir, const char *folder, const char *masks_dst, const char *dash_dst, const char *manifest, int force, char *why, size_t why_cap);
/* 1 while it runs; when it ends `finished`, `ok` and `text` are set and the project's files are copied (or not) */
int psd_job_poll(PsdJob *job);
int psd_job_busy(const PsdJob *job);
/* waits for the conversion to end (it is short) and puts nothing in the project */
void psd_job_stop(PsdJob *job);
void psd_job_free(PsdJob *job);

#endif

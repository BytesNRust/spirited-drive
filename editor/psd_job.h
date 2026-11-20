/* Convert PSD (dep98 item 1): the owner's PSDs of an original car turned into the project's mask PNGs, in the background. It runs tools/car_mask_from_psd.py
 * (and tools/dash_mask_from_psd.py when the car's folder has a dash.psd) into a scratch folder, and only when both ended well copies the PNGs into
 * <piece>/car_masks/<folder>/ and <piece>/dash_masks/<folder>/ (atomic file by file): a failed conversion leaves the project as it was. Nothing is ever
 * written into the PSD folder. POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_PSD_JOB_H
#define TNFS_TRACK_EDITOR_PSD_JOB_H

#include "proc_run.h"

#include <stddef.h>

enum { PSD_PATH = 4096, PSD_TEXT = 400 };

/* Which python runs the scripts: an app opened from the Finder has PATH=/usr/bin:/bin:..., so "python3" is Apple's, which has no psd-tools; the owner's is most likely
 * Homebrew's. The candidates are tried in order, each with `-c "import psd_tools, PIL"` in the background; the first that succeeds is kept for the session. */
enum { PY_CANDIDATES = 8 };
typedef struct {
    ProcRun run;
    char cand[PY_CANDIDATES][PSD_PATH];   /* "" stands for python3 found through PATH (run as /usr/bin/env python3) */
    int count, next;
    double started;             /* when the candidate being tried was started (a monotonic clock, seconds) */
    double timeout_s;           /* a candidate that has not answered by then is given up (a shim or a stalled import must not hold the button forever); 12 */
    int state;                  /* 0 not started, 1 looking, 2 done */
    int found;                  /* 1: `python` is the one (a "" there is the PATH's) */
    char python[PSD_PATH];
    char tried[PSD_TEXT];       /* the ones that could not import psd_tools and Pillow, for the tooltip */
} PyProbe;

/* The list to try: `env_python` (TNFS_PYTHON, may be NULL or ""), /opt/homebrew/bin/python3, /usr/local/bin/python3, then the PATH's. */
void py_probe_init(PyProbe *probe, const char *env_python);
/* The same with a list of your own (tests). */
void py_probe_init_list(PyProbe *probe, const char *const *cands, int count);
/* Starts the probe when it has not started, and advances it: 1 while it looks; 0 when done (`found`, `python`, `tried`). */
int py_probe_poll(PyProbe *probe);
void py_probe_free(PyProbe *probe);

typedef struct {
    ProcRun run;
    char python[PSD_PATH];      /* the interpreter ("" for the PATH's python3) */
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
/* Starts the conversion with the interpreter `python` ("" for the PATH's python3); 1, or 0 with `why` (no scripts, a scratch folder that cannot be made, python missing). */
int psd_job_start(PsdJob *job, const char *python, const char *tools, const char *psd_dir, const char *folder, const char *piece, char *why, size_t why_cap);
/* 1 while it runs; when it ends `finished`, `ok` and `text` are set and the project's files are copied (or not) */
int psd_job_poll(PsdJob *job);
int psd_job_busy(const PsdJob *job);
void psd_job_stop(PsdJob *job);
void psd_job_free(PsdJob *job);

#endif

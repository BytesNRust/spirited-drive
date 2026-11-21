/* The owner's PSDs into a migrated kit (dep100 item 6), in C with the PSD reader (formats/psd_mask.h), no python. */
#ifndef TNFS_TOOLS_CAR_MIGRATE_ART_H
#define TNFS_TOOLS_CAR_MIGRATE_ART_H

#include <stdio.h>

typedef struct {
    int same;                    /* masks byte for byte the game's data/ file */
    int changed_count;           /* masks written from the PSD on purpose */
    int picture_count;           /* the same picture as data/'s, other bytes (the older PNGs' palette): data/'s bytes kept */
    int failure_count;
    char changed[16][96];
    char picture[16][96];
    char failures[64][320];
    char skipped[96][96];        /* PSDs not copied: a backup (a name with an extra dot) */
    int skipped_count, psds_copied;
} MigrateReport;

/* Copies `art_root`/<stem>'s PSDs (names with no extra dot) into `kit`/art-source/masks and checks the masks they make against the kit's art-generated (the game's
 * data/ bytes): equal is fine; one of the owner's five changes (AXXESS frnt, frnt_lo, side, side_lo; ANSX dash, with its new dash_color) is written; the same
 * picture in other bytes is kept as data/ has it; anything else is a failure in `r`. `art_root` is only read. */
void migrate_art(const char *kit, const char *stem, const char *art_root, MigrateReport *r);

void migrate_report_print(const MigrateReport *r, FILE *out);

#endif

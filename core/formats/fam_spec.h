/* The text spec of a track's art (`tnfs-studio fam build SPEC`): one picture per line, in the order the file takes them.
 *
 *   # comment
 *   terrain 0 grass.png      terrain texture 0 (the ids 0, 1, ... in order, one per line)
 *   terrain 1 asphalt.png
 *   scenery trees.png        a scenery texture (optional, in order)
 *   horizon sky.png          the horizon, exactly one line
 *
 * Paths are as written; the caller resolves them (relative to the spec's folder). Blank lines and text after '#' are ignored.
 * What the parser refuses is the syntax; the counts the game holds are checked by fam_build (fam_build.h). */
#ifndef TNFS_FAM_SPEC_H
#define TNFS_FAM_SPEC_H

#include "formats/exchange.h"

#include <stddef.h>

enum { FAM_SPEC_PATH = 512, FAM_SPEC_ITEMS = 256 };

typedef enum { FAM_SPEC_TERRAIN, FAM_SPEC_SCENERY, FAM_SPEC_HORIZON } FamSpecKind;

typedef struct {
    FamSpecKind kind;
    char path[FAM_SPEC_PATH];
} FamSpecItem;

typedef struct {
    FamSpecItem item[FAM_SPEC_ITEMS];
    size_t count;
} FamSpec;

/* 1 with `out` filled in the spec's order; 0 with the line and the reason in `err` */
int fam_spec_parse(const char *text, FamSpec *out, ExchangeError *err);

#endif

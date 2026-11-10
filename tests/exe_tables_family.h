/* The tables read from the real exe, checked against a fixture instead of the generated tables the repository no longer keeps. */
#ifndef TNFS_TESTS_EXE_TABLES_FAMILY_H
#define TNFS_TESTS_EXE_TABLES_FAMILY_H

#include "game/exe/exe_tables.h"
#include "game/sys/sha256.h"

#include <stdio.h>
#include <string.h>

/* The fixture for the tables read from the real exe: the SHA-256 of each family's members as the extractor leaves them (a table
 * starts at 0xAA, so the extractor's own bytes are all that is hashed; the names count up to their NUL). The hashes are in
 * tests/exe_tables_fixture.h, taken from the exe whose size and SHA-256 are in data_manifest.c; they hold no table, so the
 * repository keeps none of the exe's data. A table that changes by one byte changes its family's hash. */
static void fam_raw(Sha256 *s, const void *p, size_t n) { sha256_update(s, p, n); }
static void fam_names(Sha256 *s, const char *p, size_t count, size_t stride)
{
    for (size_t i = 0; i < count; i++) sha256_update(s, p + i * stride, strlen(p + i * stride) + 1);
}

static void family_sha(const ExeTables *t, const char *family, char hex[65])
{
    Sha256 s;
    sha256_init(&s);
    if (strcmp(family, "ai") == 0) fam_raw(&s, t->ai_driver_data, sizeof t->ai_driver_data);
    else if (strcmp(family, "split") == 0) fam_raw(&s, t->race_split_tables, sizeof t->race_split_tables);
    else if (strcmp(family, "view") == 0) {
        fam_raw(&s, t->race_view_layouts, sizeof t->race_view_layouts);
        fam_raw(&s, t->race_view_rows, sizeof t->race_view_rows);
        fam_raw(&s, t->race_view_model_y, sizeof t->race_view_model_y);
    } else if (strcmp(family, "track") == 0) {
        fam_raw(&s, t->track_lod_by_distance, sizeof t->track_lod_by_distance);
        fam_raw(&s, t->track_lods, sizeof t->track_lods);
        fam_raw(&s, t->track_uvs, sizeof t->track_uvs);
        fam_raw(&s, t->track_polygons, sizeof t->track_polygons);
        fam_raw(&s, t->track_variant_near, sizeof t->track_variant_near);
        fam_raw(&s, t->track_variant_far, sizeof t->track_variant_far);
        fam_raw(&s, t->track_subdivide_z, sizeof t->track_subdivide_z);
    } else if (strcmp(family, "menu") == 0) {
        fam_raw(&s, t->menu_item_x, sizeof t->menu_item_x);
        fam_raw(&s, t->menu_item_y, sizeof t->menu_item_y);
        fam_raw(&s, t->menu_x_offset, sizeof t->menu_x_offset);
        fam_raw(&s, t->menu_y_offset, sizeof t->menu_y_offset);
        fam_names(&s, &t->menu_track_name[0][0], RACE_MENU_TRACKS, EXE_NAME_MAX);
        fam_names(&s, &t->menu_marker_name[0][0], RACE_MENU_CARS, 5);
        fam_raw(&s, t->menu_pool, sizeof t->menu_pool);
        fam_raw(&s, t->menu_map_first, sizeof t->menu_map_first);
        fam_raw(&s, t->menu_map_count, sizeof t->menu_map_count);
    } else if (strcmp(family, "config") == 0) {
        fam_names(&s, &t->config_row_name[0][0], CONFIG_RECORD_ROW_COUNT, EXE_NAME_MAX);
        fam_raw(&s, t->config_row_number, sizeof t->config_row_number);
        fam_names(&s, &t->config_name[0][0], 3, EXE_NAME_MAX);
    }
    sha256_hex(&s, hex);
}

#include "tests/exe_tables_fixture.h"

/* 1 when the family's tables are the ones the fixture holds; otherwise prints both hashes (and 0). */
static int family_ok(const ExeTables *t, const char *family)
{
    char hex[65];
    family_sha(t, family, hex);
    for (size_t i = 0; i < sizeof EXE_FAMILY_SHA / sizeof EXE_FAMILY_SHA[0]; i++)
        if (strcmp(EXE_FAMILY_SHA[i].family, family) == 0) {
            if (strcmp(EXE_FAMILY_SHA[i].sha256, hex) == 0) return 1;
            fprintf(stderr, "family %s: sha256 %s, the fixture has %s\n", family, hex, EXE_FAMILY_SHA[i].sha256);
            return 0;
        }
    fprintf(stderr, "family %s: no fixture (sha256 %s)\n", family, hex);
    return 0;
}

#endif

/* The in-race menu's constants (00447564's), from the exe: the items' places 00447410, the map's offsets 00447428 / 0044745e,
 * the track and marker shape names 00447494 / 004474b8, the point counts 004474d8 / 004474ea and the tracks' map points it reads
 * (x, y words). The points of every map are kept in one pool, in track and variant order; menu_map_first / menu_map_count say
 * where each map is. */
#include "game/exe/exe_tables.h"

#include <stdio.h>
#include <string.h>

enum { ITEMS = 0x447410, X_OFFSETS = 0x447428, Y_OFFSETS = 0x44745E, TRACK_NAMES = 0x447494, MARKER_NAMES = 0x4474B8,
       COUNTS0 = 0x4474D8, COUNTS1 = 0x4474EA, COUNTS0_N = 9, COUNTS1_N = 7 };

/* 00447564's switch: each track's (and for 0..2 each variant's) points, the address the asm reads x from less 2, and its
 * count's place in COUNTS0 or COUNTS1 */
typedef struct { uint32_t at; int table, k; } MapAt;
static const MapAt MAPS[RACE_MENU_TRACKS][RACE_MENU_VARIANTS] = {
    { { 0x4BFB56, 0, 0 }, { 0x4C02D6, 0, 1 }, { 0x4C09B6, 0, 2 } },
    { { 0x4BE576, 0, 3 }, { 0x4BECF6, 0, 4 }, { 0x4BF426, 0, 5 } },
    { { 0x4BCBD6, 0, 6 }, { 0x4BD3F6, 0, 7 }, { 0x4BDCFE, 0, 8 } },
    { { 0x4C124E, 1, 0 } }, { { 0x4C144E, 1, 1 } }, { { 0x4C204E, 1, 5 } }, { { 0x4C184E, 1, 2 } }, { { 0x4C244E, 1, 6 } },
    { { 0x4C1C4E, 1, 3 } },
};
static const int VARIANTS[RACE_MENU_TRACKS] = { 3, 3, 3, 1, 1, 1, 1, 1, 1 };

static int fail(char *err, size_t errsz, const char *what, uint32_t va)
{
    if (err && errsz) snprintf(err, errsz, "menu tables: %s (address %08x)", what, (unsigned)va);
    return -1;
}

static int read_words(const ExeImage *img, uint32_t va, int16_t *out, int n, char *err, size_t errsz)
{
    uint8_t raw[2 * RACE_MENU_OFFSETS];
    if (n * 2 > (int)sizeof raw || exe_va_read(img, va, raw, (size_t)n * 2) != 0) return fail(err, errsz, "words in no section", va);
    for (int i = 0; i < n; i++) out[i] = (int16_t)(raw[2 * i] | raw[2 * i + 1] << 8);
    return 0;
}

static int read_names(const ExeImage *img, uint32_t table, int n, int room, char *out, size_t stride, const char *what,
                      char *err, size_t errsz)
{
    for (int i = 0; i < n; i++) {
        uint8_t p[4];
        char text[EXE_NAME_MAX];
        if (exe_va_read(img, table + 4u * (uint32_t)i, p, 4) != 0) return fail(err, errsz, "a name table is in no section", table);
        uint32_t va = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
        if (exe_va_cstr(img, va, text, sizeof text) != 0) return fail(err, errsz, "a name is in no section", va);
        if ((int)strlen(text) >= room) return fail(err, errsz, what, va);
        memcpy(out + stride * (size_t)i, text, strlen(text) + 1);
    }
    return 0;
}

int exe_tables_menu(const ExeImage *img, ExeTables *t, char *err, size_t errsz)
{
    if (read_words(img, ITEMS, t->menu_item_x, RACE_MENU_ITEMS, err, errsz) != 0) return -1;
    if (read_words(img, ITEMS + 2 * RACE_MENU_ITEMS, t->menu_item_y, RACE_MENU_ITEMS, err, errsz) != 0) return -1;
    if (read_words(img, X_OFFSETS, t->menu_x_offset, RACE_MENU_OFFSETS, err, errsz) != 0) return -1;
    if (read_words(img, Y_OFFSETS, t->menu_y_offset, RACE_MENU_OFFSETS, err, errsz) != 0) return -1;
    if (read_names(img, TRACK_NAMES, RACE_MENU_TRACKS, EXE_NAME_MAX, &t->menu_track_name[0][0], EXE_NAME_MAX, "a track name is too long",
                   err, errsz) != 0)
        return -1;
    if (read_names(img, MARKER_NAMES, RACE_MENU_CARS, 5, &t->menu_marker_name[0][0], 5, "a marker name is too long (room for 4 letters)",
                   err, errsz) != 0)
        return -1;
    int16_t counts[2][COUNTS0_N];
    memset(counts, 0, sizeof counts);
    if (read_words(img, COUNTS0, counts[0], COUNTS0_N, err, errsz) != 0) return -1;
    if (read_words(img, COUNTS1, counts[1], COUNTS1_N, err, errsz) != 0) return -1;
    int used = 0;
    memset(t->menu_map_first, 0, sizeof t->menu_map_first);
    memset(t->menu_map_count, 0, sizeof t->menu_map_count);
    for (int tr = 0; tr < RACE_MENU_TRACKS; tr++)
        for (int v = 0; v < VARIANTS[tr]; v++) {
            const MapAt *m = &MAPS[tr][v];
            int n = counts[m->table][m->k];
            if (n < 0) return fail(err, errsz, "a map's point count is negative", m->at);
            if (n > EXE_MENU_POOL_POINTS - used) return fail(err, errsz, "the maps' points do not fit the pool", m->at);
            for (int i = 0; i < n; i++) {
                uint8_t raw[4];
                if (exe_va_read(img, m->at + 2u + 4u * (uint32_t)i, raw, 4) != 0) return fail(err, errsz, "a map's points are in no section", m->at);
                t->menu_pool[used + i][0] = (int16_t)(raw[0] | raw[1] << 8);
                t->menu_pool[used + i][1] = (int16_t)(raw[2] | raw[3] << 8);
            }
            t->menu_map_first[tr][v] = (int16_t)used;
            t->menu_map_count[tr][v] = (int16_t)n;
            used += n;
        }
    return 0;
}

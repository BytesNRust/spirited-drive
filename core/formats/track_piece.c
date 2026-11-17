/* See track_piece.h. */
#include "formats/track_piece.h"
#include "formats/crc32.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* the CD's track roots begin with these two letters (AL, CL, CY, TR); the race tells a root by the first of them until it reads all of it
 * (track_traits, T1/T2), so a piece may not begin with one */
static const char *const FACTORY_PREFIX[] = { "AL", "CL", "CY", "TR" };

int track_piece_stem_ok(const char *stem)
{
    const size_t n = strlen(stem);
    if (n < 1 || n > TRACK_PIECE_STEM_MAX) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((stem[i] >= 'A' && stem[i] <= 'Z') || (stem[i] >= '0' && stem[i] <= '9'))) return 0;
    for (size_t i = 0; i < sizeof FACTORY_PREFIX / sizeof FACTORY_PREFIX[0]; i++)
        if (strncmp(stem, FACTORY_PREFIX[i], 2) == 0) return 0;
    return 1;
}

/* `value` (a word of a card, "" when not given) among `names` (NULL at the end): its index, `dflt` when not given, -1 otherwise */
static int word_index(const char *value, const char *const names[], int dflt)
{
    if (value[0] == '\0') return dflt;
    for (int i = 0; names[i] != NULL; i++)
        if (strcmp(value, names[i]) == 0) return i;
    return -1;
}

static int times_of(const char *text, int *out)
{
    static const char *const NAMES[] = { "dawn", "day", "dusk", "night" };
    static const int BITS[] = { TRACK_TIME_DAWN, TRACK_TIME_DAY, TRACK_TIME_DUSK, TRACK_TIME_NIGHT };
    int set = 0;
    for (const char *p = text;;) {
        const char *comma = strchr(p, ',');
        const char *end = comma != NULL ? comma : p + strlen(p);
        const char *s = p, *e = end;
        while (s < e && *s == ' ') s++;
        while (e > s && e[-1] == ' ') e--;
        int found = -1;
        for (int i = 0; i < 4; i++)
            if (strlen(NAMES[i]) == (size_t)(e - s) && memcmp(NAMES[i], s, (size_t)(e - s)) == 0) found = i;
        if (found < 0 || (set & BITS[found])) return 0;   /* an unknown name, an empty entry, or one twice */
        set |= BITS[found];
        if (comma == NULL) break;
        p = comma + 1;
    }
    *out = set;
    return 1;
}

int track_piece_spec(const ModInfo *card, TrackPieceSpec *out, Report *r)
{
    static const char *const KINDS[] = { "stage", "circuit", NULL };
    static const char *const ROADS[] = { "asphalt", "dirt", NULL };
    static const char *const EDGES[] = { "grass", "gravel", "sand", "snow", "dirt", "water", NULL };
    const size_t fatal_before = report_fatal_count(r);
    memset(out, 0, sizeof *out);
    out->segments = 1, out->times = TRACK_TIME_DAY;
    if (card->type != MOD_TYPE_TRACK) report_add(r, "PIECE.TYPE", card->type, "mod.ini: type is not track");
    const int kind = word_index(card->kind, KINDS, TRACK_KIND_STAGE);
    if (kind < 0) report_add(r, "PIECE.KIND", 0, "mod.ini: kind=%s is not stage", card->kind);
    else if (kind == TRACK_KIND_CIRCUIT) report_add(r, "PIECE.KIND", 1, "mod.ini: kind=circuit: this version adds A-to-B tracks only");
    else out->kind = (TrackKind)kind;
    if (card->segments[0] != '\0') {
        const int n = card->segments[0] - '0';
        if (card->segments[1] != '\0' || n < 1 || n > TRACK_PIECE_SEGMENTS_MAX)
            report_add(r, "PIECE.SEGMENTS", 0, "mod.ini: segments=%s is not 1, 2 or 3", card->segments);
        else out->segments = n;
    }
    const int road = word_index(card->road, ROADS, TRACK_ROAD_ASPHALT);
    if (road < 0) report_add(r, "PIECE.ROAD", 0, "mod.ini: road=%s is not asphalt or dirt", card->road);
    else out->road = (TrackRoad)road;
    const int edge = word_index(card->edge, EDGES, TRACK_EDGE_GRASS);
    if (edge < 0) report_add(r, "PIECE.EDGE", 0, "mod.ini: edge=%s is not grass, gravel, sand, snow, dirt or water", card->edge);
    else out->edge = (TrackEdge)edge;
    if (card->times[0] != '\0' && !times_of(card->times, &out->times))
        report_add(r, "PIECE.TIMES", 0, "mod.ini: times=%s is not some of dawn, day, dusk and night, each once", card->times);
    return report_fatal_count(r) == fatal_before;
}

void track_piece_paths(const char *stem, int stage, char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH])
{
    snprintf(paths[0], TRACK_PIECE_PATH, "SIMDATA/MISC/%s%d.TRI", stem, stage);
    snprintf(paths[1], TRACK_PIECE_PATH, "SIMDATA/MISC/%s%dINFO", stem, stage);
    snprintf(paths[2], TRACK_PIECE_PATH, "SIMDATA/ETRACKFM/%s%d_001.FAM", stem, stage);
    snprintf(paths[3], TRACK_PIECE_PATH, "SIMDATA/GTRACKFM/%s%d_001.FAM", stem, stage);
}

/* the length of `dir` (with its '/') when `rel` begins with it, any case; 0 otherwise */
static size_t under(const char *rel, const char *dir)
{
    const size_t n = strlen(dir);
    return strncasecmp(rel, dir, n) == 0 ? n : 0;
}

int track_piece_owns(const char *stem, const char *rel)
{
    static const char *const ART[] = { "_T01.FAM", "_M01.FAM", "_R01.FAM" };
    char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH], root[TRACK_PIECE_STEM_MAX + 2];
    if (!track_piece_stem_ok(stem)) return 0;
    for (int stage = 1; stage <= TRACK_PIECE_SEGMENTS_MAX; stage++) {
        track_piece_paths(stem, stage, paths);
        for (int i = 0; i < TRACK_PIECE_FILES; i++)
            if (strcasecmp(rel, paths[i]) == 0) return 1;
        char art[TRACK_PIECE_PATH];
        for (size_t i = 0; i < sizeof ART / sizeof ART[0]; i++) {
            snprintf(art, sizeof art, "SIMDATA/NTRACKFM/%s%d%s", stem, stage, ART[i]);
            if (strcasecmp(rel, art) == 0) return 1;
        }
        snprintf(root, sizeof root, "%s%d", stem, stage);
        const size_t rl = strlen(root);
        size_t at;
        if ((at = under(rel, "track_light/")) != 0 || (at = under(rel, "track/")) != 0) {   /* a text file named by the root */
            const size_t n = strlen(rel);
            if (strncasecmp(rel + at, root, rl) == 0 && strchr(rel + at, '/') == NULL && n > 4 && strcasecmp(rel + n - 4, ".txt") == 0) return 1;
        }
        if ((at = under(rel, "track_masks/")) != 0 && strncasecmp(rel + at, root, rl) == 0) {   /* a folder named by it, one PNG in it */
            const char *slash = strchr(rel + at, '/');
            const size_t n = strlen(rel);
            if (slash != NULL && strchr(slash + 1, '/') == NULL && n > 4 && strcasecmp(rel + n - 4, ".png") == 0) return 1;
        }
    }
    return 0;
}

/* the file of `path` in `files`, as it is, in upper case or in lower case (the overlay's rule, docs/modding.md) */
static const SetFile *find_any_case(const FileSet *files, const char *path)
{
    const SetFile *f = fileset_find(files, path);
    char other[TRACK_PIECE_PATH];
    for (int lower = 0; f == NULL && lower < 2; lower++) {
        size_t i = 0;
        for (; path[i] != '\0' && i + 1 < sizeof other; i++)
            other[i] = (char)(lower ? tolower((unsigned char)path[i]) : toupper((unsigned char)path[i]));
        other[i] = '\0';
        f = fileset_find(files, other);
    }
    return f;
}

int track_piece_check(const ModInfo *card, const FileSet *files, TrackPieceSpec *spec, Report *r)
{
    const size_t fatal_before = report_fatal_count(r);
    TrackPieceSpec local;
    if (spec == NULL) spec = &local;
    track_piece_spec(card, spec, r);
    if (!track_piece_stem_ok(card->stem)) {
        report_add(r, "PIECE.STEM", (int64_t)strlen(card->stem), "mod.ini: stem \"%s\" is not 1..%d of A-Z 0-9, or begins like one of the CD's (AL, CL, CY, TR)",
                   card->stem, TRACK_PIECE_STEM_MAX);
        return 0;   /* no files to look for */
    }
    if (report_fatal_count(r) != fatal_before) return 0;   /* a card that is not good has no stages to look in */
    for (int stage = 1; stage <= spec->segments; stage++) {
        char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH];
        track_piece_paths(card->stem, stage, paths);
        for (int i = 0; i < TRACK_PIECE_FILES; i++) {
            const SetFile *f = find_any_case(files, paths[i]);
            if (f == NULL) report_add(r, "PIECE.FILE_MISSING", 0, "%s is not in the piece", paths[i]);
            else validate_file(f->data, f->size, paths[i], r);
        }
    }
    return report_fatal_count(r) == fatal_before;
}

uint32_t track_piece_fingerprint(const TrackPieceSpec *spec, const char *stem, const FileSet *files)
{
    uint32_t crc = 0;
    for (int stage = 1; stage <= spec->segments; stage++) {
        char paths[TRACK_PIECE_FILES][TRACK_PIECE_PATH];
        track_piece_paths(stem, stage, paths);
        const SetFile *f = find_any_case(files, paths[0]);
        if (f == NULL) return 0;
        crc = crc32_update(crc, f->data, f->size);
    }
    const uint8_t tail[4] = { (uint8_t)spec->kind, (uint8_t)spec->segments, (uint8_t)spec->road, (uint8_t)spec->edge };
    crc = crc32_update(crc, tail, sizeof tail);
    return crc != 0 ? crc : 1;
}

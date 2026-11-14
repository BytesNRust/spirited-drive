/* port: see tournament_variants.h. */
#include "game/frontend/tournament_variants.h"

#include "game/platform/platform.h"
#include "game/sys/port_log.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { LOST_VEGAS = 8, HIDDEN = 9, FIRST_CIRCUIT = 3, TEXT = 96 };

static const char HEADER[] = "tnfs-tournament-variants 1";

static TournamentVariants chosen;

TournamentVariants tournament_variants_get(void) { return chosen; }

void tournament_variants_set(TournamentVariants v)
{
    chosen = (TournamentVariants){ v.mirror != 0, v.inverse != 0, v.rally != 0 };
}

void tournament_variants_reset(void) { chosen = (TournamentVariants){ 0, 0, 0 }; }

int tournament_variants_locked(const int16_t races[TOURNAMENT_VARIANTS_TRACKS])
{
    for (int i = 0; i < TOURNAMENT_VARIANTS_TRACKS; i++)
        if (races[i] != 0) return 1;
    return 0;
}

TournamentVariants tournament_variants_follow(TournamentVariants was, int track, int mirror, int inverse, int rally)
{
    TournamentVariants v = was;
    if (track != LOST_VEGAS && track != HIDDEN) v.mirror = mirror != 0, v.rally = rally != 0;
    if (track >= FIRST_CIRCUIT && track != HIDDEN) v.inverse = inverse != 0;
    return v;
}

TournamentVariants tournament_variants_on_track(TournamentVariants v, int track)
{
    if (track == HIDDEN) return (TournamentVariants){ 0, 0, 0 };
    if (track == LOST_VEGAS) v.mirror = 0, v.rally = 0;
    return v;
}

int tournament_variants_settings(TournamentVariants chosen, int track, int restomod, int unlock_m, int unlock_r, int level,
                                 TournamentVariants *out)
{
    const TournamentVariants v = tournament_variants_on_track(chosen, track);
    if (!restomod) {
        if (!v.mirror && !v.inverse && !v.rally) return 0;
        *out = (TournamentVariants){ 0, 0, 0 };
        return 1;
    }
    *out = (TournamentVariants){ v.mirror && unlock_m, v.inverse && level >= 2, v.rally && unlock_r };
    return 1;
}

int tournament_variants_path(const char *save, char *out, size_t size)
{
    const size_t n = strlen(save);
    if (n < 4 || save[n - 4] != '.' || toupper((unsigned char)save[n - 3]) != 'S' || toupper((unsigned char)save[n - 2]) != 'A' ||
        toupper((unsigned char)save[n - 1]) != 'V' || n + 1 > size)
        return 0;
    memcpy(out, save, n - 3);
    memcpy(out + n - 3, "VAR", 4);
    return 1;
}

int tournament_variants_format(TournamentVariants v, char *out, size_t size)
{
    return snprintf(out, size, "%s\nmirror=%d\ninverse=%d\nrally=%d\n", HEADER, v.mirror != 0, v.inverse != 0, v.rally != 0);
}

/* "key=0" or "key=1" at the start of `line`: 1 and the value when it is */
static int key_value(const char *line, const char *key, int *value)
{
    const size_t n = strlen(key);
    if (strncmp(line, key, n) != 0 || line[n] != '=' || (line[n + 1] != '0' && line[n + 1] != '1')) return 0;
    if (line[n + 2] != '\0' && line[n + 2] != '\r') return 0;
    *value = line[n + 1] == '1';
    return 1;
}

int tournament_variants_parse(const uint8_t *text, size_t size, TournamentVariants *out)
{
    char copy[TEXT];
    if (text == NULL || size == 0 || size >= sizeof copy) return 0;
    memcpy(copy, text, size);
    copy[size] = '\0';
    if (strlen(copy) != size) return 0;   /* a NUL inside: not the port's text */
    int seen = 0, values[3] = { 0, 0, 0 }, line_no = 0;
    static const char *const KEYS[3] = { "mirror", "inverse", "rally" };
    for (char *line = copy, *next; line != NULL && *line != '\0'; line = next, line_no++) {
        next = strchr(line, '\n');
        if (next != NULL) *next++ = '\0';
        const size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
        if (line_no == 0) {
            if (strcmp(line, HEADER) != 0) return 0;
            continue;
        }
        int matched = 0;
        for (int k = 0; k < 3; k++)
            if (key_value(line, KEYS[k], &values[k])) seen |= 1 << k, matched = 1;
        if (!matched && line[0] != '\0') return 0;
    }
    if (seen != 7) return 0;
    *out = (TournamentVariants){ values[0], values[1], values[2] };
    return 1;
}

int tournament_variants_save(const char *save)
{
    char path[TOURNAMENT_VARIANTS_PATH], text[TEXT];
    if (!tournament_variants_path(save, path, sizeof path)) {
        port_log("tournament_variants: no .SAV in %s, variants not saved\n", save);
        return 0;
    }
    const int n = tournament_variants_format(chosen, text, sizeof text);
    if (!platform_write_user_file(path, (const uint8_t *)text, (size_t)n)) {
        port_log("tournament_variants: %s could not be written\n", path);
        return 0;
    }
    return 1;
}

void tournament_variants_load(const char *save)
{
    char path[TOURNAMENT_VARIANTS_PATH];
    tournament_variants_reset();   /* nothing is kept from the tournament in memory before */
    if (!tournament_variants_path(save, path, sizeof path)) {
        port_log("tournament_variants: no .SAV in %s, base variants\n", save);
        return;
    }
    size_t size = 0;
    uint8_t *text = platform_read_user_file(path, &size);
    if (text == NULL) {
        port_log("tournament_variants: %s missing, base variants\n", path);
        return;
    }
    TournamentVariants v;
    if (tournament_variants_parse(text, size, &v)) tournament_variants_set(v);
    else port_log("tournament_variants: %s damaged, base variants\n", path);
    platform_free_file(text);
}

void tournament_variants_remove(const char *save)
{
    char path[TOURNAMENT_VARIANTS_PATH];
    if (!tournament_variants_path(save, path, sizeof path)) return;
    size_t size = 0;
    uint8_t *text = platform_read_user_file(path, &size);   /* a save from before has none: nothing to remove, nothing to log */
    if (text == NULL) return;
    platform_free_file(text);
    if (!platform_remove_user_file(path)) port_log("tournament_variants: cannot remove %s\n", path);
}

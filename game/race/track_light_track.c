/* port: see track_light_track.h. */
#include "game/race/track_light_track.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LINE = 512, ALL_POINTS = (1 << TRACK_NODE_LIGHT_POINTS) - 1 };

/* a whole unsigned number token: 1 and its value, 0 when the word is anything else */
static int number(const char *word, unsigned *value)
{
    char *end = NULL;
    if (!isdigit((unsigned char)word[0])) return 0;
    const unsigned long v = strtoul(word, &end, 10);
    if (*end != '\0' || v > 0xffffffu) return 0;
    *value = (unsigned)v;
    return 1;
}

/* the next word of `*at` into `word`; 0 at the end of the line */
static int next_word(const char **at, char *word, size_t cap)
{
    *at += strspn(*at, " \t\r\n");
    const size_t n = strcspn(*at, " \t\r\n");
    if (n == 0 || n >= cap) return 0;
    memcpy(word, *at, n);
    word[n] = '\0';
    *at += n;
    return 1;
}

/* a point word `pN` (0..10) */
static int point(const char *word, unsigned *p)
{
    return word[0] == 'p' && number(word + 1, p) && *p < TRACK_NODE_LIGHT_POINTS;
}

/* STRIP at `*at`, if there is one: its points in `mask` (every point when there is none). 0 for a malformed strip (`pA pB` with B before A). */
static int strip(const char **at, unsigned *mask)
{
    const char *save = *at;
    char word[32];
    unsigned a, b;
    *mask = ALL_POINTS;
    if (!next_word(at, word, sizeof word)) return *at = save, 1;
    if (strcmp(word, "all") == 0) return 1;
    if (strcmp(word, "left") == 0) return *mask = 0x7c0u, 1;
    if (strcmp(word, "right") == 0) return *mask = 0x03eu, 1;
    if (strcmp(word, "road") == 0) return *mask = (1u << 0) | (1u << 1) | (1u << 6), 1;
    if (!point(word, &a)) return *at = save, 1;   /* not a strip: what follows is the next part of the line */
    const char *after = *at;
    if (next_word(at, word, sizeof word) && point(word, &b)) {
        if (b < a) return 0;
    } else {
        b = a, *at = after;
    }
    *mask = 0;
    for (unsigned p = a; p <= b; p++) *mask |= 1u << p;
    return 1;
}

/* the colour word of a look: 6 hex digits (then `:strength` perhaps), or r/g/b. A word of 6 digits is a colour, never a node (a track has under 100000 nodes). */
static int colour_word(const char *word)
{
    return strchr(word, '/') != NULL || (strspn(word, "0123456789abcdefABCDEF") == 6 && (word[6] == '\0' || word[6] == ':'));
}

typedef struct {
    uint32_t nodes;
    uint8_t *fact;                  /* per node: 1 a tunnel, 2 a hall (a `tunnel` fact) */
    int has_time[4];
    TrackLightTweaks *scratch;      /* the tweaks of a block that is not the one asked for: read to be checked, then dropped */
} State;

static int time_index(char c) { return c == 'A' ? 0 : c == 'B' ? 1 : c == 'C' ? 2 : c == 'N' ? 3 : -1; }

/* `tunnel_look [A B] [LIGHT] [COLOUR] [lamps S [COLOUR]] [reach R]` as lines of the old grammar (tunnel_tone, or tunnel with its nodes, and tunnel_lamp): 1 when well-formed */
static int tunnel_look(const char *rest, const State *st, TrackLightTweaks *tw)
{
    char w[10][48], line[MAX_LINE];
    int count = 0;
    const char *at = rest;
    while (count < 10 && next_word(&at, w[count], sizeof w[count])) count++;
    if (next_word(&at, line, sizeof line)) return 0;
    int i = 0, ranged = 0;
    unsigned a = 0, b = 0;
    if (count >= 2 && !colour_word(w[0]) && !colour_word(w[1]) && number(w[0], &a) && number(w[1], &b)) {   /* two whole numbers first are the nodes */
        if (a > b || b >= st->nodes) return 0;
        for (unsigned n = a; n <= b; n++)
            if (st->fact[n] == 0) return 0;   /* a time block cannot make a tunnel */
        ranged = 1, i = 2;
    }
    const char *light = "", *colour = "", *strength = "", *lamp_colour = "", *reach = "";
    if (i < count && !colour_word(w[i]) && isdigit((unsigned char)w[i][0])) light = w[i++];
    if (i < count && colour_word(w[i])) colour = w[i++];
    while (i < count) {
        if (strcmp(w[i], "lamps") == 0 && strength[0] == '\0' && i + 1 < count) {
            strength = w[i + 1], i += 2;
            if (i < count && colour_word(w[i])) lamp_colour = w[i++];
        } else if (strcmp(w[i], "reach") == 0 && reach[0] == '\0' && i + 1 < count) {
            reach = w[i + 1], i += 2;
        } else {
            return 0;
        }
    }
    if (light[0] == '\0' && colour[0] == '\0' && strength[0] == '\0') return 0;   /* a look that says nothing (a reach alone goes with lamps) */
    if (colour[0] != '\0' && light[0] == '\0' && !ranged) return 0;   /* the default look's colour comes with its light, as `tunnel_tone` has it */
    if (light[0] != '\0' || colour[0] != '\0') {
        if (ranged) snprintf(line, sizeof line, "tunnel %u %u %s %s", a, b, light, colour);
        else snprintf(line, sizeof line, "tunnel_tone %s %s", light, colour);
        if (!track_light_tweak_line(line, st->nodes, tw)) return 0;
        if (ranged) tw->zones[tw->zone_count - 1].line = 0;   /* the caller gives it its line */
    }
    if (strength[0] != '\0') {
        if (ranged) snprintf(line, sizeof line, "tunnel_lamp %u %u %s %s %s", a, b, strength, lamp_colour, reach);
        else snprintf(line, sizeof line, "tunnel_lamp %s %s %s", strength, lamp_colour, reach);
        if (!track_light_tweak_line(line, st->nodes, tw)) return 0;
    }
    return 1;
}

/* `brightness A B PERCENT [STRIP]` */
static int brightness(const char *rest, uint32_t nodes, TrackLightTweaks *tw)
{
    unsigned a, b, c, mask;
    int n = 0;
    if (sscanf(rest, "%u %u %u%n", &a, &b, &c, &n) != 3 || a > b || b >= nodes || c > 1000 || tw->brightness_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
    if (rest[n] != '\0' && !isspace((unsigned char)rest[n])) return 0;
    const char *at = rest + n;
    if (!strip(&at, &mask)) return 0;
    at += strspn(at, " \t\r\n");
    if (*at != '\0') return 0;
    tw->brightness[tw->brightness_count++] = (TrackLightBrightness){ .from = a, .to = b, .percent = (int)c, .mask = mask == ALL_POINTS ? 0 : (uint16_t)mask };
    return 1;
}

/* `tone A B [STRIP] RRGGBB:PERCENT` as the old line: the strip's points one by one */
static int tone(const char *rest, uint32_t nodes, TrackLightTweaks *tw)
{
    unsigned a, b, mask;
    int n = 0;
    if (sscanf(rest, "%u %u%n", &a, &b, &n) != 2) return 0;
    const char *at = rest + n;
    if (!strip(&at, &mask)) return 0;
    char line[MAX_LINE], points[96] = "";
    if (mask != ALL_POINTS)
        for (unsigned p = 0; p < TRACK_NODE_LIGHT_POINTS; p++)
            if ((mask >> p) & 1) snprintf(points + strlen(points), sizeof points - strlen(points), " p%u", p);
    snprintf(line, sizeof line, "tone %u %u%s %s", a, b, points, at + strspn(at, " \t"));
    return track_light_tweak_line(line, nodes, tw);
}

/* `rows A B v0..v10` into the file's rows */
static int rows(const char *rest, uint32_t nodes, TrackLightFile *out, int active)
{
    unsigned a, b, v[TRACK_NODE_LIGHT_POINTS];
    int n = 0;
    if (sscanf(rest, "%u %u%n", &a, &b, &n) != 2 || a > b || b >= nodes) return 0;
    rest += n;
    for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) {
        if (sscanf(rest, "%u%n", &v[p], &n) != 1 || v[p] > 255) return 0;
        rest += n;
    }
    if (rest[strspn(rest, " \t\r\n")] != '\0') return 0;
    if (!active) return 1;
    for (unsigned node = a; node <= b; node++) {
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) out->light.data[(size_t)node * TRACK_NODE_LIGHT_POINTS + (size_t)p] = (uint8_t)v[p];
        out->row_set[node] = 1;
    }
    return 1;
}

/* `sun AZ EL SHADOW`, each part a number or `-` */
static int sun(const char *rest, TrackLightSun *sun_out)
{
    char w[3][32], fourth[32], extra[32];
    const char *at = rest;
    double v[3] = { 0, 0, 0 };
    int has[3];
    for (int i = 0; i < 3; i++) {
        if (!next_word(&at, w[i], sizeof w[i])) return 0;
        has[i] = strcmp(w[i], "-") != 0;
        if (has[i]) {
            char *end = NULL;
            v[i] = strtod(w[i], &end);
            if (end == w[i] || *end != '\0') return 0;
        }
    }
    int has_reach = 0;
    double reach = 0.0;
    if (next_word(&at, fourth, sizeof fourth)) {   /* an optional fourth word: the reach in units, `-` the generator's */
        has_reach = strcmp(fourth, "-") != 0;
        char *end = NULL;
        if (has_reach) reach = strtod(fourth, &end);
        if (has_reach && (end == fourth || *end != '\0' || reach < 1.0 || reach > 2000.0)) return 0;
    }
    if (next_word(&at, extra, sizeof extra) || (has[2] && (v[2] < 0.0 || v[2] > 100.0)) || (has[1] && (v[1] < -90.0 || v[1] > 90.0))) return 0;
    *sun_out = (TrackLightSun){ 1, has[0], has[1], has[2], v[0], v[1], v[2], has_reach, reach };
    return 1;
}

int track_light_track_parse(const char *text, size_t size, const char *track, char letter, uint32_t nodes, TrackLightFile *out, int *bad_line)
{
    if (bad_line != NULL) *bad_line = 0;
    const int want = time_index(letter);
    if (text == NULL || track == NULL || out == NULL || nodes == 0 || want < 0) return 0;
    TrackLightFile file = { 0 };
    State st = { .nodes = nodes };
    TrackNodeLight places = { 0 };
    TrackLightTweaks *scratch = calloc(1, sizeof *scratch);
    st.fact = calloc(nodes, 1), file.row_set = calloc(nodes, 1), file.light.data = calloc((size_t)nodes * TRACK_NODE_LIGHT_POINTS, 1);
    file.light.nodes = nodes;
    places.nodes = nodes;
    places.tunnel = calloc(nodes, 1);
    int line_no = 0, header = 0, in_block = 0, active = 0, blocks_seen = 0;
    TrackLightTweaks *cur = &file.tweaks;
    int halls[TRACK_LIGHT_MAX_BRIGHT][2], hall_count = 0, tunnel_facts = 0, ok = 0;
    if (scratch == NULL || st.fact == NULL || file.row_set == NULL || file.light.data == NULL || places.tunnel == NULL) goto done;
    st.scratch = scratch;
    for (size_t at = 0; at < size;) {
        char line[MAX_LINE];
        size_t n = 0;
        while (at < size && text[at] != '\n') {
            if (n + 1 < sizeof line) line[n++] = text[at];
            at++;
        }
        at++;
        line[n] = '\0';
        line_no++;
        char *hash = strchr(line, '#');
        if (hash != NULL) *hash = '\0';
        char key[32];
        const char *rest = line;
        if (!next_word(&rest, key, sizeof key)) continue;
        if (!header) {
            char name[32], extra[32];
            if (strcmp(key, "track") != 0 || !next_word(&rest, name, sizeof name) || next_word(&rest, extra, sizeof extra)) goto bad;
            if (strcmp(name, track) != 0) goto done;   /* another track's file: no line is bad */
            header = 1;
            continue;
        }
        if (strcmp(key, "time") == 0) {
            char name[32], extra[32];
            if (!next_word(&rest, name, sizeof name) || next_word(&rest, extra, sizeof extra) || name[1] != '\0' || time_index(name[0]) < 0 || st.has_time[time_index(name[0])]) goto bad;
            st.has_time[time_index(name[0])] = 1;
            in_block = 1, blocks_seen++;
            active = time_index(name[0]) == want;
            if (!active) memset(scratch, 0, sizeof *scratch);
            cur = active ? &file.tweaks : scratch;
            for (int h = 0; h < hall_count; h++) {   /* the halls are a fact of the track, in every time's zones */
                char hall[96];
                snprintf(hall, sizeof hall, "tunnel %d %d lit", halls[h][0], halls[h][1]);
                if (!track_light_tweak_line(hall, nodes, cur)) goto bad;
            }
            continue;
        }
        if (!in_block) {   /* the facts */
            unsigned a, b;
            if (strcmp(key, "tunnel") == 0) {
                char w[32], lit[32];
                if (!next_word(&rest, w, sizeof w) || !number(w, &a) || !next_word(&rest, w, sizeof w) || !number(w, &b) || a > b || b >= nodes) goto bad;
                int is_hall = 0;
                if (next_word(&rest, lit, sizeof lit)) {
                    if (strcmp(lit, "lit") != 0 || next_word(&rest, lit, sizeof lit)) goto bad;
                    is_hall = 1;
                }
                if (is_hall && hall_count >= TRACK_LIGHT_MAX_BRIGHT) goto bad;
                for (unsigned node = a; node <= b; node++) st.fact[node] = is_hall ? 2 : 1;
                if (is_hall) halls[hall_count][0] = (int)a, halls[hall_count][1] = (int)b, hall_count++;
                else memset(places.tunnel + a, 1, b - a + 1), tunnel_facts = 1;
            } else if (strcmp(key, "snow") == 0) {
                int on;
                if (sscanf(rest, "%d", &on) != 1 || (on != 0 && on != 1)) goto bad;
                places.snow = on;
            } else if (strcmp(key, "snow_map") == 0) {
                unsigned from, to;
                int used = 0;
                if (sscanf(rest, "%u %u%n", &from, &to, &used) != 2 || from >= TRACK_NODE_LIGHT_SNOW_SLOTS || to >= TRACK_NODE_LIGHT_SNOW_SLOTS || rest[used + strspn(rest + used, " \t\r")] != '\0') goto bad;
                places.snow_map[from] = (uint8_t)(to + 1);
            } else {
                goto bad;
            }
            continue;
        }
        int good = 0;   /* a line of a time's block */
        if (strcmp(key, "sun") == 0) {
            TrackLightSun s;
            good = sun(rest, &s);
            if (good && active) file.sun = s;
        } else if (strcmp(key, "tunnel_look") == 0) {
            good = tunnel_look(rest, &st, cur);
            if (good && cur->zone_count > 0 && cur->zones[cur->zone_count - 1].line == 0) cur->zones[cur->zone_count - 1].line = line_no;
        } else if (strcmp(key, "lamp") == 0) {
            good = track_light_tweak_line(line, nodes, cur);
        } else if (strcmp(key, "brightness") == 0) {
            good = brightness(rest, nodes, cur);
        } else if (strcmp(key, "tone") == 0) {
            good = tone(rest, nodes, cur);
        } else if (strcmp(key, "rows") == 0) {
            good = rows(rest, nodes, &file, active);
        }
        if (!good) goto bad;
        if (!active) continue;
    }
    (void)blocks_seen;
    if (!header) { line_no = 0; goto bad; }
    if (!track_light_tweaks_finish(&file.tweaks, &line_no)) goto bad;
    if (tunnel_facts) file.light.tunnel = places.tunnel, places.tunnel = NULL;   /* a track with no tunnel has none: the rows decide, as with no places file's `present tunnel` */
    file.light.snow = places.snow;
    memcpy(file.light.snow_map, places.snow_map, sizeof file.light.snow_map);
    file.light.from_places = 1;
    file.base = 1, file.flags = 0;
    *out = file;
    memset(&file, 0, sizeof file);
    ok = 1;
    goto done;
bad:
    if (bad_line != NULL) *bad_line = line_no;
done:
    free(places.tunnel);
    free(st.fact);
    free(scratch);
    track_light_file_free(&file);
    return ok;
}

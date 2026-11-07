/* port: see track_light_file.h. */
#include "game/race/track_light_file.h"


#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LINE = 512 };

typedef struct {
    char *text;
    size_t size, capacity;
    int failed;
} Buffer;

static void put(Buffer *b, const char *format, ...)
{
    va_list args;
    char line[MAX_LINE];
    va_start(args, format);
    const int n = vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof line || b->failed) { b->failed = 1; return; }
    if (b->size + (size_t)n + 1 > b->capacity) {
        const size_t capacity = (b->capacity != 0 ? b->capacity * 2 : 4096) + (size_t)n;
        char *grown = realloc(b->text, capacity);
        if (grown == NULL) { b->failed = 1; return; }
        b->text = grown, b->capacity = capacity;
    }
    memcpy(b->text + b->size, line, (size_t)n + 1);
    b->size += (size_t)n;
}

/* the runs of 1s of a per-node flag array, one `key from to` line each */
static void put_ranges(Buffer *b, const char *key, const uint8_t *flag, uint32_t nodes)
{
    if (flag == NULL) return;
    for (uint32_t n = 0; n < nodes;) {
        if (!flag[n]) { n++; continue; }
        uint32_t end = n;
        while (end + 1 < nodes && flag[end + 1]) end++;
        put(b, "%s %u %u\n", key, n, end);
        n = end + 1;
    }
}

/* the runs of equal non-zero classes, one `key from to class` line each */
static void put_classes(Buffer *b, const char *key, const uint8_t *cls, uint32_t nodes)
{
    if (cls == NULL) return;
    for (uint32_t n = 0; n < nodes;) {
        if (cls[n] == 0) { n++; continue; }
        uint32_t end = n;
        while (end + 1 < nodes && cls[end + 1] == cls[n]) end++;
        put(b, "%s %u %u %u\n", key, n, end, cls[n]);
        n = end + 1;
    }
}

/* a colour as the file writes it: RRGGBB when the bytes give the same factors exactly, else r/g/b, the factors written out (the export's, for the made looks whose factors 8 bits cannot hold) */
static void colour_text(const float cast[3], char out[64])
{
    int exact = 1;
    unsigned v8[3];
    for (int k = 0; k < 3; k++) {
        const double c = cast[k] < 0.0f ? 0.0 : cast[k] > 1.0f ? 1.0 : (double)cast[k];
        v8[k] = (unsigned)(c * 255.0 + 0.5);
        if ((float)((double)v8[k] / 255.0) != cast[k]) exact = 0;
    }
    if (exact) snprintf(out, 64, "%02x%02x%02x", v8[0], v8[1], v8[2]);
    else {   /* each factor with the fewest digits that read back as the very same float */
        char part[3][16];
        for (int k = 0; k < 3; k++)
            for (int digits = 1; digits <= 9; digits++) {
                snprintf(part[k], sizeof part[k], "%.*g", digits, (double)cast[k]);
                if ((float)strtod(part[k], NULL) == cast[k]) break;
            }
        snprintf(out, 64, "%s/%s/%s", part[0], part[1], part[2]);
    }
}

int track_light_place_format(const PsxLight *light, const char *track, char **text, size_t *size)
{
    if (light == NULL || track == NULL || light->nodes == 0 || text == NULL || size == NULL) return 0;
    Buffer b = { 0 };
    put(&b, "# tnfs track places 1\n");
    put(&b, "track %s\n", track);
    if (light->tunnel != NULL || light->hall != NULL) put(&b, "present tunnel\n");   /* an array that is there but empty is not an array that is not (the rows decide a tunnel without one) */
    if (light->canyon != NULL) put(&b, "present canyon\n");
    if (light->car_dim != NULL) put(&b, "present car_dim\n");
    uint8_t *both = calloc(light->nodes, 1);   /* the road's tunnels and the halls: a hall is a tunnel here, and its time's file says it is lit */
    if (both == NULL) return 0;
    for (uint32_t n = 0; n < light->nodes; n++) both[n] = (light->tunnel != NULL && light->tunnel[n]) || (light->hall != NULL && light->hall[n]);
    put_ranges(&b, "tunnel", both, light->nodes);
    free(both);
    put_ranges(&b, "canyon", light->canyon, light->nodes);
    put_ranges(&b, "car_dim", light->car_dim, light->nodes);
    if (b.failed) {
        free(b.text);
        return 0;
    }
    *text = b.text, *size = b.size;
    return 1;
}

int track_light_format(const PsxLight *light, const char *track, char letter, uint32_t flags, char **text, size_t *size)
{
    if (light == NULL || track == NULL || light->data == NULL || light->nodes == 0 || text == NULL || size == NULL) return 0;
    Buffer b = { 0 };
    put(&b, "# tnfs track light 1\n");
    put(&b, "track %s time %c nodes %u flags %x\n", track, letter, light->nodes, flags);
    for (uint32_t n = 0; n < light->nodes;) {
        const uint8_t *row = &light->data[(size_t)n * PSX_LIGHT_POINTS];
        uint32_t end = n;
        while (end + 1 < light->nodes && memcmp(row, &light->data[(size_t)(end + 1) * PSX_LIGHT_POINTS], PSX_LIGHT_POINTS) == 0) end++;
        put(&b, "rows %u %u", n, end);
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) put(&b, " %u", row[p]);
        put(&b, "\n");
        n = end + 1;
    }
    /* the places (tunnels, canyon, car_dim) are the track's file (track_light_place_format); this file holds the time's light: the pools' arrays are there but empty is not an array that is not */
    if (light->pool_class != NULL) put(&b, "present pool\n");
    if (light->car_pool != NULL) put(&b, "present car_pool\n");
    for (uint32_t n = 0; light->hall != NULL && n < light->nodes;) {   /* a hall is a tunnel with a lot of light: lit by its own lamps */
        if (!light->hall[n]) { n++; continue; }
        uint32_t end = n;
        while (end + 1 < light->nodes && light->hall[end + 1]) end++;
        put(&b, "tunnel %u %u lit\n", n, end);
        n = end + 1;
    }
    put_classes(&b, "pool", light->pool_class, light->nodes);
    put_classes(&b, "car_pool", light->car_pool, light->nodes);
    if (light->car_disc)
        put(&b, "car_disc 1 %a %a %a %a %a %a\n", (double)light->car_even[0], (double)light->car_even[1], (double)light->car_even[2], (double)light->car_odd[0], (double)light->car_odd[1], (double)light->car_odd[2]);
    if (light->look_set) {   /* the tunnels' look: its light as a percent of full light at this time, and its colour */
        char colour[64];
        colour_text(light->look_cast, colour);
        put(&b, "tunnel_tone %.12g %s\n", light->look_light, colour);
    }
    if (b.failed) {
        free(b.text);
        return 0;
    }
    *text = b.text, *size = b.size;
    return 1;
}

/* The words a file may use are English; the first version of the format was Spanish and those files (the owner's edited ones included) still load: each Spanish word
 * is the English one (tools/track_light_merge.py writes the files out in English). A word that is neither is returned as it is. */
static const char *english(const char *word)
{
    static const char *const WORDS[][2] = {
        { "pista", "track" }, { "hora", "time" }, { "nodos", "nodes" }, { "banderas", "flags" }, { "filas", "rows" }, { "presente", "present" },
        { "tunel", "tunnel" }, { "coche_tenue", "car_dim" }, { "canon", "canyon" }, { "pozo", "pool" }, { "pozo_coche", "car_pool" },
        { "coche_disco", "car_disc" }, { "tono_tunel", "tunnel_tone" }, { "brillo", "brightness" }, { "tono_coche", "car_tone" },
    };
    for (size_t i = 0; i < sizeof WORDS / sizeof WORDS[0]; i++)
        if (strcmp(word, WORDS[i][0]) == 0) return WORDS[i][1];
    return word;
}

static uint8_t *flag_array(uint8_t **slot, uint32_t nodes)
{
    if (*slot == NULL) *slot = calloc(nodes, 1);
    return *slot;
}

/* a colour: RRGGBB (6 hex digits), or r/g/b, the three factors written out (the export's, where 8 bits do not hold a made look's exactly), then an optional :strength (0..100, 100 whole). The
 * factor on each channel is the colour (0..1) at full strength and 1 (no change) at none, as a `tone` is. */
static int parse_colour(const char *token, float cast[3])
{
    const size_t colon = strcspn(token, ":");
    double strength = 100.0;
    if (token[colon] == ':') {
        char *end = NULL;
        strength = strtod(token + colon + 1, &end);
        if (end == token + colon + 1 || *end != '\0' || strength < 0.0 || strength > 100.0) return 0;
    }
    double full[3];
    if (memchr(token, '/', colon) != NULL) {   /* r/g/b: the three factors (0..1) as they are, for the made looks 8 bits cannot hold exactly */
        char part[48];
        if (colon >= sizeof part) return 0;
        memcpy(part, token, colon), part[colon] = '\0';
        char *at = part, *end = NULL;
        for (int k = 0; k < 3; k++) {
            full[k] = strtod(at, &end);
            if (end == at || full[k] < 0.0 || full[k] > 1.0 || (k < 2 ? *end != '/' : *end != '\0')) return 0;
            at = end + 1;
        }
    } else {
        if (colon != 6) return 0;
        for (size_t i = 0; i < colon; i++)
            if (!isxdigit((unsigned char)token[i])) return 0;
        for (int k = 0; k < 3; k++) {
            char part[3] = { token[2 * k], token[2 * k + 1], '\0' };
            full[k] = (double)strtoul(part, NULL, 16) / 255.0;
        }
    }
    for (int k = 0; k < 3; k++) cast[k] = (strength == 100.0 ? (float)full[k] : (float)(1.0 + (full[k] - 1.0) * strength / 100.0));
    return 1;
}

/* the rest of a tunnel line, `[light] [colour | lit]` (a zone may say `lit`; tunnel_tone may not): the light is a number (percent of full light, 100 is row 255), a token of 6 hex digits or r/g/b is
 * a colour. 1 when well-formed. */
static int parse_look(const char *rest, int zone, int *lit, int *has_light, double *light, int *has_colour, float cast[3])
{
    char token[48];
    int used = 0;
    *lit = *has_light = *has_colour = 0;
    while (sscanf(rest, "%47s%n", token, &used) == 1) {
        rest += used;
        if (zone && strcmp(token, "lit") == 0) {
            if (*lit) return 0;
            *lit = 1;
        } else if (parse_colour(token, cast) && !*has_colour) {
            *has_colour = 1;
        } else {
            char *end = NULL;
            const double v = strtod(token, &end);
            if (*has_light || *has_colour || end == token || *end != '\0' || v < 0.0 || v > 10000.0) return 0;
            *light = v, *has_light = 1;
        }
    }
    return !*lit || (!*has_light && !*has_colour);   /* a hall is lit by its own lamps: no light or colour of its own */
}

/* a look a line gave goes into the time's one look; a second that differs is refused */
static int take_look(TrackLightTweaks *tweaks, int has_light, double light, int has_colour, const float cast[3])
{
    if (has_light) {
        if (tweaks->look_has_light && tweaks->look_light != light) return 0;
        tweaks->look_has_light = 1, tweaks->look_light = light;
    }
    if (has_colour) {
        if (tweaks->look_has_colour && memcmp(tweaks->look_cast, cast, sizeof tweaks->look_cast) != 0) return 0;
        tweaks->look_has_colour = 1, memcpy(tweaks->look_cast, cast, sizeof tweaks->look_cast);
    }
    return 1;
}

/* one line of the file into `light`; 0 when it is not well-formed. `seen` counts the node rows given. */
static int parse_line(const char *line, PsxLight *light, TrackLightTweaks *tweaks, uint32_t *seen)
{
    char word[32];
    int used = 0;
    if (sscanf(line, "%31s%n", word, &used) != 1) return 1;   /* blank */
    const char *const key = english(word);
    const char *rest = line + used;
    const uint32_t nodes = light->nodes;
    unsigned a, b, c;
    if (strcmp(key, "present") == 0) {
        char spoken[32];
        if (sscanf(rest, "%31s", spoken) != 1) return 0;
        const char *const what = english(spoken);
        uint8_t **slot = strcmp(what, "pool") == 0 ? &light->pool_class : strcmp(what, "car_pool") == 0 ? &light->car_pool : NULL;   /* the places' arrays are the track file's (track_light_place_parse) */
        return slot != NULL && flag_array(slot, nodes) != NULL;
    }
    if (strcmp(key, "rows") == 0) {
        unsigned v[PSX_LIGHT_POINTS];
        int n = 0;
        if (sscanf(rest, "%u %u%n", &a, &b, &n) != 2) return 0;
        rest += n;
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
            if (sscanf(rest, "%u%n", &v[p], &n) != 1 || v[p] > 255) return 0;
            rest += n;
        }
        if (a > b || b >= nodes) return 0;
        for (unsigned node = a; node <= b; node++)
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) light->data[(size_t)node * PSX_LIGHT_POINTS + (size_t)p] = (uint8_t)v[p];
        *seen += b - a + 1;
        return 1;
    }
    if (strcmp(key, "tunnel") == 0) {   /* a zone: nodes a..b are a tunnel (added to the track file's), with this time's light and colour, or a hall when `lit` */
        int n = 0;
        if (sscanf(rest, "%u %u%n", &a, &b, &n) != 2 || a > b || b >= nodes || tweaks->zone_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        int lit = 0, has_light = 0, has_colour = 0;
        double value = 0.0;
        float cast[3];
        if (!parse_look(rest + n, 1, &lit, &has_light, &value, &has_colour, cast) || !take_look(tweaks, has_light, value, has_colour, cast)) return 0;
        tweaks->zones[tweaks->zone_count++] = (TrackLightZone){ a, b, lit };
        return 1;
    }
    uint8_t **class_slot = strcmp(key, "pool") == 0 ? &light->pool_class : strcmp(key, "car_pool") == 0 ? &light->car_pool : NULL;
    if (class_slot != NULL) {
        if (sscanf(rest, "%u %u %u", &a, &b, &c) != 3 || a > b || b >= nodes || c < 1 || c > 4) return 0;
        uint8_t *cls = flag_array(class_slot, nodes);
        if (cls == NULL) return 0;
        memset(cls + a, (int)c, b - a + 1);
        return 1;
    }
    if (strcmp(key, "car_disc") == 0) {
        double v[6];
        int flag = 0, n = 0;
        if (sscanf(rest, "%d%n", &flag, &n) != 1) return 0;
        rest += n;
        for (int i = 0; i < 6; i++) {
            char *end = NULL;
            v[i] = strtod(rest, &end);
            if (end == rest) return 0;
            rest = end;
        }
        light->car_disc = flag != 0;
        for (int i = 0; i < 3; i++) light->car_even[i] = (float)v[i], light->car_odd[i] = (float)v[3 + i];
        return 1;
    }
    if (strcmp(key, "brightness") == 0) {
        if (sscanf(rest, "%u %u %u", &a, &b, &c) != 3 || a > b || b >= nodes || c > 1000 || tweaks->brightness_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        tweaks->brightness[tweaks->brightness_count++] = (TrackLightBrightness){ a, b, (int)c };
        return 1;
    }
    if (strcmp(key, "tone") == 0) {
        unsigned from, to, rgb = 0, mask = 0;
        int n = 0, percent = 0;
        if (sscanf(rest, "%u %u%n", &from, &to, &n) != 2 || from > to || to >= nodes) return 0;
        rest += n;
        for (;;) {   /* the side strip's points, p0..p10, before the colour */
            unsigned point;
            int used = 0;
            if (sscanf(rest, " p%u%n", &point, &used) != 1) break;
            if (point >= PSX_LIGHT_POINTS) return 0;
            mask |= 1u << point, rest += used;
        }
        if (sscanf(rest, " %x:%d", &rgb, &percent) != 2 || rgb > 0xffffffu || percent < 0 || percent > 100 || tweaks->tone_run_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        if (mask == 0) mask = (1u << PSX_LIGHT_POINTS) - 1u;
        const PsxLightTone tone = { { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb }, (uint8_t)percent };
        int id = -1;
        for (int i = 0; i < tweaks->tone_count; i++)
            if (memcmp(&tweaks->tones[i], &tone, sizeof tone) == 0) id = i;
        if (id < 0) {
            if (tweaks->tone_count >= PSX_LIGHT_TONES) return 0;   /* a 17th distinct tone */
            id = tweaks->tone_count;
            tweaks->tones[tweaks->tone_count++] = tone;
        }
        tweaks->tone_runs[tweaks->tone_run_count++] = (TrackLightToneRun){ from, to, (uint16_t)mask, (uint8_t)id };
        return 1;
    }
    if (strcmp(key, "car_tone") == 0) {
        unsigned from = 0, to = nodes > 0 ? nodes - 1 : 0, rgb = 0;
        int percent = 0;
        if (sscanf(rest, "%u %u %x:%d", &from, &to, &rgb, &percent) != 4) {   /* no nodes: the whole track */
            from = 0, to = nodes > 0 ? nodes - 1 : 0;
            if (sscanf(rest, "%x:%d", &rgb, &percent) != 2) return 0;
        }
        if (from > to || to >= nodes || rgb > 0xffffffu || percent < 0 || percent > 100 || tweaks->car_tone_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        tweaks->car_tone[tweaks->car_tone_count++] = (TrackLightCarTone){ from, to, { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb }, percent };
        return 1;
    }
    if (strcmp(key, "tunnel_tone") == 0) {   /* the tunnels' look this time: tunnel_tone <light> [RRGGBB[:strength]] */
        int lit = 0, has_light = 0, has_colour = 0;
        double value = 0.0;
        float cast[3];
        return parse_look(rest, 0, &lit, &has_light, &value, &has_colour, cast) && has_light && take_look(tweaks, has_light, value, has_colour, cast);
    }
    return 0;
}

int track_light_parse(const char *text, size_t size, const char *track, char letter, uint32_t expected_nodes, uint32_t flags, TrackLightFile *out, int *bad_line)
{
    if (bad_line != NULL) *bad_line = 0;
    if (text == NULL || track == NULL || out == NULL || expected_nodes == 0) return 0;
    TrackLightFile file = { 0 };
    uint32_t seen = 0;
    int header = 0, line_no = 0;
    size_t at = 0;
    while (at < size) {
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
        if (!header) {
            char file_track[32], file_letter, w_track[16], w_time[16], w_nodes[16], w_flags[16];
            unsigned file_nodes, file_flags;
            if (line[strspn(line, " \t\r")] == '\0') continue;
            if (sscanf(line, " %15s %31s %15s %c %15s %u %15s %x", w_track, file_track, w_time, &file_letter, w_nodes, &file_nodes, w_flags, &file_flags) != 8 ||
                strcmp(english(w_track), "track") != 0 || strcmp(english(w_time), "time") != 0 || strcmp(english(w_nodes), "nodes") != 0 || strcmp(english(w_flags), "flags") != 0)
                goto bad;
            if (strcmp(file_track, track) != 0 || file_letter != letter || file_nodes != expected_nodes) { track_light_file_free(&file); return 0; }
            file.flags = file_flags;
            file.base = flags == TRACK_LIGHT_ANY_FLAGS || file_flags == flags;
            file.light.nodes = expected_nodes;
            file.light.data = calloc((size_t)expected_nodes * PSX_LIGHT_POINTS, 1);   /* parsed even when it is not the base: its lines are still checked */
            if (file.light.data == NULL) return 0;
            header = 1;
            continue;
        }
        if (!parse_line(line, &file.light, &file.tweaks, &seen)) goto bad;
    }
    if (!header || seen != expected_nodes) {
        line_no = 0;
        goto bad;
    }
    if (!file.base) psx_light_free(&file.light);
    *out = file;
    return 1;
bad:
    if (bad_line != NULL) *bad_line = line_no;
    track_light_file_free(&file);
    return 0;
}

int track_light_place_parse(const char *text, size_t size, const char *track, uint32_t expected_nodes, PsxLight *out, int *bad_line)
{
    if (bad_line != NULL) *bad_line = 0;
    if (text == NULL || track == NULL || out == NULL || expected_nodes == 0) return 0;
    PsxLight places = { 0 };
    places.nodes = expected_nodes;
    int header = 0, line_no = 0;
    size_t at = 0;
    while (at < size) {
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
        char word[32], what[32];
        int used = 0;
        if (sscanf(line, "%31s%n", word, &used) != 1) continue;
        const char *rest = line + used;
        const char *const key = english(word);
        unsigned a, b;
        if (!header) {
            char file_track[32];
            if (strcmp(key, "track") != 0 || sscanf(rest, "%31s", file_track) != 1 || strchr(rest + strspn(rest, " \t"), ' ') != NULL) goto bad;
            if (strcmp(file_track, track) != 0) { psx_light_free(&places); return 0; }
            header = 1;
            continue;
        }
        uint8_t **slot = strcmp(key, "tunnel") == 0 ? &places.tunnel : strcmp(key, "canyon") == 0 ? &places.canyon : strcmp(key, "car_dim") == 0 ? &places.car_dim : NULL;
        if (slot == NULL && strcmp(key, "present") != 0) goto bad;
        if (strcmp(key, "present") == 0) {
            if (sscanf(rest, "%31s", what) != 1) goto bad;
            const char *const w = english(what);
            uint8_t **pslot = strcmp(w, "tunnel") == 0 ? &places.tunnel : strcmp(w, "canyon") == 0 ? &places.canyon : strcmp(w, "car_dim") == 0 ? &places.car_dim : NULL;
            if (pslot == NULL || flag_array(pslot, expected_nodes) == NULL) goto bad;
            continue;
        }
        if (sscanf(rest, "%u %u", &a, &b) != 2 || a > b || b >= expected_nodes) goto bad;
        uint8_t *flag = flag_array(slot, expected_nodes);
        if (flag == NULL) goto bad;
        memset(flag + a, 1, b - a + 1);
    }
    if (!header) { line_no = 0; goto bad; }
    out->tunnel = places.tunnel, out->canyon = places.canyon, out->car_dim = places.car_dim;   /* the caller's light had none of them */
    return 1;
bad:
    if (bad_line != NULL) *bad_line = line_no;
    psx_light_free(&places);
    return 0;
}

void track_light_file_free(TrackLightFile *file)
{
    if (file == NULL) return;
    psx_light_free(&file->light);
    memset(file, 0, sizeof *file);
}

void track_light_apply(const TrackLightTweaks *tweaks, PsxLight *light)
{
    if (tweaks == NULL || light == NULL) return;
    for (int i = 0; i < tweaks->brightness_count && light->data != NULL; i++) {
        const TrackLightBrightness *r = &tweaks->brightness[i];
        for (uint32_t n = r->from; n <= r->to && n < light->nodes; n++)
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
                uint8_t *at = &light->data[(size_t)n * PSX_LIGHT_POINTS + (size_t)p];
                const int scaled = *at * r->percent / 100;
                *at = (uint8_t)(scaled > 255 ? 255 : scaled);
            }
    }
    if (tweaks->tone_run_count > 0 && light->nodes > 0) {   /* the scene tones: a node's tone is its last line's */
        free(light->tone_id), free(light->tone_mask);
        light->tone_id = calloc(light->nodes, 1), light->tone_mask = calloc(light->nodes, sizeof *light->tone_mask);
        if (light->tone_id == NULL || light->tone_mask == NULL) {
            free(light->tone_id), free(light->tone_mask);
            light->tone_id = NULL, light->tone_mask = NULL, light->tone_count = 0;
        } else {
            memcpy(light->tone, tweaks->tones, sizeof light->tone);
            light->tone_count = tweaks->tone_count;
            for (int i = 0; i < tweaks->tone_run_count; i++) {
                const TrackLightToneRun *r = &tweaks->tone_runs[i];
                for (uint32_t n = r->from; n <= r->to && n < light->nodes; n++) light->tone_id[n] = (uint8_t)(r->tone + 1), light->tone_mask[n] = r->mask;
            }
        }
    }
    if (tweaks->car_tone_count > 0 && light->nodes > 0) {
        free(light->car_tone);
        light->car_tone = calloc((size_t)light->nodes, 4);
        for (int i = 0; light->car_tone != NULL && i < tweaks->car_tone_count; i++) {
            const TrackLightCarTone *t = &tweaks->car_tone[i];
            for (uint32_t n = t->from; n <= t->to && n < light->nodes; n++) {
                uint8_t *at = &light->car_tone[4 * (size_t)n];
                at[0] = t->rgb[0], at[1] = t->rgb[1], at[2] = t->rgb[2], at[3] = (uint8_t)t->percent;
            }
        }
    }
    for (int i = 0; i < tweaks->zone_count; i++) {   /* the zones over the places: a tunnel, or a hall (a tunnel with a lot of light: its own lamps) */
        const TrackLightZone *z = &tweaks->zones[i];
        uint8_t *const tunnel = flag_array(&light->tunnel, light->nodes);
        uint8_t *const hall = z->lit ? flag_array(&light->hall, light->nodes) : light->hall;
        for (uint32_t n = z->from; n <= z->to && n < light->nodes; n++) {
            if (z->lit && hall != NULL) hall[n] = 1, tunnel != NULL ? (void)(tunnel[n] = 0) : (void)0;
            else if (tunnel != NULL) tunnel[n] = 1, hall != NULL ? (void)(hall[n] = 0) : (void)0;
        }
    }
    light->look_set = 1;   /* a file's light always has the tunnels' look: its own, or the dark default (10 % of full light, no colour) */
    light->look_light = tweaks->look_has_light ? tweaks->look_light : 10.0;
    for (int k = 0; k < 3; k++) light->look_cast[k] = tweaks->look_has_colour ? tweaks->look_cast[k] : 1.0f;
}

void track_light_diff(uint32_t file, uint32_t now, char *out, size_t out_size)
{
    static const char *const NAMES[TLF_COUNT] = { "trans_shadows", "made_tunnels", "car_tunnel_light", "hall_light", "parking_dark", "cavern_dark", "canyon_shade", "lamp_pool_tint",
                                                  "lamp_car_shade", "lamp_pool_bright", "city_lift 3", "street lamps", "reverse" };
    if (out == NULL || out_size == 0) return;
    out[0] = '\0';
    size_t len = 0;
    for (int bit = 0; bit < TLF_COUNT; bit++) {
        if (((file ^ now) >> bit & 1u) == 0) continue;
        const int n = snprintf(out + len, out_size - len, "%s%s=%s", len ? ", " : "", NAMES[bit], (now >> bit & 1u) ? "on" : "off");
        if (n < 0 || (size_t)n >= out_size - len) break;
        len += (size_t)n;
    }
}

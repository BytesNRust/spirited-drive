/* port: see track_light_file.h. */
#include "game/race/track_light_file.h"
#include "game/race/terrain_light.h"


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

static int node_order(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

/* the street lamps as `lamp <node> 100` lines (the ordinary lamp: 100 percent, the default radius), by node, one for each */
static void put_lamps(Buffer *b, const TrackNodeLight *light)
{
    if (light->lamp_node == NULL || light->lamp_node_count <= 0 || light->nodes == 0) return;
    uint32_t *nodes = malloc((size_t)light->lamp_node_count * sizeof *nodes);
    if (nodes == NULL) { b->failed = 1; return; }
    int n = 0;
    for (int i = 0; i < light->lamp_node_count; i++) {
        if (!light->lamp_ring && light->lamp_node[i] >= light->nodes) continue;   /* terrain_light_lamp_classes drops it too */
        nodes[n++] = light->lamp_node[i] % light->nodes;
    }
    qsort(nodes, (size_t)n, sizeof *nodes, node_order);
    for (int i = 0; i < n; i++)
        if (i == 0 || nodes[i] != nodes[i - 1]) put(b, "lamp %u 100\n", nodes[i]);
    free(nodes);
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

int track_light_place_format(const TrackNodeLight *light, const char *track, char **text, size_t *size)
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
    if (light->snow) put(&b, "snow 1\n");
    for (int slot = 0; slot < TRACK_NODE_LIGHT_SNOW_SLOTS; slot++)
        if (light->snow_map[slot] != 0) put(&b, "snow_map %d %d\n", slot, light->snow_map[slot] - 1);
    if (b.failed) {
        free(b.text);
        return 0;
    }
    *text = b.text, *size = b.size;
    return 1;
}

int track_light_format(const TrackNodeLight *light, const char *track, char letter, uint32_t flags, char **text, size_t *size)
{
    if (light == NULL || track == NULL || light->data == NULL || light->nodes == 0 || text == NULL || size == NULL) return 0;
    Buffer b = { 0 };
    put(&b, "# tnfs track light 1\n");
    put(&b, "track %s time %c nodes %u flags %x\n", track, letter, light->nodes, flags);
    for (uint32_t n = 0; n < light->nodes;) {
        const uint8_t *row = &light->data[(size_t)n * TRACK_NODE_LIGHT_POINTS];
        uint32_t end = n;
        while (end + 1 < light->nodes && memcmp(row, &light->data[(size_t)(end + 1) * TRACK_NODE_LIGHT_POINTS], TRACK_NODE_LIGHT_POINTS) == 0) end++;
        put(&b, "rows %u %u", n, end);
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) put(&b, " %u", row[p]);
        put(&b, "\n");
        n = end + 1;
    }
    /* the places (tunnels, canyon, car_dim) are the track's file (track_light_place_format); this file holds the time's light */
    for (uint32_t n = 0; light->hall != NULL && n < light->nodes;) {   /* a hall is a tunnel with a lot of light: lit by its own lamps */
        if (!light->hall[n]) { n++; continue; }
        uint32_t end = n;
        while (end + 1 < light->nodes && light->hall[end + 1]) end++;
        put(&b, "tunnel %u %u lit\n", n, end);
        n = end + 1;
    }
    put_lamps(&b, light);
    for (uint32_t n = 0; light->tone_id != NULL && light->tunnel != NULL && n < light->nodes;) {   /* a zone with a look of its own: its tunnel nodes under a slot of that kind */
        const int id = light->tone_id[n];
        if (id <= 0 || id > light->tone_count || !light->tone[id - 1].look || !light->tunnel[n]) { n++; continue; }
        uint32_t end = n;
        while (end + 1 < light->nodes && light->tone_id[end + 1] == id && light->tunnel[end + 1]) end++;
        char colour[64];
        colour_text(light->tone[id - 1].cast, colour);
        put(&b, "tunnel %u %u %.9g %s\n", n, end, (double)light->tone[id - 1].light, colour);
        n = end + 1;
    }
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
        { "tunel", "tunnel" }, { "coche_tenue", "car_dim" }, { "canon", "canyon" }, { "nieve", "snow" },
        { "tono_tunel", "tunnel_tone" }, { "brillo", "brightness" }, { "tono_coche", "car_tone" }, { "farola", "lamp" },
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

/* a look the time's `tunnel_tone` gave is the time's one look; a second that differs is refused */
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

/* a `lamp` line (after its word): the node, the light in percent (1..TRACK_LIGHT_MAX_LAMP_PERCENT), then the colour (six hex digits, `:strength` 0..100 after them) and the radius (one or two
 * decimal digits), each optional, in that order */
static int parse_lamp(const char *rest, uint32_t nodes, TrackLightTweaks *tweaks)
{
    unsigned node, percent;
    int used = 0;
    if (sscanf(rest, "%u %u%n", &node, &percent, &used) != 2 || node >= nodes || percent < 1 || percent > TRACK_LIGHT_MAX_LAMP_PERCENT || tweaks->lamp_count >= TRACK_LIGHT_MAX_LAMPS) return 0;
    rest += used;
    TrackLightLamp lamp = { node, (uint16_t)percent, TRACK_NODE_LIGHT_LAMP_RADIUS, 0, 100, { 0, 0, 0 } };
    int have_radius = 0;
    char token[32];
    while (sscanf(rest, "%31s%n", token, &used) == 1) {
        rest += used;
        const size_t len = strlen(token);
        char *end = NULL;
        if (len >= 6 && !lamp.coloured && !have_radius) {   /* the colour: RRGGBB or RRGGBB:strength */
            char hex[7] = { 0 };
            memcpy(hex, token, 6);
            const unsigned long rgb = strtoul(hex, &end, 16);
            long strength = 100;
            if (*end != '\0' || strspn(hex, "0123456789abcdefABCDEF") != 6) return 0;
            if (len > 6) {
                if (token[6] != ':' || len < 8 || len > 10) return 0;
                strength = strtol(token + 7, &end, 10);
                if (*end != '\0' || strength < 0 || strength > 100) return 0;
            }
            lamp.coloured = 1, lamp.strength = (uint8_t)strength;
            lamp.rgb[0] = (uint8_t)(rgb >> 16), lamp.rgb[1] = (uint8_t)(rgb >> 8), lamp.rgb[2] = (uint8_t)rgb;
        } else if (len <= 2 && !have_radius) {   /* the radius */
            const unsigned long radius = strtoul(token, &end, 10);
            if (*end != '\0' || radius < 1 || radius > TRACK_LIGHT_MAX_LAMP_RADIUS) return 0;
            lamp.radius = (uint8_t)radius, have_radius = 1;
        } else {
            return 0;
        }
    }
    tweaks->lamps[tweaks->lamp_count++] = lamp;
    return 1;
}

/* the zones that have a look of their own, as tone slots shared with the `tone` lines (a look the time's tunnel_tone has is no slot; two zones with the same look share one). 0, and the line of the
 * zone, when a slot does not fit */
static int zone_slots(TrackLightTweaks *tweaks, int *bad_line)
{
    const double base_light = tweaks->look_has_light ? tweaks->look_light : 10.0;
    for (int i = 0; i < tweaks->zone_count; i++) {
        TrackLightZone *const z = &tweaks->zones[i];
        if (z->lit || (!z->has_light && !z->has_colour)) continue;
        const double light = z->has_light ? z->light : base_light;
        const float *const cast = z->has_colour ? z->cast : tweaks->look_has_colour ? tweaks->look_cast : (const float[3]){ 1.0f, 1.0f, 1.0f };
        if (light == base_light && memcmp(cast, tweaks->look_has_colour ? tweaks->look_cast : (const float[3]){ 1.0f, 1.0f, 1.0f }, 3 * sizeof(float)) == 0) continue;
        TrackNodeLightTone slot;
        memset(&slot, 0, sizeof slot);
        slot.rgb[0] = slot.rgb[1] = slot.rgb[2] = 255, slot.look = 1, slot.light = (float)light;
        memcpy(slot.cast, cast, sizeof slot.cast);
        int id = -1;
        for (int t = 0; t < tweaks->tone_count; t++)
            if (memcmp(&tweaks->tones[t], &slot, sizeof slot) == 0) id = t;
        if (id < 0) {
            if (tweaks->tone_count >= TRACK_NODE_LIGHT_TONES) {
                *bad_line = z->line;
                return 0;
            }
            id = tweaks->tone_count;
            tweaks->tones[tweaks->tone_count++] = slot;
        }
        z->tone = id;
    }
    return 1;
}

/* one line of the file into `light`; 0 when it is not well-formed. `seen` counts the node rows given. */
static int parse_line(const char *line, TrackNodeLight *light, TrackLightTweaks *tweaks, uint32_t *seen)
{
    char word[32];
    int used = 0;
    if (sscanf(line, "%31s%n", word, &used) != 1) return 1;   /* blank */
    const char *const key = english(word);
    const char *rest = line + used;
    const uint32_t nodes = light->nodes;
    unsigned a, b, c;
    if (strcmp(key, "rows") == 0) {
        unsigned v[TRACK_NODE_LIGHT_POINTS];
        int n = 0;
        if (sscanf(rest, "%u %u%n", &a, &b, &n) != 2) return 0;
        rest += n;
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) {
            if (sscanf(rest, "%u%n", &v[p], &n) != 1 || v[p] > 255) return 0;
            rest += n;
        }
        if (a > b || b >= nodes) return 0;
        for (unsigned node = a; node <= b; node++)
            for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) light->data[(size_t)node * TRACK_NODE_LIGHT_POINTS + (size_t)p] = (uint8_t)v[p];
        *seen += b - a + 1;
        return 1;
    }
    if (strcmp(key, "tunnel") == 0) {   /* a zone: nodes a..b are a tunnel (added to the track file's), with this time's light and colour, or a hall when `lit` */
        int n = 0;
        if (sscanf(rest, "%u %u%n", &a, &b, &n) != 2 || a > b || b >= nodes || tweaks->zone_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        int lit = 0, has_light = 0, has_colour = 0;
        double value = 0.0;
        float cast[3];
        if (!parse_look(rest + n, 1, &lit, &has_light, &value, &has_colour, cast)) return 0;
        TrackLightZone *const z = &tweaks->zones[tweaks->zone_count++];
        *z = (TrackLightZone){ .from = a, .to = b, .lit = lit, .has_light = has_light, .has_colour = has_colour, .light = value, .tone = -1 };
        if (has_colour) memcpy(z->cast, cast, sizeof z->cast);
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
            if (point >= TRACK_NODE_LIGHT_POINTS) return 0;
            mask |= 1u << point, rest += used;
        }
        if (sscanf(rest, " %x:%d", &rgb, &percent) != 2 || rgb > 0xffffffu || percent < 0 || percent > 100 || tweaks->tone_run_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        if (mask == 0) mask = (1u << TRACK_NODE_LIGHT_POINTS) - 1u;
        const TrackNodeLightTone tone = { .rgb = { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb }, .percent = (uint8_t)percent };
        int id = -1;
        for (int i = 0; i < tweaks->tone_count; i++)
            if (memcmp(&tweaks->tones[i], &tone, sizeof tone) == 0) id = i;
        if (id < 0) {
            if (tweaks->tone_count >= TRACK_NODE_LIGHT_TONES) return 0;   /* a 17th distinct tone */
            id = tweaks->tone_count;
            tweaks->tones[tweaks->tone_count++] = tone;
        }
        tweaks->tone_runs[tweaks->tone_run_count++] = (TrackLightToneRun){ from, to, (uint16_t)mask, (uint8_t)id };
        return 1;
    }
    if (strcmp(key, "lamp") == 0) return parse_lamp(rest, nodes, tweaks);
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
            file.light.data = calloc((size_t)expected_nodes * TRACK_NODE_LIGHT_POINTS, 1);   /* parsed even when it is not the base: its lines are still checked */
            if (file.light.data == NULL) return 0;
            header = 1;
            continue;
        }
        const int zones_before = file.tweaks.zone_count;
        if (!parse_line(line, &file.light, &file.tweaks, &seen)) goto bad;
        if (file.tweaks.zone_count > zones_before) file.tweaks.zones[zones_before].line = line_no;
    }
    if (!zone_slots(&file.tweaks, &line_no)) goto bad;
    if (!header || seen != expected_nodes) {
        line_no = 0;
        goto bad;
    }
    if (!file.base) track_node_light_free(&file.light);
    *out = file;
    return 1;
bad:
    if (bad_line != NULL) *bad_line = line_no;
    track_light_file_free(&file);
    return 0;
}

int track_light_place_parse(const char *text, size_t size, const char *track, uint32_t expected_nodes, TrackNodeLight *out, int *bad_line)
{
    if (bad_line != NULL) *bad_line = 0;
    if (text == NULL || track == NULL || out == NULL || expected_nodes == 0) return 0;
    TrackNodeLight places = { 0 };
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
            if (strcmp(file_track, track) != 0) { track_node_light_free(&places); return 0; }
            header = 1;
            continue;
        }
        if (strcmp(key, "snow") == 0) {   /* the track is a snowy one: its rain falls as snow */
            int on = 0;
            if (sscanf(rest, "%d", &on) != 1 || (on != 0 && on != 1)) goto bad;
            places.snow = on;
            continue;
        }
        if (strcmp(key, "snow_map") == 0) {   /* a texture slot of the road that the snow's bitmaps take over in the rally */
            unsigned from, to;
            int used_map = 0;
            if (sscanf(rest, "%u %u%n", &from, &to, &used_map) != 2 || from >= TRACK_NODE_LIGHT_SNOW_SLOTS || to >= TRACK_NODE_LIGHT_SNOW_SLOTS || rest[used_map + strspn(rest + used_map, " \t\r")] != '\0') goto bad;
            places.snow_map[from] = (uint8_t)(to + 1);
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
    out->tunnel = places.tunnel, out->canyon = places.canyon, out->car_dim = places.car_dim, out->snow = places.snow, out->from_places = 1;
    memcpy(out->snow_map, places.snow_map, sizeof out->snow_map);   /* the caller's light had none of them */
    return 1;
bad:
    if (bad_line != NULL) *bad_line = line_no;
    track_node_light_free(&places);
    return 0;
}

void track_light_file_free(TrackLightFile *file)
{
    if (file == NULL) return;
    track_node_light_free(&file->light);
    memset(file, 0, sizeof *file);
}

/* the colour a lamp adds, 0..1 a channel, its strongest channel its percent / 100: the warm street lamp's, or the line's mixed toward it by the line's strength */
static void lamp_colour(const TrackLightLamp *lamp, float out[3])
{
    static const float WARM[3] = { 1.0f, 0.9f, 0.55f };
    float top = 0.0f;
    for (int k = 0; k < 3; k++) {
        const float want = (float)lamp->rgb[k] / 255.0f, mix = (float)lamp->strength / 100.0f;
        out[k] = lamp->coloured ? WARM[k] + (want - WARM[k]) * mix : WARM[k];
        top = out[k] > top ? out[k] : top;
    }
    for (int k = 0; k < 3; k++) out[k] = top > 0.0f ? out[k] / top * (float)lamp->percent / 100.0f : 0.0f;
}

static int lamp_by_node(const void *a, const void *b)
{
    const TrackNodeLightLamp *x = a, *y = b;
    return x->node < y->node ? -1 : x->node > y->node;
}

/* the `lamp` lines over `light`: its lamp list, by node (the world place comes later), and the old pools the code made dropped: a lamp is light, the only one */
static void apply_lamps(const TrackLightTweaks *tweaks, TrackNodeLight *light, int ring)
{
    if (tweaks->lamp_count == 0 || light->nodes < 2) return;
    TrackNodeLightLamp *list = calloc((size_t)tweaks->lamp_count, sizeof *list);
    if (list == NULL) return;
    int reach = 0;
    for (int i = 0; i < tweaks->lamp_count; i++) {
        const TrackLightLamp *lamp = &tweaks->lamps[i];
        list[i].node = lamp->node;
        list[i].reach = (float)lamp->radius;   /* in nodes until track_node_light_lamps_place turns it into metres */
        lamp_colour(lamp, list[i].rgb);
        reach = lamp->radius > reach ? lamp->radius : reach;
    }
    qsort(list, (size_t)tweaks->lamp_count, sizeof *list, lamp_by_node);
    free(light->lamps);
    light->lamps = list, light->lamp_n = tweaks->lamp_count, light->lamp_reach_nodes = reach, light->lamp_ring = ring, light->lamp_placed = 0;
    free(light->pool_class), light->pool_class = NULL;
    free(light->car_pool), light->car_pool = NULL;
}

void track_light_apply(const TrackLightTweaks *tweaks, TrackNodeLight *light, int lamps, int ring)
{
    if (tweaks == NULL || light == NULL) return;
    for (int i = 0; i < tweaks->brightness_count && light->data != NULL; i++) {
        const TrackLightBrightness *r = &tweaks->brightness[i];
        for (uint32_t n = r->from; n <= r->to && n < light->nodes; n++)
            for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) {
                uint8_t *at = &light->data[(size_t)n * TRACK_NODE_LIGHT_POINTS + (size_t)p];
                const int scaled = *at * r->percent / 100;
                *at = (uint8_t)(scaled > 255 ? 255 : scaled);
            }
    }
    int zone_looks = 0;
    for (int i = 0; i < tweaks->zone_count; i++) zone_looks += tweaks->zones[i].tone >= 0;
    if ((tweaks->tone_run_count > 0 || zone_looks > 0) && light->nodes > 0) {   /* the scene tones and the zones' own looks: a node's slot is its last line's (the zones first, then the `tone` lines) */
        free(light->tone_id), free(light->tone_mask);
        light->tone_id = calloc(light->nodes, 1), light->tone_mask = calloc(light->nodes, sizeof *light->tone_mask);
        if (light->tone_id == NULL || light->tone_mask == NULL) {
            free(light->tone_id), free(light->tone_mask);
            light->tone_id = NULL, light->tone_mask = NULL, light->tone_count = 0;
        } else {
            memcpy(light->tone, tweaks->tones, sizeof light->tone);
            light->tone_count = tweaks->tone_count;
            for (int i = 0; i < tweaks->zone_count; i++) {
                const TrackLightZone *z = &tweaks->zones[i];
                for (uint32_t n = z->from; z->tone >= 0 && n <= z->to && n < light->nodes; n++) light->tone_id[n] = (uint8_t)(z->tone + 1), light->tone_mask[n] = (1u << TRACK_NODE_LIGHT_POINTS) - 1u;
            }
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
    if (lamps) apply_lamps(tweaks, light, ring);
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

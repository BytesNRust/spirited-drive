/* port: see track_light_file.h. */
#include "game/race/track_light_file.h"


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
    /* an array that is there but empty is not an array that is not (the rows decide a tunnel without one): named first */
    const struct { const char *key; const uint8_t *array; } arrays[] = { { "tunnel", light->tunnel }, { "hall", light->hall }, { "car_dim", light->car_dim }, { "canyon", light->canyon },
                                                                         { "pool", light->pool_class }, { "car_pool", light->car_pool } };
    for (size_t i = 0; i < sizeof arrays / sizeof *arrays; i++)
        if (arrays[i].array != NULL) put(&b, "present %s\n", arrays[i].key);
    put_ranges(&b, "tunnel", light->tunnel, light->nodes);
    put_ranges(&b, "hall", light->hall, light->nodes);
    put_ranges(&b, "car_dim", light->car_dim, light->nodes);
    put_ranges(&b, "canyon", light->canyon, light->nodes);
    put_classes(&b, "pool", light->pool_class, light->nodes);
    put_classes(&b, "car_pool", light->car_pool, light->nodes);
    if (light->car_disc)
        put(&b, "car_disc 1 %a %a %a %a %a %a\n", (double)light->car_even[0], (double)light->car_even[1], (double)light->car_even[2], (double)light->car_odd[0], (double)light->car_odd[1], (double)light->car_odd[2]);
    put(&b, "cast neutral %d city %d\n", light->neutral_cast, light->city_tunnel);
    for (int dark = 0; dark < 2; dark++)
        if (light->warm_percent[dark] > 0)
            put(&b, "tunnel_tone %s %a %a %a %d\n", dark ? "night" : "day", (double)light->warm_cast[dark][0], (double)light->warm_cast[dark][1], (double)light->warm_cast[dark][2], light->warm_percent[dark]);
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
        { "tunel", "tunnel" }, { "sala", "hall" }, { "coche_tenue", "car_dim" }, { "canon", "canyon" }, { "pozo", "pool" }, { "pozo_coche", "car_pool" },
        { "coche_disco", "car_disc" }, { "reparto", "cast" }, { "neutro", "neutral" }, { "ciudad", "city" }, { "tono_tunel", "tunnel_tone" },
        { "dia", "day" }, { "noche", "night" }, { "brillo", "brightness" }, { "tono_coche", "car_tone" },
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
        uint8_t **slot = strcmp(what, "tunnel") == 0 ? &light->tunnel : strcmp(what, "hall") == 0 ? &light->hall : strcmp(what, "car_dim") == 0 ? &light->car_dim
                         : strcmp(what, "canyon") == 0 ? &light->canyon : strcmp(what, "pool") == 0 ? &light->pool_class : strcmp(what, "car_pool") == 0 ? &light->car_pool : NULL;
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
    uint8_t **range_slot = strcmp(key, "tunnel") == 0 ? &light->tunnel : strcmp(key, "hall") == 0 ? &light->hall : strcmp(key, "car_dim") == 0 ? &light->car_dim
                           : strcmp(key, "canyon") == 0 ? &light->canyon : NULL;
    if (range_slot != NULL) {
        if (sscanf(rest, "%u %u", &a, &b) != 2 || a > b || b >= nodes) return 0;
        uint8_t *flag = flag_array(range_slot, nodes);
        if (flag == NULL) return 0;
        memset(flag + a, 1, b - a + 1);
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
    if (strcmp(key, "tunnel_tone") == 0) {
        char spoken[16];
        int n = 0, percent;
        if (sscanf(rest, "%15s%n", spoken, &n) != 1) return 0;
        const char *const when = english(spoken);
        if (strcmp(when, "day") != 0 && strcmp(when, "night") != 0) return 0;
        const int dark = when[0] == 'n';
        float cast[3];
        rest += n;
        for (int i = 0; i < 3; i++) {
            char *end = NULL;
            cast[i] = (float)strtod(rest, &end);
            if (end == rest || cast[i] < 0.0f || cast[i] > 4.0f) return 0;
            rest = end;
        }
        if (sscanf(rest, "%d", &percent) != 1 || percent < 1 || percent > 1000) return 0;
        memcpy(tweaks->warm_cast[dark], cast, sizeof cast);
        tweaks->warm_percent[dark] = percent;
        tweaks->warm_set[dark] = 1;
        return 1;
    }
    if (strcmp(key, "cast") == 0) {
        char n1[16], n2[16];
        int neutral, city;
        if (sscanf(rest, "%15s %d %15s %d", n1, &neutral, n2, &city) != 4 || strcmp(english(n1), "neutral") != 0 || strcmp(english(n2), "city") != 0) return 0;
        light->neutral_cast = neutral, light->city_tunnel = city;
        return 1;
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
    for (int dark = 0; dark < 2; dark++)
        if (tweaks->warm_set[dark]) {
            memcpy(light->warm_cast[dark], tweaks->warm_cast[dark], sizeof light->warm_cast[dark]);
            light->warm_percent[dark] = tweaks->warm_percent[dark];
        }
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

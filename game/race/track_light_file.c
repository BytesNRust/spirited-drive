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
    put(&b, "pista %s hora %c nodos %u banderas %x\n", track, letter, light->nodes, flags);
    for (uint32_t n = 0; n < light->nodes;) {
        const uint8_t *row = &light->data[(size_t)n * PSX_LIGHT_POINTS];
        uint32_t end = n;
        while (end + 1 < light->nodes && memcmp(row, &light->data[(size_t)(end + 1) * PSX_LIGHT_POINTS], PSX_LIGHT_POINTS) == 0) end++;
        put(&b, "filas %u %u", n, end);
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) put(&b, " %u", row[p]);
        put(&b, "\n");
        n = end + 1;
    }
    /* an array that is there but empty is not an array that is not (the rows decide a tunnel without one): named first */
    const struct { const char *key; const uint8_t *array; } arrays[] = { { "tunel", light->tunnel }, { "sala", light->hall }, { "coche_tenue", light->car_dim }, { "canon", light->canyon },
                                                                         { "pozo", light->pool_class }, { "pozo_coche", light->car_pool } };
    for (size_t i = 0; i < sizeof arrays / sizeof *arrays; i++)
        if (arrays[i].array != NULL) put(&b, "presente %s\n", arrays[i].key);
    put_ranges(&b, "tunel", light->tunnel, light->nodes);
    put_ranges(&b, "sala", light->hall, light->nodes);
    put_ranges(&b, "coche_tenue", light->car_dim, light->nodes);
    put_ranges(&b, "canon", light->canyon, light->nodes);
    put_classes(&b, "pozo", light->pool_class, light->nodes);
    put_classes(&b, "pozo_coche", light->car_pool, light->nodes);
    if (light->car_disc)
        put(&b, "coche_disco 1 %a %a %a %a %a %a\n", (double)light->car_even[0], (double)light->car_even[1], (double)light->car_even[2], (double)light->car_odd[0], (double)light->car_odd[1], (double)light->car_odd[2]);
    put(&b, "reparto neutro %d ciudad %d\n", light->neutral_cast, light->city_tunnel);
    for (int dark = 0; dark < 2; dark++)
        if (light->warm_percent[dark] > 0)
            put(&b, "tono_tunel %s %a %a %a %d\n", dark ? "noche" : "dia", (double)light->warm_cast[dark][0], (double)light->warm_cast[dark][1], (double)light->warm_cast[dark][2], light->warm_percent[dark]);
    if (b.failed) {
        free(b.text);
        return 0;
    }
    *text = b.text, *size = b.size;
    return 1;
}

static uint8_t *flag_array(uint8_t **slot, uint32_t nodes)
{
    if (*slot == NULL) *slot = calloc(nodes, 1);
    return *slot;
}

/* one line of the file into `light`; 0 when it is not well-formed. `seen` counts the node rows given. */
static int parse_line(const char *line, PsxLight *light, TrackLightTweaks *tweaks, uint32_t *seen)
{
    char key[32];
    int used = 0;
    if (sscanf(line, "%31s%n", key, &used) != 1) return 1;   /* blank */
    const char *rest = line + used;
    const uint32_t nodes = light->nodes;
    unsigned a, b, c;
    if (strcmp(key, "presente") == 0) {
        char what[32];
        if (sscanf(rest, "%31s", what) != 1) return 0;
        uint8_t **slot = strcmp(what, "tunel") == 0 ? &light->tunnel : strcmp(what, "sala") == 0 ? &light->hall : strcmp(what, "coche_tenue") == 0 ? &light->car_dim
                         : strcmp(what, "canon") == 0 ? &light->canyon : strcmp(what, "pozo") == 0 ? &light->pool_class : strcmp(what, "pozo_coche") == 0 ? &light->car_pool : NULL;
        return slot != NULL && flag_array(slot, nodes) != NULL;
    }
    if (strcmp(key, "filas") == 0) {
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
    uint8_t **range_slot = strcmp(key, "tunel") == 0 ? &light->tunnel : strcmp(key, "sala") == 0 ? &light->hall : strcmp(key, "coche_tenue") == 0 ? &light->car_dim
                           : strcmp(key, "canon") == 0 ? &light->canyon : NULL;
    if (range_slot != NULL) {
        if (sscanf(rest, "%u %u", &a, &b) != 2 || a > b || b >= nodes) return 0;
        uint8_t *flag = flag_array(range_slot, nodes);
        if (flag == NULL) return 0;
        memset(flag + a, 1, b - a + 1);
        return 1;
    }
    uint8_t **class_slot = strcmp(key, "pozo") == 0 ? &light->pool_class : strcmp(key, "pozo_coche") == 0 ? &light->car_pool : NULL;
    if (class_slot != NULL) {
        if (sscanf(rest, "%u %u %u", &a, &b, &c) != 3 || a > b || b >= nodes || c < 1 || c > 4) return 0;
        uint8_t *cls = flag_array(class_slot, nodes);
        if (cls == NULL) return 0;
        memset(cls + a, (int)c, b - a + 1);
        return 1;
    }
    if (strcmp(key, "coche_disco") == 0) {
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
    if (strcmp(key, "brillo") == 0) {
        if (sscanf(rest, "%u %u %u", &a, &b, &c) != 3 || a > b || b >= nodes || c > 1000 || tweaks->brightness_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        tweaks->brightness[tweaks->brightness_count++] = (TrackLightBrightness){ a, b, (int)c };
        return 1;
    }
    if (strcmp(key, "tono_tunel") == 0) {
        char when[16];
        int n = 0, percent;
        if (sscanf(rest, "%15s%n", when, &n) != 1 || (strcmp(when, "dia") != 0 && strcmp(when, "noche") != 0)) return 0;
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
    if (strcmp(key, "reparto") == 0) {
        char n1[16], n2[16];
        int neutral, city;
        if (sscanf(rest, "%15s %d %15s %d", n1, &neutral, n2, &city) != 4 || strcmp(n1, "neutro") != 0 || strcmp(n2, "ciudad") != 0) return 0;
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
            char file_track[32], file_letter;
            unsigned file_nodes, file_flags;
            if (line[strspn(line, " \t\r")] == '\0') continue;
            if (sscanf(line, " pista %31s hora %c nodos %u banderas %x", file_track, &file_letter, &file_nodes, &file_flags) != 4) goto bad;
            if (strcmp(file_track, track) != 0 || file_letter != letter || file_nodes != expected_nodes) { track_light_file_free(&file); return 0; }
            file.flags = file_flags;
            file.base = file_flags == flags;
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

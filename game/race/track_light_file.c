/* port: see track_light_file.h. */
#include "formats/light_points.h"
#include "game/race/track_light_file.h"
#include "game/race/terrain_light.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LINE = 512 };

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
static int parse_sun(const char *rest, TrackLightTweaks *tweaks);

/* a lamp line's STRIP, its first word `word` and the rest of the line at `*rest`: right, left, road or all; or `pA pB` / `pA` (points 0..10, A not above B). Returns 1 when `word` begins a strip, its points in `*mask`
 * (0 for a malformed one: a point out of the row, B before A), and moves `*rest` past the strip's second word */
static int lamp_strip_word(const char *word, const char **rest, uint16_t *mask)
{
    *mask = 0;
    if (strcmp(word, "all") == 0) return *mask = 0x7ff, 1;
    if (strcmp(word, "left") == 0) return *mask = 0x7c0, 1;
    if (strcmp(word, "right") == 0) return *mask = 0x03e, 1;
    if (strcmp(word, "road") == 0) return *mask = 0x043, 1;
    unsigned a, b;
    char tail;
    unsigned points = 0;
    const int list = light_points_parse(word, &points);   /* `p1,p3,p7` */
    if (list != 0) return *mask = (uint16_t)points, 1;
    if (word[0] != 'p' || sscanf(word + 1, "%u%c", &a, &tail) != 1) return 0;
    if (a > 10) return 1;
    char next[32];
    int used = 0;
    b = a;
    if (sscanf(*rest, "%31s%n", next, &used) == 1 && next[0] == 'p' && sscanf(next + 1, "%u%c", &b, &tail) == 1) {
        *rest += used;
        if (b > 10 || b < a) return 1;
    } else {
        b = a;
    }
    for (unsigned p = a; p <= b; p++) *mask |= (uint16_t)(1u << p);
    return 1;
}

static int parse_lamp(const char *rest, uint32_t nodes, TrackLightTweaks *tweaks)
{
    unsigned node, percent;
    int used = 0;
    if (sscanf(rest, "%u %u%n", &node, &percent, &used) != 2 || node >= nodes || percent < 1 || percent > TRACK_LIGHT_MAX_LAMP_PERCENT || tweaks->lamp_count >= TRACK_LIGHT_MAX_LAMPS) return 0;
    rest += used;
    TrackLightLamp lamp = { node, (uint16_t)percent, TRACK_NODE_LIGHT_LAMP_RADIUS, 0, 100, { 0, 0, 0 }, 0 };
    int have_radius = 0;
    char token[32];
    while (sscanf(rest, "%31s%n", token, &used) == 1) {
        rest += used;
        const size_t len = strlen(token);
        char *end = NULL;
        if (lamp.mask != 0) return 0;   /* the strip is the last word of the line */
        if (lamp_strip_word(token, &rest, &lamp.mask)) {
            if (lamp.mask == 0) return 0;
        } else if (len >= 6 && !lamp.coloured && !have_radius) {   /* the colour: RRGGBB or RRGGBB:strength */
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

static int parse_uint(const char *token, unsigned *out)
{
    char *end = NULL;
    if (token[0] < '0' || token[0] > '9') return 0;
    const unsigned long v = strtoul(token, &end, 10);
    if (*end != '\0' || v > 0xffffffu) return 0;
    *out = (unsigned)v;
    return 1;
}

/* `tunnel_lamp [FROM TO] <strength> [colour] [reach]` (the second word is a colour or a number: a colour is 6 hex digits or r/g/b, so two whole numbers first are the nodes) */
static int parse_tunnel_lamp(const char *rest, uint32_t nodes, TrackLightTweaks *tweaks)
{
    char token[6][48], extra[48];
    int count = 0, used = 0;
    if (tweaks->tunnel_lamp_count >= TRACK_LIGHT_MAX_TUNNEL_LAMPS) return 0;
    while (count < 6 && sscanf(rest, "%47s%n", token[count], &used) == 1) rest += used, count++;
    if (sscanf(rest, "%47s", extra) == 1 || count == 0) return 0;   /* nothing, or too many words */
    TrackLightTunnelLamp line = { .from = 0, .to = nodes > 0 ? nodes - 1 : 0, .general = 1 };
    unsigned a = 0, b = 0;
    float colour[3];
    int at = 0;
    if (count >= 3 && parse_uint(token[0], &a) && parse_uint(token[1], &b) && !parse_colour(token[1], colour)) {
        if (a > b || b >= nodes) return 0;
        line.from = a, line.to = b, line.general = 0, at = 2;
    }
    char *end = NULL;
    line.strength = strtod(token[at], &end);
    if (end == token[at] || *end != '\0' || line.strength < 0.0 || line.strength > 1000.0) return 0;
    for (++at; at < count; ++at) {
        if (parse_colour(token[at], colour) && !line.has_colour && !line.has_reach) {
            line.has_colour = 1, memcpy(line.cast, colour, sizeof line.cast);
            continue;
        }
        const double v = strtod(token[at], &end);
        if (line.has_reach || end == token[at] || *end != '\0' || v < 0.5 || v > 4.0) return 0;
        line.has_reach = 1, line.reach = v;
    }
    tweaks->tunnel_lamps[tweaks->tunnel_lamp_count++] = line;
    return 1;
}

int track_light_tunnel_lamp_at(const TrackLightTweaks *tweaks, uint32_t node, double *strength, float cast[3], double *reach)
{
    int general = -1, over = -1, set = 0;
    for (int i = 0; i < tweaks->tunnel_lamp_count; i++) {
        const TrackLightTunnelLamp *l = &tweaks->tunnel_lamps[i];
        if (l->general) general = i;
        else if (node >= l->from && node <= l->to) over = i;
    }
    const int lines[2] = { general, over };
    for (int k = 0; k < 2; k++) {
        if (lines[k] < 0) continue;
        const TrackLightTunnelLamp *l = &tweaks->tunnel_lamps[lines[k]];
        *strength = l->strength, set |= TRACK_LIGHT_TUNNEL_LAMP_STRENGTH;
        if (l->has_colour) memcpy(cast, l->cast, sizeof l->cast), set |= TRACK_LIGHT_TUNNEL_LAMP_COLOUR;
        if (l->has_reach) *reach = l->reach, set |= TRACK_LIGHT_TUNNEL_LAMP_REACH;
    }
    return set;
}

/* one line of a time's retouches into `tweaks`; 0 when it is not well-formed */
static int parse_line(const char *line, uint32_t nodes, TrackLightTweaks *tweaks)
{
    char key[32];
    int used = 0;
    if (sscanf(line, "%31s%n", key, &used) != 1) return 1;   /* blank */
    const char *rest = line + used;
    unsigned a, b, c;
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
        int n = 0;
        if (sscanf(rest, "%u %u %u%n", &a, &b, &c, &n) != 3 || a > b || b >= nodes || c > 1000 || tweaks->brightness_count >= TRACK_LIGHT_MAX_BRIGHT) return 0;
        if (rest[n] != '\0' && rest[n] != ' ' && rest[n] != '\t' && rest[n] != '\r' && rest[n] != '\n') return 0;   /* a word is a word: "50left" is no line */
        TrackLightBrightness line = { .from = a, .to = b, .percent = (int)c };
        int has_side = 0;
        for (const char *word = rest + n;;) {   /* [left|right] [shade], each once, in either order: any other word is a bad line */
            while (*word == ' ' || *word == '\t') word++;
            if (*word == '\0' || *word == '\n' || *word == '\r') break;
            const size_t len = strcspn(word, " \t\r\n");
            if (len == 4 && strncmp(word, "left", 4) == 0 && !has_side) line.side = TRACK_LIGHT_SIDE_LEFT, has_side = 1;
            else if (len == 5 && strncmp(word, "right", 5) == 0 && !has_side) line.side = TRACK_LIGHT_SIDE_RIGHT, has_side = 1;
            else if (len == 5 && strncmp(word, "shade", 5) == 0 && !line.shade) line.shade = 1;
            else return 0;
            word += len;
        }
        tweaks->brightness[tweaks->brightness_count++] = line;
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
    if (strcmp(key, "tunnel_lamp") == 0) return parse_tunnel_lamp(rest, nodes, tweaks);
    if (strcmp(key, "sun") == 0) return parse_sun(rest, tweaks);
    if (strcmp(key, "tunnel_tone") == 0) {   /* the tunnels' look this time: tunnel_tone <light> [RRGGBB[:strength]] */
        int lit = 0, has_light = 0, has_colour = 0;
        double value = 0.0;
        float cast[3];
        return parse_look(rest, 0, &lit, &has_light, &value, &has_colour, cast) && has_light && take_look(tweaks, has_light, value, has_colour, cast);
    }
    return 0;
}

int track_light_tweak_line(const char *line, uint32_t nodes, TrackLightTweaks *tweaks)
{
    return parse_line(line, nodes, tweaks);
}

int track_light_tweaks_finish(TrackLightTweaks *tweaks, int *bad_line)
{
    return zone_slots(tweaks, bad_line);
}

/* `sun AZIMUTH ELEVATION SHADOW`: degrees, degrees, percent; a word may be `-` for the generator's own. One a file. */
static int sun_word(const char **at, double lo, double hi, double *out, int *given)
{
    while (**at == ' ' || **at == '\t') (*at)++;
    if (**at == '-' && ((*at)[1] == '\0' || (*at)[1] == ' ' || (*at)[1] == '\t' || (*at)[1] == '\r')) {
        (*at)++;
        *given = 0;
        return 1;
    }
    char *end = NULL;
    *out = strtod(*at, &end);
    if (end == *at || !(*out >= lo && *out <= hi) || (*end != '\0' && *end != ' ' && *end != '\t' && *end != '\r')) return 0;
    *at = end;
    *given = 1;
    return 1;
}

static int parse_sun(const char *rest, TrackLightTweaks *tweaks)
{
    double az = 0, el = 0, shadow = 100, reach = 0;
    int has_az, has_el, has_shadow, has_reach = 0;
    if (tweaks->sun_set != 0 || !sun_word(&rest, -360, 360, &az, &has_az) || !sun_word(&rest, 1, 90, &el, &has_el) || !sun_word(&rest, 0, 100, &shadow, &has_shadow)) return 0;
    while (*rest == ' ' || *rest == '\t' || *rest == '\r') rest++;
    if (*rest != '\0' && !sun_word(&rest, 1, 2000, &reach, &has_reach)) return 0;   /* an optional fourth word: the reach in units */
    while (*rest == ' ' || *rest == '\t' || *rest == '\r') rest++;
    if (*rest != '\0') return 0;   /* a fifth word */
    if (!has_az && !has_el && !has_shadow && !has_reach) return 0;   /* nothing said: not a line */
    tweaks->sun_set = (has_az ? TRACK_LIGHT_SUN_AZIMUTH : 0) | (has_el ? TRACK_LIGHT_SUN_ELEVATION : 0) | (has_shadow ? TRACK_LIGHT_SUN_SHADOW : 0) | (has_reach ? TRACK_LIGHT_SUN_REACH : 0);
    tweaks->sun_azimuth = (float)az, tweaks->sun_elevation = (float)el, tweaks->sun_shadow = (float)shadow, tweaks->sun_reach = (float)reach;
    return 1;
}

void track_light_file_free(TrackLightFile *file)
{
    if (file == NULL) return;
    free(file->row_set);
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
        list[i].strip = lamp->mask;
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
    /* the lines with `shade` first, over the rows as the file gives them (that is the shade the sun calculated), then the plain ones in file order */
    for (int pass = 1; pass >= 0; pass--)
        for (int i = 0; i < tweaks->brightness_count && light->data != NULL; i++) {
            const TrackLightBrightness *r = &tweaks->brightness[i];
            if (r->shade != pass) continue;
            for (uint32_t n = r->from; n <= r->to && n < light->nodes; n++)
                for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) {
                    if (r->mask != 0 ? !((r->mask >> p) & 1) : ((r->side == TRACK_LIGHT_SIDE_RIGHT && p > 5) || (r->side == TRACK_LIGHT_SIDE_LEFT && p >= 1 && p <= 5))) continue;   /* the middle point is on both sides of an old line */
                    uint8_t *at = &light->data[(size_t)n * TRACK_NODE_LIGHT_POINTS + (size_t)p];
                    if (r->shade) {   /* only the darkness: 255 - v is scaled by 100 / percent, at most all the way to black; a point in full light stays */
                        const int dark = 255 - *at, scaled = r->percent == 0 ? (dark > 0 ? 255 : 0) : (dark * 100 + r->percent / 2) / r->percent;
                        *at = (uint8_t)(255 - (scaled > 255 ? 255 : scaled));
                    } else {
                        const int scaled = *at * r->percent / 100;
                        *at = (uint8_t)(scaled > 255 ? 255 : scaled);
                    }
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
    for (int i = 0; i < tweaks->zone_count; i++) {   /* the zones over the places: a tunnel, or a hall (a tunnel with a lot of light: its own lamps) */
        const TrackLightZone *z = &tweaks->zones[i];
        uint8_t *const tunnel = flag_array(&light->tunnel, light->nodes);
        uint8_t *const hall = z->lit ? flag_array(&light->hall, light->nodes) : light->hall;
        for (uint32_t n = z->from; n <= z->to && n < light->nodes; n++) {
            if (z->lit && hall != NULL) hall[n] = 1, tunnel != NULL ? (void)(tunnel[n] = 0) : (void)0;
            else if (tunnel != NULL) tunnel[n] = 1, hall != NULL ? (void)(hall[n] = 0) : (void)0;
        }
    }
    for (uint32_t n = 0; light->tunnel != NULL && n < light->nodes; n++) {   /* the tunnels whose lamps are written at 0 %: the car's lamp alternation is off there (an unlit tunnel) */
        double strength = 0.0, reach = 0.0;
        float cast[3];
        if (!light->tunnel[n] || (track_light_tunnel_lamp_at(tweaks, n, &strength, cast, &reach) & TRACK_LIGHT_TUNNEL_LAMP_STRENGTH) == 0 || strength > 0.0) continue;
        uint8_t *const off = flag_array(&light->lamps_off, light->nodes);
        if (off != NULL) off[n] = 1;
    }
    light->look_set = 1;   /* a file's light always has the tunnels' look: its own, or the dark default (10 % of full light, no colour) */
    light->look_light = tweaks->look_has_light ? tweaks->look_light : 10.0;
    for (int k = 0; k < 3; k++) light->look_cast[k] = tweaks->look_has_colour ? tweaks->look_cast[k] : 1.0f;
    if (lamps) apply_lamps(tweaks, light, ring);
}

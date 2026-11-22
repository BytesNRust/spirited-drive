/* A track's light file held as its lines (light_file.h). The rules of a value are the game's parser's (game/race/track_light_file.c). */
#include "light_file.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TOKEN = 64 };

static int fail(char *why, size_t cap, const char *text)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s", text);
    return 0;
}

// ---- tokens -------------------------------------------------------------------------------------------------------------------------------------------------------

// the next blank-separated word of *p (a trailing '\r' counts as blank) into `out`; 1, 0 at the end, -1 for a word longer than TOKEN - 1
static int next_token(const char **p, char *out)
{
    const char *s = *p;
    while (*s == ' ' || *s == '\t' || *s == '\r') ++s;
    if (*s == '\0') return 0;
    size_t n = 0;
    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r') {
        if (n + 1 >= TOKEN) return -1;
        out[n++] = *s++;
    }
    out[n] = '\0';
    *p = s;
    return 1;
}

static int is_unsigned(const char *token, unsigned *out)
{
    if (token[0] == '\0' || strlen(token) > 9) return 0;
    unsigned v = 0;
    for (const char *c = token; *c != '\0'; ++c) {
        if (!isdigit((unsigned char)*c)) return 0;
        v = v * 10 + (unsigned)(*c - '0');
    }
    *out = v;
    return 1;
}

// the game's English words for the header (the first version of the format was Spanish)
static const char *english(const char *word)
{
    static const char *const WORDS[][2] = { { "pista", "track" }, { "hora", "time" }, { "nodos", "nodes" } };
    for (size_t i = 0; i < sizeof WORDS / sizeof WORDS[0]; ++i)
        if (strcmp(word, WORDS[i][0]) == 0) return WORDS[i][1];
    return word;
}

// ---- values -------------------------------------------------------------------------------------------------------------------------------------------------------

static int is_hex6(const char *s)
{
    for (int i = 0; i < 6; ++i)
        if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

// the strength after a colon: 0..100, the whole of the rest
static int is_strength(const char *s)
{
    char *end = NULL;
    const double v = strtod(s, &end);
    return end != s && *end == '\0' && v >= 0.0 && v <= 100.0;
}

// a tunnel look's colour as the game takes it: RRGGBB or r/g/b (three factors 0..1), then an optional :strength 0..100
static int is_look_colour(const char *token)
{
    const size_t colon = strcspn(token, ":");
    if (token[colon] == ':' && !is_strength(token + colon + 1)) return 0;
    if (memchr(token, '/', colon) != NULL) {
        char part[TOKEN];
        if (colon >= sizeof part) return 0;
        memcpy(part, token, colon);
        part[colon] = '\0';
        const char *at = part;
        for (int k = 0; k < 3; ++k) {
            char *end = NULL;
            const double v = strtod(at, &end);
            if (end == at || v < 0.0 || v > 1.0 || (k < 2 ? *end != '/' : *end != '\0')) return 0;
            at = end + 1;
        }
        return 1;
    }
    return colon == 6 && is_hex6(token);
}

// a lamp's colour: RRGGBB or RRGGBB:strength
static int is_lamp_colour(const char *token)
{
    const size_t len = strlen(token);
    if (len < 6 || !is_hex6(token)) return 0;
    if (len == 6) return 1;
    return token[6] == ':' && len >= 8 && len <= 10 && is_strength(token + 7);
}

static int parse_brightness(const char *line, unsigned nodes, LightBrightness *out)
{
    char word[TOKEN], a[TOKEN], b[TOKEN], c[TOKEN], extra[TOKEN];
    const char *p = line;
    LightBrightness v;
    memset(&v, 0, sizeof v);
    if (next_token(&p, word) != 1 || strcmp(word, "brightness") != 0) return 0;
    if (next_token(&p, a) != 1 || next_token(&p, b) != 1 || next_token(&p, c) != 1) return 0;
    if (!is_unsigned(a, &v.from) || !is_unsigned(b, &v.to) || !is_unsigned(c, &v.percent)) return 0;
    int has_side = 0, got;
    while ((got = next_token(&p, extra)) == 1) {   // [left|right] [shade], each once, in either order; any other word is a bad line, as the game takes it
        if (strcmp(extra, "left") == 0 && !has_side) v.side = LIGHT_SIDE_LEFT, has_side = 1;
        else if (strcmp(extra, "right") == 0 && !has_side) v.side = LIGHT_SIDE_RIGHT, has_side = 1;
        else if (strcmp(extra, "shade") == 0 && !v.shade) v.shade = 1;
        else return 0;
    }
    if (got != 0) return 0;
    if (v.from > v.to || v.to >= nodes || v.percent > LIGHT_MAX_BRIGHTNESS_PERCENT) return 0;
    if (out != NULL) *out = v;
    return 1;
}

static int parse_lamp(const char *line, unsigned nodes, LightLamp *out)
{
    char word[TOKEN], token[TOKEN];
    const char *p = line;
    LightLamp v;
    memset(&v, 0, sizeof v);
    if (next_token(&p, word) != 1 || strcmp(word, "lamp") != 0) return 0;
    if (next_token(&p, token) != 1 || !is_unsigned(token, &v.node)) return 0;
    if (next_token(&p, token) != 1 || !is_unsigned(token, &v.percent)) return 0;
    if (v.node >= nodes || v.percent < 1 || v.percent > LIGHT_MAX_LAMP_PERCENT) return 0;
    int got;
    while ((got = next_token(&p, token)) == 1) {
        const size_t len = strlen(token);
        if (len >= 6 && !v.has_colour && !v.has_radius) {
            if (!is_lamp_colour(token) || len >= sizeof v.colour) return 0;
            v.has_colour = 1;
            memcpy(v.colour, token, len + 1);
        } else if (len <= 2 && !v.has_radius) {
            if (!is_unsigned(token, &v.radius) || v.radius < 1 || v.radius > LIGHT_MAX_LAMP_RADIUS) return 0;
            v.has_radius = 1;
        } else {
            return 0;
        }
    }
    if (got < 0) return 0;
    if (out != NULL) *out = v;
    return 1;
}

static int parse_tunnel_tone(const char *line, LightTunnelTone *out)
{
    char word[TOKEN], token[TOKEN];
    const char *p = line;
    LightTunnelTone v;
    memset(&v, 0, sizeof v);
    int has_light = 0, got;
    if (next_token(&p, word) != 1 || strcmp(word, "tunnel_tone") != 0) return 0;
    while ((got = next_token(&p, token)) == 1) {
        if (is_look_colour(token) && !v.has_colour) {
            if (strlen(token) >= sizeof v.colour) return 0;
            v.has_colour = 1;
            memcpy(v.colour, token, strlen(token) + 1);
        } else {
            char *end = NULL;
            const double light = strtod(token, &end);
            if (has_light || v.has_colour || end == token || *end != '\0' || !(light >= 0.0 && light <= LIGHT_MAX_TUNNEL_LIGHT)) return 0;
            v.light = light;
            has_light = 1;
        }
    }
    if (got < 0 || !has_light) return 0;
    if (out != NULL) *out = v;
    return 1;
}

// a tone's colour and percent as one token, RRGGBB:percent (the game reads `%x:%d`; the editor takes exactly six hex digits and a whole percent 0..100)
static int parse_tone_colour(const char *token, char colour[8], unsigned *percent)
{
    if (strlen(token) < 8 || token[6] != ':' || !is_hex6(token)) return 0;
    if (!is_unsigned(token + 7, percent) || *percent > 100) return 0;
    for (int i = 0; i < 6; ++i) colour[i] = (char)tolower((unsigned char)token[i]);
    colour[6] = '\0';
    return 1;
}

static int parse_tone(const char *line, unsigned nodes, LightTone *out)
{
    char word[TOKEN], token[TOKEN], a[TOKEN], b[TOKEN];
    const char *p = line;
    LightTone v;
    memset(&v, 0, sizeof v);
    if (next_token(&p, word) != 1 || strcmp(word, "tone") != 0) return 0;
    if (next_token(&p, a) != 1 || next_token(&p, b) != 1 || !is_unsigned(a, &v.from) || !is_unsigned(b, &v.to) || v.from > v.to || v.to >= nodes) return 0;
    int got;
    int coloured = 0;
    while ((got = next_token(&p, token)) == 1) {
        unsigned point;
        if (coloured) return 0;
        if (token[0] == 'p' && is_unsigned(token + 1, &point)) {
            if (point >= LIGHT_POINTS) return 0;
            v.mask |= 1u << point;
        } else if (parse_tone_colour(token, v.colour, &v.percent)) {
            coloured = 1;
        } else {
            return 0;
        }
    }
    if (got < 0 || !coloured) return 0;
    if (out != NULL) *out = v;
    return 1;
}

static int parse_car_tone(const char *line, unsigned nodes, LightCarTone *out)
{
    char word[TOKEN], t[3][TOKEN], extra[TOKEN];
    const char *p = line;
    LightCarTone v;
    memset(&v, 0, sizeof v);
    if (next_token(&p, word) != 1 || strcmp(word, "car_tone") != 0) return 0;
    int n = 0;
    while (n < 3 && next_token(&p, t[n]) == 1) ++n;
    if (next_token(&p, extra) != 0) return 0;
    if (n == 1) {
        if (!parse_tone_colour(t[0], v.colour, &v.percent)) return 0;
    } else {
        if (n != 3 || !is_unsigned(t[0], &v.from) || !is_unsigned(t[1], &v.to) || v.from > v.to || v.to >= nodes || !parse_tone_colour(t[2], v.colour, &v.percent)) return 0;
        v.has_nodes = 1;
    }
    if (out != NULL) *out = v;
    return 1;
}

static int parse_zone(const char *line, unsigned nodes, LightZone *out)
{
    char word[TOKEN], a[TOKEN], b[TOKEN], token[TOKEN];
    const char *p = line;
    LightZone v;
    memset(&v, 0, sizeof v);
    if (next_token(&p, word) != 1 || strcmp(word, "tunnel") != 0) return 0;
    if (next_token(&p, a) != 1 || next_token(&p, b) != 1 || !is_unsigned(a, &v.from) || !is_unsigned(b, &v.to) || v.from > v.to || v.to >= nodes) return 0;
    int got;
    while ((got = next_token(&p, token)) == 1) {
        if (strcmp(token, "lit") == 0) {
            if (v.lit) return 0;
            v.lit = 1;
        } else if (is_look_colour(token) && !v.has_colour) {
            if (strlen(token) >= sizeof v.colour) return 0;
            v.has_colour = 1;
            memcpy(v.colour, token, strlen(token) + 1);
        } else {
            char *end = NULL;
            const double light = strtod(token, &end);
            if (v.has_light || v.has_colour || end == token || *end != '\0' || !(light >= 0.0 && light <= LIGHT_MAX_TUNNEL_LIGHT)) return 0;
            v.light = light;
            v.has_light = 1;
        }
    }
    if (got < 0 || (v.lit && (v.has_light || v.has_colour))) return 0;   // a hall is lit by its own lamps
    if (out != NULL) *out = v;
    return 1;
}

static int parse_place(const char *line, LightPlace *out)
{
    char word[TOKEN], a[TOKEN], b[TOKEN], extra[TOKEN];
    const char *p = line;
    LightPlace v;
    memset(&v, 0, sizeof v);
    if (next_token(&p, word) != 1 || (strcmp(word, "tunnel") != 0 && strcmp(word, "canyon") != 0 && strcmp(word, "car_dim") != 0)) return 0;
    if (next_token(&p, a) != 1 || next_token(&p, b) != 1 || next_token(&p, extra) != 0 || !is_unsigned(a, &v.from) || !is_unsigned(b, &v.to) || v.from > v.to) return 0;
    memcpy(v.what, word, strlen(word) + 1);
    if (out != NULL) *out = v;
    return 1;
}

// ---- lines --------------------------------------------------------------------------------------------------------------------------------------------------------

static int push_line(LightFile *f, const char *text, size_t len)
{
    if (f->count == f->cap) {
        const int cap = f->cap == 0 ? 256 : f->cap * 2;
        LightLine *lines = realloc(f->lines, sizeof *lines * (size_t)cap);
        if (lines == NULL) return 0;
        f->lines = lines;
        f->cap = cap;
    }
    char *copy = malloc(len + 1);
    if (copy == NULL) return 0;
    memcpy(copy, text, len);
    copy[len] = '\0';
    f->lines[f->count].text = copy;
    f->lines[f->count].kind = LIGHT_KIND_OTHER;
    ++f->count;
    return 1;
}

void light_file_free(LightFile *f)
{
    for (int i = 0; i < f->count; ++i) free(f->lines[i].text);
    free(f->lines);
    memset(f, 0, sizeof *f);
}

// the header: the first line whose first word is track (pista): its name, and a time and a node count for a time's file. 1, or 0 with why
static int read_header(LightFile *f, char *why, size_t cap)
{
    for (int i = 0; i < f->count; ++i) {
        const char *p = f->lines[i].text;
        char word[TOKEN];
        if (next_token(&p, word) != 1 || strcmp(english(word), "track") != 0) continue;
        char name[TOKEN];
        if (next_token(&p, name) != 1 || strlen(name) >= sizeof f->track) return fail(why, cap, "the track line has no track name (three characters such as CY1)");
        memcpy(f->track, name, strlen(name) + 1);
        char key[TOKEN], value[TOKEN];
        int has_time = 0, has_nodes = 0;
        while (next_token(&p, key) == 1 && next_token(&p, value) == 1) {
            const char *const k = english(key);
            if (strcmp(k, "time") == 0) {
                if (strlen(value) != 1 || strchr("ABCN", value[0]) == NULL) return fail(why, cap, "the time of the file is not A, B, C or N");
                f->letter = value[0];
                has_time = 1;
            } else if (strcmp(k, "nodes") == 0) {
                if (!is_unsigned(value, &f->nodes) || f->nodes == 0) return fail(why, cap, "the node count of the file is not a number over 0");
                has_nodes = 1;
            }
        }
        if (has_time && !has_nodes) return fail(why, cap, "the track line has a time but no node count");
        if (!has_time) f->nodes = 0;   // a places file: no time, no nodes
        return 1;
    }
    return fail(why, cap, "not a light file: no 'track' line");
}

static int classify(const LightFile *f, const char *text)
{
    char word[TOKEN];
    const char *p = text;
    if (next_token(&p, word) != 1) return LIGHT_KIND_OTHER;
    if (f->letter == 0) {   // a places file: its ranges are the window's; `present`, `snow` and the rest are not
        if (strcmp(word, "tunnel") == 0 || strcmp(word, "canyon") == 0 || strcmp(word, "car_dim") == 0) return parse_place(text, NULL) ? LIGHT_KIND_PLACE : LIGHT_KIND_BAD;
        return LIGHT_KIND_OTHER;
    }
    if (strcmp(word, "brightness") == 0) return parse_brightness(text, f->nodes, NULL) ? LIGHT_KIND_BRIGHTNESS : LIGHT_KIND_BAD;
    if (strcmp(word, "lamp") == 0) return parse_lamp(text, f->nodes, NULL) ? LIGHT_KIND_LAMP : LIGHT_KIND_BAD;
    if (strcmp(word, "tunnel_tone") == 0) return parse_tunnel_tone(text, NULL) ? LIGHT_KIND_TUNNEL_TONE : LIGHT_KIND_BAD;
    if (strcmp(word, "tone") == 0) return parse_tone(text, f->nodes, NULL) ? LIGHT_KIND_TONE : LIGHT_KIND_BAD;
    if (strcmp(word, "car_tone") == 0) return parse_car_tone(text, f->nodes, NULL) ? LIGHT_KIND_CAR_TONE : LIGHT_KIND_BAD;
    if (strcmp(word, "tunnel") == 0) return parse_zone(text, f->nodes, NULL) ? LIGHT_KIND_ZONE : LIGHT_KIND_BAD;
    return LIGHT_KIND_OTHER;
}

int light_file_parse(const char *text, size_t size, LightFile *out, char *why, size_t cap)
{
    memset(out, 0, sizeof *out);
    size_t at = 0;
    while (at < size) {
        const char *nl = memchr(text + at, '\n', size - at);
        const size_t end = nl != NULL ? (size_t)(nl - text) : size;
        if (!push_line(out, text + at, end - at)) {
            light_file_free(out);
            return fail(why, cap, "out of memory");
        }
        out->final_newline = nl != NULL;
        at = nl != NULL ? end + 1 : size;
    }
    if (out->count > 0) {
        const char *first = out->lines[0].text;
        const size_t n = strlen(first);
        out->crlf = n > 0 && first[n - 1] == '\r';
    }
    if (!read_header(out, why, cap)) {
        light_file_free(out);
        return 0;
    }
    for (int i = 0; i < out->count; ++i) out->lines[i].kind = classify(out, out->lines[i].text);
    return 1;
}

char *light_file_format(const LightFile *f, size_t *size)
{
    size_t total = 0;
    for (int i = 0; i < f->count; ++i) total += strlen(f->lines[i].text) + 1;
    if (f->count > 0 && !f->final_newline) --total;
    char *out = malloc(total + 1);
    if (out == NULL) return NULL;
    size_t at = 0;
    for (int i = 0; i < f->count; ++i) {
        const size_t n = strlen(f->lines[i].text);
        memcpy(out + at, f->lines[i].text, n);
        at += n;
        if (i + 1 < f->count || f->final_newline) out[at++] = '\n';
    }
    out[at] = '\0';
    if (size != NULL) *size = at;
    return out;
}

int light_file_count(const LightFile *f, int kind)
{
    int n = 0;
    for (int i = 0; i < f->count; ++i) n += f->lines[i].kind == kind;
    return n;
}

int light_file_first_bad(const LightFile *f)
{
    for (int i = 0; i < f->count; ++i)
        if (f->lines[i].kind == LIGHT_KIND_BAD) return i + 1;
    return 0;
}

// the index in `lines` of the i-th line of a kind, -1 for none
static int find_line(const LightFile *f, int kind, int i)
{
    if (i < 0) return -1;
    for (int at = 0; at < f->count; ++at)
        if (f->lines[at].kind == kind && i-- == 0) return at;
    return -1;
}

int light_file_get_brightness(const LightFile *f, int i, LightBrightness *out)
{
    const int at = find_line(f, LIGHT_KIND_BRIGHTNESS, i);
    return at >= 0 && parse_brightness(f->lines[at].text, f->nodes, out);
}

int light_file_get_lamp(const LightFile *f, int i, LightLamp *out)
{
    const int at = find_line(f, LIGHT_KIND_LAMP, i);
    return at >= 0 && parse_lamp(f->lines[at].text, f->nodes, out);
}

int light_file_get_tunnel_tone(const LightFile *f, int i, LightTunnelTone *out)
{
    const int at = find_line(f, LIGHT_KIND_TUNNEL_TONE, i);
    return at >= 0 && parse_tunnel_tone(f->lines[at].text, out);
}

int light_file_get_tone(const LightFile *f, int i, LightTone *out)
{
    const int at = find_line(f, LIGHT_KIND_TONE, i);
    return at >= 0 && parse_tone(f->lines[at].text, f->nodes, out);
}

int light_file_get_car_tone(const LightFile *f, int i, LightCarTone *out)
{
    const int at = find_line(f, LIGHT_KIND_CAR_TONE, i);
    return at >= 0 && parse_car_tone(f->lines[at].text, f->nodes, out);
}

int light_file_get_zone(const LightFile *f, int i, LightZone *out)
{
    const int at = find_line(f, LIGHT_KIND_ZONE, i);
    return at >= 0 && parse_zone(f->lines[at].text, f->nodes, out);
}

int light_file_get_place(const LightFile *f, int i, LightPlace *out)
{
    const int at = find_line(f, LIGHT_KIND_PLACE, i);
    return at >= 0 && parse_place(f->lines[at].text, out);
}

// ---- edits --------------------------------------------------------------------------------------------------------------------------------------------------------

// `text` (no line end) becomes the i-th line of `kind`, or a new last line for i = -1; the file's line ending is kept. 1, or 0 for no such line or no memory
static int put_line(LightFile *f, int kind, int i, const char *text)
{
    size_t n = strlen(text);
    char *copy = malloc(n + 2);
    if (copy == NULL) return 0;
    memcpy(copy, text, n);
    if (f->crlf) copy[n++] = '\r';
    copy[n] = '\0';
    if (i < 0) {
        if (!push_line(f, copy, n)) {
            free(copy);
            return 0;
        }
        f->lines[f->count - 1].kind = kind;
        free(copy);
        return 1;
    }
    const int at = find_line(f, kind, i);
    if (at < 0) {
        free(copy);
        return 0;
    }
    free(f->lines[at].text);
    f->lines[at].text = copy;
    return 1;
}

int light_file_set_brightness(LightFile *f, int i, const LightBrightness *v, char *why, size_t cap)
{
    if (f->letter == 0) return fail(why, cap, "a places file has no brightness lines");
    if (i >= light_file_count(f, LIGHT_KIND_BRIGHTNESS) || i < -1) return fail(why, cap, "there is no such brightness line");
    if (v->from > v->to) return fail(why, cap, "a brightness run goes from a node to a later one");
    if (v->to >= f->nodes) {
        char text[LIGHT_WHY];
        snprintf(text, sizeof text, "the road has nodes 0 to %u: the run ends past it", f->nodes - 1);
        return fail(why, cap, text);
    }
    if (v->percent > LIGHT_MAX_BRIGHTNESS_PERCENT) return fail(why, cap, "the most brightness is 1000 percent");
    if (i < 0 && light_file_count(f, LIGHT_KIND_BRIGHTNESS) >= LIGHT_MAX_BRIGHTNESS) return fail(why, cap, "the game takes 64 brightness lines at most");
    if (v->side < LIGHT_SIDE_BOTH || v->side > LIGHT_SIDE_RIGHT) return fail(why, cap, "a brightness line acts on both sides, the left or the right");
    char text[TOKEN * 3];
    snprintf(text, sizeof text, "brightness %u %u %u%s%s", v->from, v->to, v->percent, v->side == LIGHT_SIDE_LEFT ? " left" : v->side == LIGHT_SIDE_RIGHT ? " right" : "", v->shade ? " shade" : "");
    return put_line(f, LIGHT_KIND_BRIGHTNESS, i, text) || fail(why, cap, "out of memory");
}

int light_file_set_lamp(LightFile *f, int i, const LightLamp *v, char *why, size_t cap)
{
    if (f->letter != 'N') return fail(why, cap, "a lamp is for the night: in a dawn, day or dusk file it does nothing");
    if (i >= light_file_count(f, LIGHT_KIND_LAMP) || i < -1) return fail(why, cap, "there is no such lamp line");
    if (v->node >= f->nodes) return fail(why, cap, "the lamp stands past the last node of the road");
    if (v->percent < 1 || v->percent > LIGHT_MAX_LAMP_PERCENT) return fail(why, cap, "a lamp's light is 1 to 1000 percent");
    if (v->has_colour && !is_lamp_colour(v->colour)) return fail(why, cap, "a lamp's colour is six hex digits, then an optional :strength 0 to 100");
    if (v->has_radius && (v->radius < 1 || v->radius > LIGHT_MAX_LAMP_RADIUS)) return fail(why, cap, "a lamp's radius is 1 to 64 nodes");
    if (i < 0 && light_file_count(f, LIGHT_KIND_LAMP) >= LIGHT_MAX_LAMPS) return fail(why, cap, "the game takes 2048 lamps at most");
    char text[TOKEN * 3];
    int n = snprintf(text, sizeof text, "lamp %u %u", v->node, v->percent);
    if (v->has_colour) n += snprintf(text + n, sizeof text - (size_t)n, " %s", v->colour);
    if (v->has_radius) snprintf(text + n, sizeof text - (size_t)n, " %u", v->radius);
    return put_line(f, LIGHT_KIND_LAMP, i, text) || fail(why, cap, "out of memory");
}

int light_file_set_tunnel_tone(LightFile *f, int i, const LightTunnelTone *v, char *why, size_t cap)
{
    if (f->letter == 0) return fail(why, cap, "a places file has no tunnel look");
    if (i >= light_file_count(f, LIGHT_KIND_TUNNEL_TONE) || i < -1) return fail(why, cap, "there is no such tunnel look line");
    if (i < 0 && light_file_count(f, LIGHT_KIND_TUNNEL_TONE) >= 1) return fail(why, cap, "a file has one tunnel look: change it instead of adding a second");
    if (!(v->light >= 0.0 && v->light <= LIGHT_MAX_TUNNEL_LIGHT)) return fail(why, cap, "a tunnel's light is 0 to 10000 percent");
    if (v->has_colour && !is_look_colour(v->colour)) return fail(why, cap, "a tunnel's colour is six hex digits or r/g/b (three factors 0 to 1), then an optional :strength");
    char text[TOKEN * 2];
    const int n = snprintf(text, sizeof text, "tunnel_tone %.10g", v->light);
    if (v->has_colour) snprintf(text + n, sizeof text - (size_t)n, " %s", v->colour);
    return put_line(f, LIGHT_KIND_TUNNEL_TONE, i, text) || fail(why, cap, "out of memory");
}

// a run FROM..TO of a time's file: in order, inside the road
static int check_run(const LightFile *f, unsigned from, unsigned to, char *why, size_t cap)
{
    if (from > to) return fail(why, cap, "a run goes from a node to a later one");
    if (to >= f->nodes) {
        char text[LIGHT_WHY];
        snprintf(text, sizeof text, "the road has nodes 0 to %u: the run ends past it", f->nodes - 1);
        return fail(why, cap, text);
    }
    return 1;
}

static int tone_colour_ok(const char *colour)
{
    return strlen(colour) == 6 && is_hex6(colour);
}

// the number of different (colour, percent) among the tone lines, not counting the line `skip` (-1 for none), and whether (colour, percent) is one of them
static int distinct_tones(const LightFile *f, int skip, const char *colour, unsigned percent, int *has)
{
    LightTone seen[LIGHT_MAX_TONES];
    int n = 0, index = 0;
    *has = 0;
    for (int at = 0; at < f->count && n < LIGHT_MAX_TONES; ++at) {
        LightTone t;
        if (f->lines[at].kind != LIGHT_KIND_TONE) continue;
        if (index++ == skip || !parse_tone(f->lines[at].text, f->nodes, &t)) continue;
        int known = 0;
        for (int k = 0; k < n; ++k) known |= strcmp(seen[k].colour, t.colour) == 0 && seen[k].percent == t.percent;
        if (!known) seen[n++] = t;
        if (strcmp(t.colour, colour) == 0 && t.percent == percent) *has = 1;
    }
    return n;
}

int light_file_set_tone(LightFile *f, int i, const LightTone *v, char *why, size_t cap)
{
    if (f->letter == 0) return fail(why, cap, "a places file has no tone lines");
    if (i >= light_file_count(f, LIGHT_KIND_TONE) || i < -1) return fail(why, cap, "there is no such tone line");
    if (!check_run(f, v->from, v->to, why, cap)) return 0;
    if (v->percent > 100) return fail(why, cap, "a tone's strength is 0 to 100 percent");
    if (!tone_colour_ok(v->colour)) return fail(why, cap, "a tone's colour is six hex digits");
    if (v->mask >= (1u << LIGHT_POINTS)) return fail(why, cap, "a row has the points p0 to p10");
    if (i < 0 && light_file_count(f, LIGHT_KIND_TONE) >= LIGHT_MAX_TONES) return fail(why, cap, "the game takes 64 tone lines at most");
    char colour[8];
    for (int k = 0; k < 7; ++k) colour[k] = (char)tolower((unsigned char)v->colour[k]);
    int has = 0;
    if (distinct_tones(f, i, colour, v->percent, &has) >= LIGHT_MAX_DISTINCT_TONES && !has) return fail(why, cap, "a track has 16 distinct tones at most: reuse a colour and strength already in a tone line");
    char text[TOKEN * 2];
    int n = snprintf(text, sizeof text, "tone %u %u", v->from, v->to);
    if (v->mask != 0 && v->mask != (1u << LIGHT_POINTS) - 1u)
        for (unsigned p = 0; p < LIGHT_POINTS; ++p)
            if (v->mask & (1u << p)) n += snprintf(text + n, sizeof text - (size_t)n, " p%u", p);
    snprintf(text + n, sizeof text - (size_t)n, " %s:%u", colour, v->percent);
    return put_line(f, LIGHT_KIND_TONE, i, text) || fail(why, cap, "out of memory");
}

int light_file_set_car_tone(LightFile *f, int i, const LightCarTone *v, char *why, size_t cap)
{
    if (f->letter == 0) return fail(why, cap, "a places file has no car tone lines");
    if (i >= light_file_count(f, LIGHT_KIND_CAR_TONE) || i < -1) return fail(why, cap, "there is no such car tone line");
    if (v->has_nodes && !check_run(f, v->from, v->to, why, cap)) return 0;
    if (v->percent > 100) return fail(why, cap, "a car tone's strength is 0 to 100 percent");
    if (!tone_colour_ok(v->colour)) return fail(why, cap, "a car tone's colour is six hex digits");
    if (i < 0 && light_file_count(f, LIGHT_KIND_CAR_TONE) >= LIGHT_MAX_TONES) return fail(why, cap, "the game takes 64 car tone lines at most");
    char colour[8];
    for (int k = 0; k < 7; ++k) colour[k] = (char)tolower((unsigned char)v->colour[k]);
    char text[TOKEN * 2];
    if (v->has_nodes) snprintf(text, sizeof text, "car_tone %u %u %s:%u", v->from, v->to, colour, v->percent);
    else snprintf(text, sizeof text, "car_tone %s:%u", colour, v->percent);
    return put_line(f, LIGHT_KIND_CAR_TONE, i, text) || fail(why, cap, "out of memory");
}

int light_file_set_zone(LightFile *f, int i, const LightZone *v, char *why, size_t cap)
{
    if (f->letter == 0) return fail(why, cap, "a places file has no zone lines: its tunnels are places");
    if (i >= light_file_count(f, LIGHT_KIND_ZONE) || i < -1) return fail(why, cap, "there is no such zone line");
    if (!check_run(f, v->from, v->to, why, cap)) return 0;
    if (v->lit && (v->has_light || v->has_colour)) return fail(why, cap, "a hall is lit by its own lamps: it has no light or colour of its own");
    if (v->has_light && !(v->light >= 0.0 && v->light <= LIGHT_MAX_TUNNEL_LIGHT)) return fail(why, cap, "a zone's light is 0 to 10000 percent");
    if (v->has_colour && !is_look_colour(v->colour)) return fail(why, cap, "a zone's colour is six hex digits or r/g/b (three factors 0 to 1), then an optional :strength");
    if (i < 0 && light_file_count(f, LIGHT_KIND_ZONE) >= LIGHT_MAX_TONES) return fail(why, cap, "the game takes 64 zone lines at most");
    char text[TOKEN * 3];
    int n = snprintf(text, sizeof text, "tunnel %u %u", v->from, v->to);
    if (v->lit) n += snprintf(text + n, sizeof text - (size_t)n, " lit");
    if (v->has_light) n += snprintf(text + n, sizeof text - (size_t)n, " %.10g", v->light);
    if (v->has_colour) snprintf(text + n, sizeof text - (size_t)n, " %s", v->colour);
    return put_line(f, LIGHT_KIND_ZONE, i, text) || fail(why, cap, "out of memory");
}

int light_file_set_place(LightFile *f, int i, const LightPlace *v, unsigned nodes, char *why, size_t cap)
{
    if (f->letter != 0) return fail(why, cap, "only a places file has places lines (tunnel, canyon, car_dim)");
    if (i >= light_file_count(f, LIGHT_KIND_PLACE) || i < -1) return fail(why, cap, "there is no such places line");
    if (strcmp(v->what, "tunnel") != 0 && strcmp(v->what, "canyon") != 0 && strcmp(v->what, "car_dim") != 0) return fail(why, cap, "a place is a tunnel, a canyon or car_dim");
    if (v->from > v->to) return fail(why, cap, "a place goes from a node to a later one");
    if (nodes == 0 || v->to >= nodes) return fail(why, cap, "the road ends before that node: the place runs past it");
    char text[TOKEN];
    snprintf(text, sizeof text, "%s %u %u", v->what, v->from, v->to);
    return put_line(f, LIGHT_KIND_PLACE, i, text) || fail(why, cap, "out of memory");
}

int light_file_remove(LightFile *f, int kind, int i)
{
    if (kind < 0 || kind >= LIGHT_KIND_OWNED) return 0;
    const int at = find_line(f, kind, i);
    if (at < 0) return 0;
    free(f->lines[at].text);
    memmove(&f->lines[at], &f->lines[at + 1], sizeof *f->lines * (size_t)(f->count - at - 1));
    --f->count;
    return 1;
}

/* A track's light file in the new format, held as its lines (light_track.h). The rules of a line are the game's parser's (game/race/track_light_track.c); the whole file is checked after each
 * change, so a change that would make the game refuse the file is not made. */
#include "light_track.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WORD = 48, MAX_WORDS = 20, K_TIME = LT_BAD + 1, K_TRACK, K_SNOW_MAP };   /* kinds the panel does not edit but the file model must know */

typedef struct { char w[MAX_WORDS][WORD]; int n; } Words;

static int fail(char *why, size_t cap, const char *text)
{
    if (why != NULL && cap > 0) snprintf(why, cap, "%s", text);
    return 0;
}

static int time_index(char c) { return c == 'A' ? 0 : c == 'B' ? 1 : c == 'C' ? 2 : c == 'N' ? 3 : -1; }

// ---- words ----------------------------------------------------------------------------------------------------------------------------------------------------------

// the words of a line before its comment; 0 when there are more than MAX_WORDS or a word is longer than WORD - 1
static int split(const char *text, Words *out)
{
    out->n = 0;
    const char *p = text;
    while (*p != '\0' && *p != '#') {
        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (*p == '\0' || *p == '#') break;
        size_t n = 0;
        while (p[n] != '\0' && p[n] != ' ' && p[n] != '\t' && p[n] != '\r' && p[n] != '#') ++n;
        if (n >= WORD || out->n >= MAX_WORDS) return 0;
        memcpy(out->w[out->n], p, n);
        out->w[out->n][n] = '\0';
        out->n++;
        p += n;
    }
    return 1;
}

static int number(const char *word, unsigned *value)
{
    if (!isdigit((unsigned char)word[0]) || strlen(word) > 8) return 0;
    unsigned v = 0;
    for (const char *c = word; *c != '\0'; ++c) {
        if (!isdigit((unsigned char)*c)) return 0;
        v = v * 10 + (unsigned)(*c - '0');
    }
    *value = v;
    return 1;
}

// a number as the game reads `brightness` and `rows` (sscanf %u): a sign is taken and a negative number wraps, so `-4294967295` is 1
static int number_u(const char *word, unsigned *value)
{
    const char *digits = word + (word[0] == '+' || word[0] == '-');
    if (!isdigit((unsigned char)digits[0]) || strlen(word) > 20) return 0;
    char *end = NULL;
    const unsigned long v = strtoul(word, &end, 10);
    if (*end != '\0') return 0;
    *value = (unsigned)v;
    return 1;
}

static int real(const char *word, double *value)
{
    char *end = NULL;
    const double v = strtod(word, &end);
    if (end == word || *end != '\0' || !(v == v)) return 0;
    *value = v;
    return 1;
}

static int is_hex6(const char *s)
{
    for (int i = 0; i < 6; ++i)
        if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

// a word of 6 hex digits (then `:strength` perhaps), or r/g/b: a colour, never a node
static int colour_word(const char *word)
{
    return strchr(word, '/') != NULL || (strspn(word, "0123456789abcdefABCDEF") == 6 && (word[6] == '\0' || word[6] == ':'));
}

static int is_strength(const char *s)
{
    double v;
    return real(s, &v) && v >= 0.0 && v <= 100.0;
}

// a tunnel look's colour as the game takes it: RRGGBB or r/g/b (three factors 0..1), then an optional :strength 0..100
static int is_look_colour(const char *token)
{
    const size_t colon = strcspn(token, ":");
    if (token[colon] == ':' && !is_strength(token + colon + 1)) return 0;
    if (memchr(token, '/', colon) != NULL) {
        char part[WORD];
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

static int is_lamp_colour(const char *token)
{
    const size_t len = strlen(token);
    if (len < 6 || !is_hex6(token)) return 0;
    if (len == 6) return 1;
    return token[6] == ':' && len >= 8 && len <= 10 && is_strength(token + 7);
}

// the strip at word *i, if there is one: its points in `mask` (every point when there is none) and *i past it. 0 for `pA pB` with B before A.
static int strip_at(const Words *w, int *i, unsigned *mask)
{
    *mask = LT_ALL_POINTS;
    if (*i >= w->n) return 1;
    const char *s = w->w[*i];
    unsigned a, b;
    if (strcmp(s, "all") == 0) return ++*i, 1;
    if (strcmp(s, "left") == 0) return *mask = LT_STRIP_LEFT, ++*i, 1;
    if (strcmp(s, "right") == 0) return *mask = LT_STRIP_RIGHT, ++*i, 1;
    if (strcmp(s, "road") == 0) return *mask = LT_STRIP_ROAD, ++*i, 1;
    if (s[0] != 'p' || !number(s + 1, &a) || a >= LT_POINTS) return 1;   // not a strip: the next part of the line
    ++*i;
    b = a;
    if (*i < w->n && w->w[*i][0] == 'p' && number(w->w[*i] + 1, &b) && b < LT_POINTS) ++*i;
    else b = a;
    if (b < a) return 0;
    *mask = 0;
    for (unsigned p = a; p <= b; p++) *mask |= 1u << p;
    return 1;
}

// a strip's words, or NULL when the mask is not one the file can say
static const char *strip_text(unsigned mask, char *buf, size_t cap)
{
    mask &= LT_ALL_POINTS;
    if (mask == LT_ALL_POINTS) return "";
    if (mask == LT_STRIP_LEFT) return "left";
    if (mask == LT_STRIP_RIGHT) return "right";
    if (mask == LT_STRIP_ROAD) return "road";
    unsigned a = 0;
    while (a < LT_POINTS && !((mask >> a) & 1)) ++a;
    unsigned b = a;
    while (b + 1 < LT_POINTS && ((mask >> (b + 1)) & 1)) ++b;
    if (mask == 0 || mask != (((1u << (b + 1)) - 1) & ~((1u << a) - 1))) return NULL;
    if (a == b) snprintf(buf, cap, "p%u", a);
    else snprintf(buf, cap, "p%u p%u", a, b);
    return buf;
}

// ---- the lines of the grammar: each returns NULL when well-formed, else what is wrong ------------------------------------------------------------------------------------

static const char *p_tunnel(const Words *w, unsigned nodes, LightTrackTunnel *out)
{
    LightTrackTunnel v = { 0, 0, 0 };
    if (w->n < 3 || w->n > 4 || !number(w->w[1], &v.from) || !number(w->w[2], &v.to)) return "tunnel FROM TO [lit]";
    if (w->n == 4) {
        if (strcmp(w->w[3], "lit") != 0) return "tunnel FROM TO [lit]";
        v.lit = 1;
    }
    if (v.from > v.to || v.to >= nodes) return "the nodes of a tunnel are in order and inside the track";
    if (out != NULL) *out = v;
    return NULL;
}

static const char *p_snow(const Words *w, int *on)
{
    if (w->n != 2 || (strcmp(w->w[1], "0") != 0 && strcmp(w->w[1], "1") != 0)) return "snow 0 or snow 1";
    if (on != NULL) *on = w->w[1][0] == '1';
    return NULL;
}

static const char *p_sun(const Words *w, LightTrackSun *out)
{
    LightTrackSun v;
    memset(&v, 0, sizeof v);
    if (w->n != 4 && w->n != 5) return "sun AZIMUTH ELEVATION SHADOW [REACH] (a part may be -)";
    int *has[3] = { &v.has_azimuth, &v.has_elevation, &v.has_shadow };
    double *val[3] = { &v.azimuth, &v.elevation, &v.shadow };
    for (int i = 0; i < 3; i++) {
        if (strcmp(w->w[i + 1], "-") == 0) continue;
        if (!real(w->w[i + 1], val[i])) return "sun AZIMUTH ELEVATION SHADOW [REACH] (a part may be -)";
        *has[i] = 1;
    }
    if (v.has_elevation && (v.elevation < -90.0 || v.elevation > 90.0)) return "the sun's elevation is -90..90 degrees";
    if (v.has_shadow && (v.shadow < 0.0 || v.shadow > 100.0)) return "the sun's shadow is 0..100 percent";
    if (w->n == 5 && strcmp(w->w[4], "-") != 0) {
        if (!real(w->w[4], &v.reach) || !(v.reach >= 1.0 && v.reach <= LT_MAX_SUN_REACH)) return "the sun's reach is 1..2000 units (or -)";
        v.has_reach = 1;
    }
    if (out != NULL) *out = v;
    return NULL;
}

// `fact` (a byte per node, 0 outside the tunnels) may be NULL: the look's nodes are then not checked against the tunnels
static const char *p_look(const Words *w, unsigned nodes, const unsigned char *fact, LightTrackLook *out)
{
    LightTrackLook v;
    memset(&v, 0, sizeof v);
    int i = 1;
    const int count = w->n;
    unsigned a = 0, b = 0;
    if (count >= 3 && !colour_word(w->w[1]) && !colour_word(w->w[2]) && number(w->w[1], &a) && number(w->w[2], &b)) {
        if (a > b || b >= nodes) return "the nodes of a look are in order and inside the track";
        if (fact != NULL)
            for (unsigned n = a; n <= b; n++)
                if (fact[n] == 0) return "a tunnel_look needs its nodes inside a tunnel of the facts";
        v.has_nodes = 1, v.from = a, v.to = b, i = 3;
    }
    if (i < count && !colour_word(w->w[i]) && isdigit((unsigned char)w->w[i][0])) {
        if (!real(w->w[i], &v.light) || !(v.light >= 0.0 && v.light <= LT_MAX_LOOK_LIGHT)) return "the light of a look is 0..10000 percent";
        v.has_light = 1, i++;
    }
    if (i < count && colour_word(w->w[i])) {
        if (!is_look_colour(w->w[i])) return "a colour is RRGGBB or r/g/b, then :strength 0..100";
        v.has_colour = 1;
        memcpy(v.colour, w->w[i], strlen(w->w[i]) + 1);
        i++;
    }
    while (i < count) {
        if (strcmp(w->w[i], "lamps") == 0 && !v.has_lamps && i + 1 < count) {
            if (!real(w->w[i + 1], &v.lamps) || !(v.lamps >= 0.0 && v.lamps <= LT_MAX_LOOK_LAMPS)) return "the lamps' strength is 0..1000 percent";
            v.has_lamps = 1, i += 2;
            if (i < count && colour_word(w->w[i])) {
                if (!is_look_colour(w->w[i])) return "a colour is RRGGBB or r/g/b, then :strength 0..100";
                v.has_lamp_colour = 1;
                memcpy(v.lamp_colour, w->w[i], strlen(w->w[i]) + 1);
                i++;
            }
        } else if (strcmp(w->w[i], "reach") == 0 && !v.has_reach && i + 1 < count) {
            if (!real(w->w[i + 1], &v.reach) || !(v.reach >= 0.5 && v.reach <= 4.0)) return "the lamps' reach is 0.5..4 nodes";
            v.has_reach = 1, i += 2;
        } else {
            return "tunnel_look [FROM TO] [LIGHT] [COLOUR] [lamps S [COLOUR]] [reach R]";
        }
    }
    if (!v.has_light && !v.has_colour && !v.has_lamps) return "a tunnel_look says a light, a colour or lamps";
    if (v.has_colour && !v.has_light && !v.has_nodes) return "the default look's colour comes with its light";
    if (out != NULL) *out = v;
    return NULL;
}

static const char *p_lamp(const Words *w, unsigned nodes, LightTrackLamp *out)
{
    LightTrackLamp v;
    memset(&v, 0, sizeof v);
    if (w->n < 3 || !number(w->w[1], &v.node) || !number(w->w[2], &v.percent)) return "lamp NODE PERCENT [COLOUR[:STRENGTH]] [RADIUS]";
    if (v.node >= nodes) return "the lamp's node is inside the track";
    if (v.percent < 1 || v.percent > LT_MAX_LAMP_PERCENT) return "the lamp's light is 1..1000 percent";
    for (int i = 3; i < w->n; i++) {
        const char *t = w->w[i];
        const size_t len = strlen(t);
        if (len >= 6 && !v.has_colour && !v.has_radius) {
            if (!is_lamp_colour(t) || len >= sizeof v.colour) return "a lamp's colour is RRGGBB or RRGGBB:strength";
            v.has_colour = 1;
            memcpy(v.colour, t, len + 1);
        } else if (len <= 2 && !v.has_radius) {
            if (!number(t, &v.radius) || v.radius < 1 || v.radius > LT_MAX_LAMP_RADIUS) return "the lamp's radius is 1..64 nodes";
            v.has_radius = 1;
        } else {
            return "lamp NODE PERCENT [COLOUR[:STRENGTH]] [RADIUS]";
        }
    }
    if (out != NULL) *out = v;
    return NULL;
}

static const char *p_brightness(const Words *w, unsigned nodes, LightTrackBrightness *out)
{
    LightTrackBrightness v;
    memset(&v, 0, sizeof v);
    if (w->n < 4 || !number_u(w->w[1], &v.from) || !number_u(w->w[2], &v.to) || !number_u(w->w[3], &v.percent)) return "brightness FROM TO PERCENT [STRIP]";
    if (v.from > v.to || v.to >= nodes) return "the nodes of a brightness are in order and inside the track";
    if (v.percent > LT_MAX_BRIGHTNESS_PERCENT) return "a brightness is 0..1000 percent";
    int i = 4;
    if (!strip_at(w, &i, &v.mask) || i != w->n) return "the strip is left, right, road, all, pA or pA pB (points 0..10, in order)";
    if (out != NULL) *out = v;
    return NULL;
}

static const char *p_tone(const Words *w, unsigned nodes, LightTrackTone *out)
{
    LightTrackTone v;
    memset(&v, 0, sizeof v);
    if (w->n < 4 || !number(w->w[1], &v.from) || !number(w->w[2], &v.to)) return "tone FROM TO [STRIP] RRGGBB:PERCENT";
    if (v.from > v.to || v.to >= nodes) return "the nodes of a tone are in order and inside the track";
    int i = 3;
    if (!strip_at(w, &i, &v.mask)) return "the strip is left, right, road, all, pA or pA pB (points 0..10, in order)";
    if (i != w->n - 1) return "tone FROM TO [STRIP] RRGGBB:PERCENT";
    const char *c = w->w[i];
    if (strlen(c) < 8 || c[6] != ':' || !is_hex6(c) || !number(c + 7, &v.percent) || v.percent > 100) return "a tone is RRGGBB:PERCENT (percent 0..100)";
    for (int k = 0; k < 6; ++k) v.colour[k] = (char)tolower((unsigned char)c[k]);
    if (out != NULL) *out = v;
    return NULL;
}

static const char *p_rows(const Words *w, unsigned nodes, LightTrackRows *out)
{
    LightTrackRows v;
    memset(&v, 0, sizeof v);
    if (w->n != 3 + LT_POINTS || !number_u(w->w[1], &v.from) || !number_u(w->w[2], &v.to)) return "rows FROM TO and 11 values";
    if (v.from > v.to || v.to >= nodes) return "the nodes of a rows line are in order and inside the track";
    for (int p = 0; p < LT_POINTS; p++) {
        unsigned x;
        if (!number_u(w->w[3 + p], &x) || x > 255) return "the values of a rows line are 0..255";
        v.value[p] = (unsigned char)x;
    }
    if (out != NULL) *out = v;
    return NULL;
}

// ---- the whole file -----------------------------------------------------------------------------------------------------------------------------------------------------

typedef struct { char colour[8]; unsigned percent; } Distinct;

// The game's tone slots (game/race/track_light_file.c, zone_slots): a time's `tone` lines take one slot for each distinct tone, and the `tunnel_look A B` lines whose look is not the time's own
// take one each too (two with the same look share one): 16 in all. The same arithmetic, so a file that needs a 17th is listed as bad here as it is refused there.
typedef struct { int line, has_light, has_colour; double light; float cast[3]; } ZoneLook;

// the factors of a look's colour as the game computes them (parse_colour): RRGGBB or r/g/b, then an optional :strength; 0 when it is neither
static int cast_of(const char *token, float cast[3])
{
    const size_t colon = strcspn(token, ":");
    double strength = 100.0, full[3];
    if (token[colon] == ':') {
        char *end = NULL;
        strength = strtod(token + colon + 1, &end);
        if (end == token + colon + 1 || *end != '\0' || strength < 0.0 || strength > 100.0) return 0;
    }
    if (memchr(token, '/', colon) != NULL) {
        char part[WORD], *at = part, *end = NULL;
        if (colon >= sizeof part) return 0;
        memcpy(part, token, colon), part[colon] = '\0';
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
    for (int k = 0; k < 3; k++) cast[k] = strength == 100.0 ? (float)full[k] : (float)(1.0 + (full[k] - 1.0) * strength / 100.0);
    return 1;
}

// the end of a time's block: the zones' slots after the tones' (`tones` distinct ones); the line of the first zone that does not fit is marked
static void zone_slots(LightTrack *f, int tones, const ZoneLook *zl, int zn, int has_light, double base_light, int has_colour, const float base_cast[3])
{
    const double light0 = has_light ? base_light : 10.0;
    const float white[3] = { 1.0f, 1.0f, 1.0f };
    const float *cast0 = has_colour ? base_cast : white;
    struct { float light; float cast[3]; } slot[LT_MAX_PER_TIME];
    int n = 0;
    for (int i = 0; i < zn; i++) {
        if (!zl[i].has_light && !zl[i].has_colour) continue;
        const double light = zl[i].has_light ? zl[i].light : light0;
        const float *cast = zl[i].has_colour ? zl[i].cast : cast0;
        if (light == light0 && memcmp(cast, cast0, 3 * sizeof(float)) == 0) continue;
        int seen = 0;
        for (int k = 0; k < n; k++)
            if (slot[k].light == (float)light && memcmp(slot[k].cast, cast, 3 * sizeof(float)) == 0) seen = 1;
        if (seen) continue;
        if (tones + n >= LT_MAX_DISTINCT_TONES) {
            if (f->lines[zl[i].line].kind != LT_BAD) f->lines[zl[i].line].kind = LT_BAD, f->lines[zl[i].line].why = "the tone slots are 16 at most: the tone lines and the tunnel_look lines with a look of their own share them";
            return;
        }
        slot[n].light = (float)light, memcpy(slot[n].cast, cast, 3 * sizeof(float)), n++;
    }
}

// Sets kind, time and why of every line, with the rules of the game's parser. 1 when the memory was enough.
static int classify(LightTrack *f)
{
    unsigned char *fact = calloc(f->nodes, 1);
    if (fact == NULL) return 0;
    int cur = LT_TIME_FACTS, header = 0, has_time[LT_TIMES] = { 0 }, halls = 0;
    int looks = 0, brights = 0, tones = 0, lamps = 0, distinct_n = 0, zones = 0;
    Distinct distinct[LT_MAX_DISTINCT_TONES];
    ZoneLook zl[LT_MAX_PER_TIME];
    int zn = 0, base_has_light = 0, base_has_colour = 0;
    double base_light = 0.0;
    float base_cast[3] = { 1.0f, 1.0f, 1.0f };
    for (int li = 0; li < f->count; li++) {
        LightTrackLine *ln = &f->lines[li];
        Words w;
        ln->kind = LT_OTHER, ln->why = NULL;
        if (!split(ln->text, &w)) { ln->kind = LT_BAD, ln->why = "a line is too long"; ln->time = cur; continue; }
        ln->time = cur;
        if (w.n == 0) continue;
        const char *key = w.w[0], *why = NULL;
        if (!header) {
            if (strcmp(key, "track") != 0 || w.n != 2) { ln->kind = LT_BAD, ln->why = "the first line is `track STEM`"; continue; }
            header = 1, ln->kind = K_TRACK;
            continue;
        }
        if (strcmp(key, "track") == 0) { ln->kind = LT_BAD, ln->why = "a second track line"; continue; }
        if (strcmp(key, "time") == 0) {
            const int t = w.n == 2 && w.w[1][1] == '\0' ? time_index(w.w[1][0]) : -1;
            if (t < 0) { ln->kind = LT_BAD, ln->why = "time A, B, C or N"; continue; }
            if (has_time[t]) { ln->kind = LT_BAD, ln->why = "a time appears once"; continue; }
            if (cur != LT_TIME_FACTS) zone_slots(f, distinct_n, zl, zn, base_has_light, base_light, base_has_colour, base_cast);
            has_time[t] = 1, cur = t, ln->time = t, ln->kind = K_TIME;
            looks = brights = tones = lamps = distinct_n = zones = zn = base_has_light = base_has_colour = 0;
            continue;
        }
        if (cur == LT_TIME_FACTS) {
            if (strcmp(key, "tunnel") == 0) {
                LightTrackTunnel t;
                why = p_tunnel(&w, f->nodes, &t);
                if (why == NULL && t.lit && ++halls > LT_MAX_PER_TIME) why = "too many halls (64)";
                if (why == NULL) {
                    for (unsigned n = t.from; n <= t.to; n++) fact[n] = t.lit ? 2 : 1;
                    ln->kind = LT_TUNNEL;
                }
            } else if (strcmp(key, "snow") == 0) {
                why = p_snow(&w, NULL);
                if (why == NULL) ln->kind = LT_SNOW;
            } else if (strcmp(key, "snow_map") == 0) {
                unsigned a, b;
                if (w.n != 3 || !number(w.w[1], &a) || !number(w.w[2], &b) || a >= LT_SNOW_SLOTS || b >= LT_SNOW_SLOTS) why = "snow_map FROM TO (slots 0..143)";
                else ln->kind = K_SNOW_MAP;
            } else {
                why = "only facts (tunnel, snow, snow_map) before the first time";
            }
        } else if (strcmp(key, "sun") == 0) {
            why = p_sun(&w, NULL);
            if (why == NULL) ln->kind = LT_SUN;
        } else if (strcmp(key, "tunnel_look") == 0) {
            LightTrackLook look;
            why = p_look(&w, f->nodes, fact, &look);
            if (why == NULL && ++looks > LT_MAX_PER_TIME) why = "too many tunnel_look lines in a time (64)";
            if (why == NULL && look.has_nodes && halls + ++zones > LT_MAX_PER_TIME) why = "the halls and the tunnel_look lines with nodes of a time are 64 at most together";
            if (why == NULL) {
                float cast[3] = { 1.0f, 1.0f, 1.0f };
                const int has_colour = look.has_colour && cast_of(look.colour, cast);
                if (look.has_nodes) {
                    zl[zn++] = (ZoneLook){ li, look.has_light, has_colour, look.light, { cast[0], cast[1], cast[2] } };
                } else {   // the time's own look: a second that differs is refused (take_look)
                    if (look.has_light && base_has_light && base_light != look.light) why = "the time's own tunnel_look says two different lights";
                    else if (has_colour && base_has_colour && memcmp(base_cast, cast, sizeof cast) != 0) why = "the time's own tunnel_look says two different colours";
                    else {
                        if (look.has_light) base_has_light = 1, base_light = look.light;
                        if (has_colour) base_has_colour = 1, memcpy(base_cast, cast, sizeof cast);
                    }
                }
            }
            if (why == NULL) ln->kind = LT_LOOK;
        } else if (strcmp(key, "lamp") == 0) {
            why = p_lamp(&w, f->nodes, NULL);
            if (why == NULL && ++lamps > LT_MAX_LAMPS) why = "too many lamps in a time (2048)";
            if (why == NULL) ln->kind = LT_LAMP;
        } else if (strcmp(key, "brightness") == 0) {
            why = p_brightness(&w, f->nodes, NULL);
            if (why == NULL && ++brights > LT_MAX_PER_TIME) why = "too many brightness lines in a time (64)";
            if (why == NULL) ln->kind = LT_BRIGHTNESS;
        } else if (strcmp(key, "tone") == 0) {
            LightTrackTone t;
            why = p_tone(&w, f->nodes, &t);
            if (why == NULL && ++tones > LT_MAX_PER_TIME) why = "too many tone lines in a time (64)";
            if (why == NULL) {
                int seen = 0;
                for (int k = 0; k < distinct_n; k++)
                    if (strcmp(distinct[k].colour, t.colour) == 0 && distinct[k].percent == t.percent) seen = 1;
                if (!seen) {
                    if (distinct_n >= LT_MAX_DISTINCT_TONES) why = "more than 16 different tones in a time";
                    else memcpy(distinct[distinct_n].colour, t.colour, 8), distinct[distinct_n++].percent = t.percent;
                }
            }
            if (why == NULL) ln->kind = LT_TONE;
        } else if (strcmp(key, "rows") == 0) {
            why = p_rows(&w, f->nodes, NULL);
            if (why == NULL) ln->kind = LT_ROWS;
        } else if (strcmp(key, "tunnel") == 0 || strcmp(key, "snow") == 0 || strcmp(key, "snow_map") == 0) {
            why = "a fact after a time line";
        } else {
            why = "a word the game does not know";
        }
        if (why != NULL) ln->kind = LT_BAD, ln->why = why;
    }
    if (cur != LT_TIME_FACTS) zone_slots(f, distinct_n, zl, zn, base_has_light, base_light, base_has_colour, base_cast);
    free(fact);
    return 1;
}

static int push_line(LightTrack *f, const char *text, size_t len)
{
    if (f->count == f->cap) {
        const int cap = f->cap == 0 ? 256 : f->cap * 2;
        LightTrackLine *lines = realloc(f->lines, sizeof *lines * (size_t)cap);
        if (lines == NULL) return 0;
        f->lines = lines;
        f->cap = cap;
    }
    char *copy = malloc(len + 1);
    if (copy == NULL) return 0;
    memcpy(copy, text, len);
    copy[len] = '\0';
    f->lines[f->count++] = (LightTrackLine){ copy, LT_OTHER, LT_TIME_FACTS, NULL };
    return 1;
}

void light_track_free(LightTrack *f)
{
    if (f == NULL) return;
    for (int i = 0; i < f->count; i++) free(f->lines[i].text);
    free(f->lines);
    memset(f, 0, sizeof *f);
}

int light_track_parse(const char *text, size_t size, unsigned nodes, LightTrack *out, char *why, size_t cap)
{
    if (text == NULL || out == NULL || nodes == 0) return fail(why, cap, "no text or no nodes");
    if (size > LT_MAX_FILE_BYTES) return fail(why, cap, "the file is over 4 MB: not a light file");
    const char *nul = memchr(text, '\0', size);
    if (nul != NULL) {
        char text_why[LT_WHY];
        snprintf(text_why, sizeof text_why, "the file has a NUL byte (byte %zu): a text file has none", (size_t)(nul - text) + 1);
        return fail(why, cap, text_why);
    }
    LightTrack f;
    memset(&f, 0, sizeof f);
    f.nodes = nodes;
    for (size_t at = 0; at < size;) {
        size_t end = at;
        while (end < size && text[end] != '\n') ++end;
        if (!push_line(&f, text + at, end - at)) { light_track_free(&f); return fail(why, cap, "out of memory"); }
        if (end == size) { f.final_newline = 0; break; }
        at = end + 1;
        f.final_newline = 1;
    }
    f.crlf = f.count > 0 && f.lines[0].text[0] != '\0' && f.lines[0].text[strlen(f.lines[0].text) - 1] == '\r';
    Words w;
    int found = 0;
    for (int i = 0; i < f.count && !found; i++) {
        if (!split(f.lines[i].text, &w) || w.n == 0) continue;
        if (strcmp(w.w[0], "track") == 0 && w.n == 2 && strlen(w.w[1]) < sizeof f.track) memcpy(f.track, w.w[1], strlen(w.w[1]) + 1), found = 1;
        else break;
    }
    if (!found) { light_track_free(&f); return fail(why, cap, "not a track light file: the first line is `track STEM`"); }
    if (!classify(&f)) { light_track_free(&f); return fail(why, cap, "out of memory"); }
    *out = f;
    return 1;
}

char *light_track_format(const LightTrack *f, size_t *size)
{
    size_t total = 1;
    for (int i = 0; i < f->count; i++) total += strlen(f->lines[i].text) + 1;
    char *out = malloc(total);
    if (out == NULL) return NULL;
    size_t at = 0;
    for (int i = 0; i < f->count; i++) {
        const size_t n = strlen(f->lines[i].text);
        memcpy(out + at, f->lines[i].text, n);
        at += n;
        if (i + 1 < f->count || f->final_newline) out[at++] = '\n';
    }
    out[at] = '\0';
    if (size != NULL) *size = at;
    return out;
}

// ---- reading ------------------------------------------------------------------------------------------------------------------------------------------------------

int light_track_count(const LightTrack *f, int time, int kind)
{
    int n = 0;
    for (int i = 0; i < f->count; i++)
        if (f->lines[i].kind == kind && (kind == LT_BAD || f->lines[i].time == time)) n++;
    return n;
}

int light_track_first_bad(const LightTrack *f)
{
    for (int i = 0; i < f->count; i++)
        if (f->lines[i].kind == LT_BAD) return i + 1;
    return 0;
}

int light_track_has_time(const LightTrack *f, int time)
{
    for (int i = 0; i < f->count; i++)
        if (f->lines[i].kind == K_TIME && f->lines[i].time == time) return 1;
    return 0;
}

static int nth(const LightTrack *f, int time, int kind, int i)
{
    if (i < 0) return -1;
    for (int k = 0; k < f->count; k++)
        if (f->lines[k].kind == kind && f->lines[k].time == time && i-- == 0) return k;
    return -1;
}

static int words_of(const LightTrack *f, int time, int kind, int i, Words *w)
{
    const int k = nth(f, time, kind, i);
    return k >= 0 && split(f->lines[k].text, w);
}

int light_track_get_tunnel(const LightTrack *f, int i, LightTrackTunnel *out)
{
    Words w;
    return words_of(f, LT_TIME_FACTS, LT_TUNNEL, i, &w) && p_tunnel(&w, f->nodes, out) == NULL;
}

int light_track_get_snow(const LightTrack *f, int *on)
{
    Words w;
    return words_of(f, LT_TIME_FACTS, LT_SNOW, 0, &w) && p_snow(&w, on) == NULL;
}

int light_track_get_sun(const LightTrack *f, int time, LightTrackSun *out)
{
    Words w;
    return words_of(f, time, LT_SUN, 0, &w) && p_sun(&w, out) == NULL;
}

int light_track_get_look(const LightTrack *f, int time, int i, LightTrackLook *out)
{
    Words w;
    return words_of(f, time, LT_LOOK, i, &w) && p_look(&w, f->nodes, NULL, out) == NULL;
}

int light_track_get_lamp(const LightTrack *f, int time, int i, LightTrackLamp *out)
{
    Words w;
    return words_of(f, time, LT_LAMP, i, &w) && p_lamp(&w, f->nodes, out) == NULL;
}

int light_track_get_brightness(const LightTrack *f, int time, int i, LightTrackBrightness *out)
{
    Words w;
    return words_of(f, time, LT_BRIGHTNESS, i, &w) && p_brightness(&w, f->nodes, out) == NULL;
}

int light_track_get_tone(const LightTrack *f, int time, int i, LightTrackTone *out)
{
    Words w;
    return words_of(f, time, LT_TONE, i, &w) && p_tone(&w, f->nodes, out) == NULL;
}

int light_track_get_rows(const LightTrack *f, int time, int i, LightTrackRows *out)
{
    Words w;
    return words_of(f, time, LT_ROWS, i, &w) && p_rows(&w, f->nodes, out) == NULL;
}

// ---- writing ------------------------------------------------------------------------------------------------------------------------------------------------------

static LightTrack *clone(const LightTrack *f)
{
    LightTrack *c = calloc(1, sizeof *c);
    if (c == NULL) return NULL;
    *c = *f;
    c->lines = NULL, c->count = c->cap = 0;
    for (int i = 0; i < f->count; i++) {
        if (!push_line(c, f->lines[i].text, strlen(f->lines[i].text))) { light_track_free(c); free(c); return NULL; }
        c->lines[i].kind = f->lines[i].kind, c->lines[i].time = f->lines[i].time, c->lines[i].why = f->lines[i].why;
    }
    return c;
}

static int bad_count(const LightTrack *f)
{
    int n = 0;
    for (int i = 0; i < f->count; i++) n += f->lines[i].kind == LT_BAD;
    return n;
}

// the text of a line with its place kept: the old line's indent, comment and "\r", or the new ones' ("  " in a block, nothing in the facts)
static char *dress(const LightTrack *f, int time, const char *old, const char *body)
{
    char indent[16] = "";
    const size_t tail_cap = old != NULL ? strlen(old) + 4 : 4;
    char *tail = calloc(tail_cap, 1);   // a comment of any length is kept whole
    if (tail == NULL) return NULL;
    if (old != NULL) {
        size_t n = strspn(old, " \t");
        if (n >= sizeof indent) n = sizeof indent - 1;
        memcpy(indent, old, n), indent[n] = '\0';
        const char *hash = strchr(old, '#');
        if (hash != NULL) {
            while (hash > old && (hash[-1] == ' ' || hash[-1] == '\t')) --hash;
            snprintf(tail, tail_cap, " %s", hash + strspn(hash, " \t"));
        } else {
            const size_t len = strlen(old);
            if (len > 0 && old[len - 1] == '\r') snprintf(tail, tail_cap, "\r");
        }
    } else {
        if (time != LT_TIME_FACTS) snprintf(indent, sizeof indent, "  ");
        if (f->crlf) snprintf(tail, tail_cap, "\r");
    }
    const size_t n = strlen(indent) + strlen(body) + strlen(tail) + 1;
    char *text = malloc(n);
    if (text != NULL) snprintf(text, n, "%s%s%s", indent, body, tail);
    free(tail);
    return text;
}

static int is_blank_line(const LightTrackLine *ln)
{
    Words w;
    return !split(ln->text, &w) ? 0 : w.n == 0;
}

// the index a new line of `time` goes to (it is inserted there); *new_block is set when the time has no block yet
static int insert_at(const LightTrack *f, int time, int *new_block)
{
    *new_block = 0;
    int first_time = f->count;
    for (int i = 0; i < f->count; i++)
        if (f->lines[i].kind == K_TIME) { first_time = i; break; }
    int start = 0, end = first_time;
    if (time != LT_TIME_FACTS) {
        start = -1;
        for (int i = 0; i < f->count; i++)
            if (f->lines[i].kind == K_TIME && f->lines[i].time == time) start = i + 1;
        if (start < 0) { *new_block = 1; return f->count; }
        end = f->count;
        for (int i = start; i < f->count; i++)
            if (f->lines[i].kind == K_TIME) { end = i; break; }
    }
    int at = start;
    for (int i = start; i < end; i++)
        if (!is_blank_line(&f->lines[i])) at = i + 1;
    return at;
}

// Replaces the i-th line of the kind with `body`, or adds one (i = -1): all on a copy, which replaces the file only if the game's rules still hold.
static int change(LightTrack *f, int time, int kind, int i, const char *body, char *why, size_t cap)
{
    if (body == NULL) return fail(why, cap, "that value cannot be written in the file");
    LightTrack *work = clone(f);
    if (work == NULL) return fail(why, cap, "out of memory");
    int pos = -1;
    if (i >= 0) {
        pos = nth(work, time, kind, i);
        if (pos < 0) { light_track_free(work); free(work); return fail(why, cap, "there is no such line"); }
        char *text = dress(work, time, work->lines[pos].text, body);
        if (text == NULL) { light_track_free(work); free(work); return fail(why, cap, "out of memory"); }
        free(work->lines[pos].text);
        work->lines[pos].text = text;
    } else {
        int new_block;
        pos = insert_at(work, time, &new_block);
        char head[16];
        snprintf(head, sizeof head, "time %c", time >= 0 ? "ABCN"[time] : 'A');
        char *lines[3] = { NULL, NULL, NULL };
        const int extra = new_block ? 3 : 1;   // a blank line, `time X`, the line
        if (new_block) {
            lines[0] = dress(work, time, NULL, ""), lines[1] = dress(work, LT_TIME_FACTS, NULL, head);
            if (lines[0] != NULL && !work->crlf) lines[0][0] = '\0';
        }
        lines[extra - 1] = dress(work, time, NULL, body);
        int good = 1;
        for (int k = 0; k < extra; k++) good = good && lines[k] != NULL;
        for (int k = 0; good && k < extra; k++) good = push_line(work, "", 0);   // room: the new lines are moved into place below
        if (!good) {
            for (int k = 0; k < extra; k++) free(lines[k]);
            light_track_free(work);
            free(work);
            return fail(why, cap, "out of memory");
        }
        for (int k = 0; k < extra; k++) free(work->lines[work->count - 1 - k].text);
        for (int k = work->count - 1 - extra; k >= pos; k--) work->lines[k + extra] = work->lines[k];
        for (int k = 0; k < extra; k++) work->lines[pos + k] = (LightTrackLine){ lines[k], LT_OTHER, LT_TIME_FACTS, NULL };
        pos += extra - 1;
        if (pos + 1 >= work->count) work->final_newline = 1;
    }
    if (!classify(work)) { light_track_free(work); free(work); return fail(why, cap, "out of memory"); }
    // the line itself must be the kind asked, and no line may have become one the game refuses
    int ok = work->lines[pos].kind == kind && bad_count(work) <= bad_count(f);
    if (!ok) {
        char text[LT_WHY];
        const char *reason = work->lines[pos].kind == LT_BAD && work->lines[pos].why != NULL ? work->lines[pos].why : NULL;
        int line = pos + 1;
        for (int k = 0; reason == NULL && k < work->count; k++)
            if (work->lines[k].kind == LT_BAD && (k >= f->count || f->lines[k].kind != LT_BAD)) reason = work->lines[k].why, line = k + 1;
        snprintf(text, sizeof text, "line %d: %s", line, reason != NULL ? reason : "the game would refuse the file");
        light_track_free(work);
        free(work);
        return fail(why, cap, text);
    }
    light_track_free(f);
    *f = *work;
    free(work);
    return 1;
}

static int kind_ok(int time, int facts)
{
    return facts ? time == LT_TIME_FACTS : time >= 0 && time < LT_TIMES;
}

int light_track_set_tunnel(LightTrack *f, int i, const LightTrackTunnel *v, char *why, size_t cap)
{
    char body[96];
    snprintf(body, sizeof body, "tunnel %u %u%s", v->from, v->to, v->lit ? " lit" : "");
    return change(f, LT_TIME_FACTS, LT_TUNNEL, i, body, why, cap);
}

int light_track_set_snow(LightTrack *f, int on, char *why, size_t cap)
{
    char body[16];
    snprintf(body, sizeof body, "snow %d", on != 0);
    return change(f, LT_TIME_FACTS, LT_SNOW, light_track_count(f, LT_TIME_FACTS, LT_SNOW) > 0 ? 0 : -1, body, why, cap);
}

static void put_real(char *body, size_t cap, double v, int has)
{
    if (has) snprintf(body + strlen(body), cap - strlen(body), " %.10g", v);
    else snprintf(body + strlen(body), cap - strlen(body), " -");
}

int light_track_set_sun(LightTrack *f, int time, const LightTrackSun *v, char *why, size_t cap)
{
    if (!kind_ok(time, 0)) return fail(why, cap, "no such time");
    char body[160] = "sun";
    put_real(body, sizeof body, v->azimuth, v->has_azimuth);
    put_real(body, sizeof body, v->elevation, v->has_elevation);
    put_real(body, sizeof body, v->shadow, v->has_shadow);
    if (v->has_reach) snprintf(body + strlen(body), sizeof body - strlen(body), " %.10g", v->reach);
    return change(f, time, LT_SUN, light_track_count(f, time, LT_SUN) > 0 ? 0 : -1, body, why, cap);
}

int light_track_set_look(LightTrack *f, int time, int i, const LightTrackLook *v, char *why, size_t cap)
{
    if (!kind_ok(time, 0)) return fail(why, cap, "no such time");
    char body[256] = "tunnel_look";
    if (v->has_nodes) snprintf(body + strlen(body), sizeof body - strlen(body), " %u %u", v->from, v->to);
    if (v->has_light) snprintf(body + strlen(body), sizeof body - strlen(body), " %.10g", v->light);
    if (v->has_colour) snprintf(body + strlen(body), sizeof body - strlen(body), " %s", v->colour);
    if (v->has_lamps) {
        snprintf(body + strlen(body), sizeof body - strlen(body), " lamps %.10g", v->lamps);
        if (v->has_lamp_colour) snprintf(body + strlen(body), sizeof body - strlen(body), " %s", v->lamp_colour);
    }
    if (v->has_reach) snprintf(body + strlen(body), sizeof body - strlen(body), " reach %.10g", v->reach);
    return change(f, time, LT_LOOK, i, body, why, cap);
}

int light_track_set_lamp(LightTrack *f, int time, int i, const LightTrackLamp *v, char *why, size_t cap)
{
    if (!kind_ok(time, 0)) return fail(why, cap, "no such time");
    char body[96];
    snprintf(body, sizeof body, "lamp %u %u", v->node, v->percent);
    if (v->has_colour) snprintf(body + strlen(body), sizeof body - strlen(body), " %s", v->colour);
    if (v->has_radius) snprintf(body + strlen(body), sizeof body - strlen(body), " %u", v->radius);
    return change(f, time, LT_LAMP, i, body, why, cap);
}

int light_track_set_brightness(LightTrack *f, int time, int i, const LightTrackBrightness *v, char *why, size_t cap)
{
    if (!kind_ok(time, 0)) return fail(why, cap, "no such time");
    char strip[16], body[96];
    const char *s = strip_text(v->mask, strip, sizeof strip);
    if (s == NULL) return fail(why, cap, "a strip is left, right, road, all or a run of points");
    snprintf(body, sizeof body, "brightness %u %u %u%s%s", v->from, v->to, v->percent, s[0] != '\0' ? " " : "", s);
    return change(f, time, LT_BRIGHTNESS, i, body, why, cap);
}

int light_track_set_tone(LightTrack *f, int time, int i, const LightTrackTone *v, char *why, size_t cap)
{
    if (!kind_ok(time, 0)) return fail(why, cap, "no such time");
    char strip[16], body[96];
    const char *s = strip_text(v->mask, strip, sizeof strip);
    if (s == NULL) return fail(why, cap, "a strip is left, right, road, all or a run of points");
    if (strlen(v->colour) != 6 || !is_hex6(v->colour)) return fail(why, cap, "a tone's colour is six hex digits");
    snprintf(body, sizeof body, "tone %u %u%s%s %s:%u", v->from, v->to, s[0] != '\0' ? " " : "", s, v->colour, v->percent);
    return change(f, time, LT_TONE, i, body, why, cap);
}

int light_track_set_rows(LightTrack *f, int time, int i, const LightTrackRows *v, char *why, size_t cap)
{
    if (!kind_ok(time, 0)) return fail(why, cap, "no such time");
    char body[160];
    snprintf(body, sizeof body, "rows %u %u", v->from, v->to);
    for (int p = 0; p < LT_POINTS; p++) snprintf(body + strlen(body), sizeof body - strlen(body), " %u", v->value[p]);
    return change(f, time, LT_ROWS, i, body, why, cap);
}

int light_track_remove(LightTrack *f, int time, int kind, int i, char *why, size_t cap)
{
    if (kind <= LT_OTHER || kind >= LT_OWNED) return fail(why, cap, "that line is not one the panel edits");
    LightTrack *work = clone(f);
    if (work == NULL) return fail(why, cap, "out of memory");
    const int pos = nth(work, time, kind, i);
    if (pos < 0) { light_track_free(work); free(work); return fail(why, cap, "there is no such line"); }
    free(work->lines[pos].text);
    for (int k = pos; k + 1 < work->count; k++) work->lines[k] = work->lines[k + 1];
    work->count--;
    if (!classify(work)) { light_track_free(work); free(work); return fail(why, cap, "out of memory"); }
    if (bad_count(work) > bad_count(f)) {
        const char *reason = NULL;
        int line = 0;
        for (int k = 0; reason == NULL && k < work->count; k++)
            if (work->lines[k].kind == LT_BAD && (k + (k >= pos) >= f->count || f->lines[k + (k >= pos)].kind != LT_BAD)) reason = work->lines[k].why, line = k + 1;
        char text[LT_WHY];
        snprintf(text, sizeof text, "line %d would stop being well-formed: %s", line, reason != NULL ? reason : "the game would refuse it");
        light_track_free(work);
        free(work);
        return fail(why, cap, text);
    }
    light_track_free(f);
    *f = *work;
    free(work);
    return 1;
}

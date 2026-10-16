#include "game/race/track_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LINE_MAX = 128, TOKEN_MAX = 128, RING_SCALE_NUM = 200, RING_SCALE_DEN = 0x1e0, TINT_GROUPS = 3 };

/* 004bb094: the built-in rows, cl1 .. cy3 then tr1 .. tr7 (33 words each) */
static const int32_t ROWS[TRACK_INFO_ROWS][TRACK_INFO_ROW_WORDS] = {
    { -30, 0, 0, 0, 0, 64, 0, 64, 9, 9, 20, 3, 12, 11, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* cl1 */
    { -15, 0, 0, 0, 30, 0, 64, 64, 9, 9, 20, 3, 10, 13, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* cl2 */
    { -15, 0, 0, 0, 30, 0, 64, 64, 9, 9, 20, 3, 3, 16, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* cl3 */
    { -4, 0, 7, 2, 6, 17, 30, 64, 4, 8, 18, 5, 5, 19, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* al1 */
    { -1, 6, 7, 2, 6, 17, 30, 64, 4, 8, 18, 5, 5, 19, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* al2 */
    { -3, 0, 7, 2, 6, 17, 30, 64, 4, 8, 18, 28, 28, 28, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* al3 */
    { -1, 3, 7, 2, 8, 20, 30, 64, 16, 25, 30, 5, 8, 3, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* cy1 */
    { -1, 2, 7, 2, 4, 20, 20, 64, 16, 25, 30, 20, 22, 24, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* cy2 */
    { -1, 2, 7, 2, 4, 20, 20, 64, 16, 25, 30, 12, 12, 4, 0, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* cy3 */
    { 0, 0, 0, 0, 0, 48, 0, 96, 0, 1, 2, 3, 4, 5, 1, -60, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* tr1 */
    { 0, 0, 0, 0, 0, 48, 0, 96, 0, 1, 2, 3, 4, 5, 1, -80, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* tr2 */
    { 0, 0, 0, 0, 0, 48, 0, 96, 0, 1, 2, 3, 4, 5, 1, -20, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 1 },  /* tr3 */
    { 0, 0, 0, 0, 0, 48, 0, 96, 0, 1, 2, 3, 4, 5, 1, -100, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* tr4 */
    { 0, 0, 0, 0, 0, 48, 0, 96, 0, 1, 2, 3, 4, 5, 1, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 1 },  /* tr5 */
    { 0, 0, 0, 0, 0, 48, 0, 96, 0, 1, 2, 3, 4, 5, 1, -20, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 0 },  /* tr6 */
    { 0, 0, 0, 0, 0, 48, 0, 96, 0, 1, 2, 3, 4, 5, 1, -50, 112, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 255, 255, 255, 0, 255, 1 },  /* tr7 */
};
static const char *const EXACT[9][2] = {
    { "cl1", "Cl1" }, { "cl2", "Cl2" }, { "cl3", "Cl3" }, { "al1", "Al1" }, { "al2", "Al2" }, { "al3", "Al3" },
    { "cy1", "Cy1" }, { "cy2", "Cy2" }, { "cy3", "Cy3" }
};
static const char *const ANY_CASE[7] = { "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7" };

/* 00481f93: the C library's atoi (32-bit, wrapping), its spaces 9..13 and 32. */
static int crt_space(char c) { return c == ' ' || (c >= 9 && c <= 13); }

static int32_t crt_atoi(const char *s)
{
    while (crt_space(*s)) s++;
    const char sign = *s;
    if (sign == '+' || sign == '-') s++;
    uint32_t v = 0;
    for (; *s >= '0' && *s <= '9'; s++) v = v * 10u + (uint32_t)(*s - '0');
    return (int32_t)(sign == '-' ? 0u - v : v);
}

/* 00488723: equal ignoring the case of A..Z. */
static int same_any_case(const char *a, const char *b)
{
    for (;; a++, b++) {
        const char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 0x20) : *a, y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 0x20) : *b;
        if (x != y) return 0;
        if (x == '\0') return 1;
    }
}

static int line_space(char c) { return c == ' ' || c == '\t' || c == '\n'; }

/* 00429050: past the spaces, the token and the spaces after it. */
static const char *next_token(const char *p)
{
    while (line_space(*p)) p++;
    while (*p != '\0' && !line_space(*p)) p++;
    while (line_space(*p)) p++;
    return p;
}

/* 00429098: the token after the spaces copied (port guard: at most TOKEN_MAX - 1; the original's buffer holds 64). */
static void copy_token(char token[TOKEN_MAX], const char *p)
{
    while (line_space(*p)) p++;
    int n = 0;
    while (*p != '\0' && !line_space(*p) && n < TOKEN_MAX - 1) token[n++] = *p++;
    token[n] = '\0';
}

static const uint32_t POWERS_OF_TEN[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };  /* 004bae98 */

int32_t track_info_decimal(char *text)
{
    char *s = text;
    while (*s == ' ') s++;
    int32_t sign = 1;
    if (*s == '-') sign = -1, s++;
    const int leading_dot = *s == '.';
    char *dot = strchr(s, '.');
    if (dot == NULL) return (int32_t)((uint32_t)crt_atoi(s) * (uint32_t)sign * 0x10000u);
    *dot = '\0';
    char *frac = dot + 1;
    const int32_t whole = leading_dot ? 0 : (int32_t)((uint32_t)crt_atoi(s) << 16);
    int digits = 1;  /* the one after the dot counted whatever it is */
    for (const char *d = dot + 2; *d >= '0' && *d <= '9'; d++) digits++;
    int zeros = 0;
    while (*frac == '0') frac++, zeros++;
    if (zeros + 4 < digits) {
        frac[zeros + 4] = '\0';  /* from past the zeros, as the original indexes */
        digits = zeros + 4;
    }
    if (digits > 9) digits = 9;  /* port guard: past 004bae98's ten powers the original divides by 0 or by other data */
    const uint32_t part = ((uint32_t)crt_atoi(frac) << 16) / POWERS_OF_TEN[digits];
    return (int32_t)((part + (uint32_t)whole) * (uint32_t)sign);
}

/* 00429020: lines until one does not start with '#', '\n' or '\r'; at the end the line stands. */
static void next_value(const TrackInfoOps *ops, char line[LINE_MAX], int32_t file)
{
    while (ops->gets(ops->ctx, line, TRACK_INFO_LINE, file) && (line[0] == '#' || line[0] == '\n' || line[0] == '\r')) {
    }
}

/* "r g b" as the file gives it: three tokens, atoi, r << 10 | g << 5 | b. */
static int32_t colour(const char *line)
{
    char token[TOKEN_MAX];
    const char *p = line;
    while (line_space(*p)) p++;
    copy_token(token, p);
    int32_t c = (int32_t)((uint32_t)crt_atoi(token) << 10);
    p = next_token(p);
    copy_token(token, p);
    c |= (int32_t)((uint32_t)crt_atoi(token) << 5);
    p = next_token(p);
    copy_token(token, p);
    return c | crt_atoi(token);
}

static void from_file(const TrackInfoOps *ops, int32_t file, TrackInfo *info)
{
    char line[LINE_MAX] = { 0 };  /* port guard: the original's starts as its stack left it */
    int32_t *const fixed[8] = { &info->vert_offset_near, &info->vert_offset_far, &info->z_offset_near,
                                &info->z_offset_far, &info->height_near, &info->height_far,
                                &info->radius_near, &info->radius_far };
    for (int i = 0; i < 8; i++) {
        next_value(ops, line, file);
        *fixed[i] = track_info_decimal(line);
    }
    next_value(ops, line, file);
    info->colour_top = colour(line);
    next_value(ops, line, file);
    info->colour_bottom = colour(line);
    int32_t *const ints[3] = { &info->ring, &info->ring_y_offset, &info->ring_height };
    for (int i = 0; i < 3; i++) {
        next_value(ops, line, file);
        *ints[i] = crt_atoi(line);
    }
    for (int g = 0; g < TINT_GROUPS; g++)
        for (int k = 0; k < 5; k++) {
            next_value(ops, line, file);
            info->time_of_day[k][g] = (int32_t)((uint32_t)crt_atoi(line) << (k == 4 ? 8 : 0));
        }
    next_value(ops, line, file);
    info->final_flag = crt_atoi(line);
    if (info->final_flag != 0) info->ring = 1;
    ops->close(ops->ctx, file);
}

static const int32_t *row_for(const char *stem)
{
    const int32_t *row = NULL;
    for (int i = 0; i < 9; i++)
        if (strcmp(stem, EXACT[i][0]) == 0 || strcmp(stem, EXACT[i][1]) == 0) row = ROWS[i];
    for (int i = 0; i < 7; i++)
        if (same_any_case(stem, ANY_CASE[i])) row = ROWS[9 + i];
    return row;
}

static void from_row(const int32_t *r, TrackInfo *info)
{
    int32_t *const fixed[8] = { &info->vert_offset_near, &info->vert_offset_far, &info->z_offset_near,
                                &info->z_offset_far, &info->height_near, &info->height_far,
                                &info->radius_near, &info->radius_far };
    for (int i = 0; i < 8; i++) *fixed[i] = (int32_t)((uint32_t)r[i] << 16);
    info->colour_top = (int32_t)((uint32_t)r[8] << 10 | (uint32_t)r[9] << 5 | (uint32_t)r[10]);
    info->colour_bottom = (int32_t)((uint32_t)r[11] << 10 | (uint32_t)r[12] << 5 | (uint32_t)r[13]);
    info->ring = r[14], info->ring_y_offset = r[15], info->ring_height = r[16];
    for (int g = 0; g < TINT_GROUPS; g++)
        for (int k = 0; k < 5; k++)
            info->time_of_day[k][g] = (int32_t)((uint32_t)r[17 + 5 * g + k] << (k == 4 ? 8 : 0));
    info->final_flag = r[32];
}

static int32_t scaled(int32_t v)
{
    return (int32_t)((uint32_t)v * RING_SCALE_NUM) / RING_SCALE_DEN;
}

void track_info_load(const TrackInfoOps *ops, const TrackInfoInputs *in, TrackInfo *info)
{
    char name[2 * LINE_MAX];
    snprintf(name, sizeof name, "%s%sInfo", in->misc_dir, in->stem);
    const int32_t file = ops->open(ops->ctx, name);
    if (in->use_file != 0 && file != 0 && in->time_of_day != 1) {
        from_file(ops, file, info);
    } else {
        ops->close(ops->ctx, file);
        const int32_t *row = row_for(in->stem);
        if (row != NULL) from_row(row, info);  /* port guard: the original reads through NULL */
    }
    if (in->screen_mode == 0) {
        info->ring_height = scaled(info->ring_height);
        info->ring_y_offset = scaled(info->ring_y_offset);
    }
    info->sky_flag = info->final_flag;
    if (in->stem[0] != 't' && in->time_of_day == 1) info->sky_flag = 0;
    const int32_t g = in->time_of_day;
    if (g >= 0 && g < TINT_GROUPS)  /* port guard: the original reads the words beside the groups */
        ops->tint(ops->ctx, info->time_of_day[0][g], info->time_of_day[1][g], info->time_of_day[2][g],
                  info->time_of_day[3][g], info->time_of_day[4][g]);
    info->unknown_4fccf0 = 0;
}

typedef struct {
    const char *p, *end;
} TextFile;

static int32_t text_open(void *ctx, const char *name) { (void)name; return ((TextFile *)ctx)->p < ((TextFile *)ctx)->end; }
static void text_close(void *ctx, int32_t file) { (void)ctx, (void)file; }
static void text_tint(void *ctx, int32_t r, int32_t g, int32_t b, int32_t s, int32_t v) { (void)ctx, (void)r, (void)g, (void)b, (void)s, (void)v; }

static int text_gets(void *ctx, char *line, int32_t size, int32_t file)
{
    (void)file;
    TextFile *f = ctx;
    if (f->p >= f->end) return 0;
    int32_t n = 0;
    while (f->p < f->end && n + 1 < size) {
        const char c = *f->p++;
        line[n++] = c;
        if (c == '\n') break;
    }
    line[n] = '\0';
    return 1;
}

int track_info_parse(TrackInfo *info, const char *text, size_t size)
{
    TextFile f = { text, text + size };
    if (size == 0) return 0;
    const TrackInfoOps ops = { &f, text_open, text_gets, text_close, text_tint };
    const TrackInfoInputs in = { "?", "", 1, 0, 1 };
    track_info_load(&ops, &in, info);
    return 1;
}

void track_stem_set(char stem[TRACK_STEM], const char *name)
{
    for (int k = 0; k < TRACK_STEM; k++)  /* port guard */
        if ((stem[k] = name[k]) == 0) break;
}

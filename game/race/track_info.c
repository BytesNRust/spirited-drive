#include "game/race/track_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LINE_MAX = 128 };

typedef struct {
    const char *p, *end;
} Reader;

/* 004822ba-like fgets; 0 at the end of the text. */
static int read_line(Reader *r, char line[LINE_MAX])
{
    if (r->p >= r->end) return 0;
    size_t n = 0;
    while (r->p < r->end && n + 1 < LINE_MAX) {
        const char c = *r->p++;
        line[n++] = c;
        if (c == '\n') break;
    }
    line[n] = '\0';
    return 1;
}

/* 00429020: the next line not starting with '#', '\n' or '\r'. */
static int next_value(Reader *r, char line[LINE_MAX])
{
    while (read_line(r, line))
        if (line[0] != '#' && line[0] != '\n' && line[0] != '\r') return 1;
    return 0;
}

static const uint32_t POWERS_OF_TEN[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };  /* 004bae98 */

int32_t track_info_decimal(const char *text)
{
    char buf[LINE_MAX];
    snprintf(buf, sizeof buf, "%s", text);
    char *s = buf;
    while (*s == ' ') s++;
    int32_t sign = 1;
    if (*s == '-') sign = -1, s++;
    char *dot = strchr(s, '.');
    if (dot == NULL) return (int32_t)((uint32_t)(atoi(s) * sign) << 16);
    *dot = '\0';
    const int32_t whole = dot == s ? 0 : (int32_t)((uint32_t)atoi(s) << 16);
    char *frac = dot + 1;
    int digits = 0;
    while (frac[digits] >= '0' && frac[digits] <= '9') digits++;
    int zeros = 0;
    while (frac[zeros] == '0') zeros++;
    if (zeros + 4 < digits) {
        frac[zeros + 4] = '\0';
        digits = zeros + 4;
    }
    if (digits > 9) digits = 9;
    const uint32_t part = (uint32_t)(atoi(frac + zeros) << 16) / POWERS_OF_TEN[digits];
    return (int32_t)(part + (uint32_t)whole) * sign;
}

/* "r g b", 5 bits each. */
static uint16_t colour(const char *line)
{
    char *end;
    const long r = strtol(line, &end, 10), g = strtol(end, &end, 10), b = strtol(end, &end, 10);
    return (uint16_t)((r << 10) | (g << 5) | b);
}

int track_info_parse(TrackInfo *info, const char *text, size_t size)
{
    Reader r = { text, text + size };
    char line[LINE_MAX];
    int32_t *const fixed[8] = { &info->vert_offset_near, &info->vert_offset_far, &info->z_offset_near,
                                &info->z_offset_far, &info->height_near, &info->height_far,
                                &info->radius_near, &info->radius_far };
    memset(info, 0, sizeof *info);
    for (int i = 0; i < 8; i++) {
        if (!next_value(&r, line)) return 0;
        *fixed[i] = track_info_decimal(line);
    }
    if (!next_value(&r, line)) return 0;
    info->colour_top = colour(line);
    if (!next_value(&r, line)) return 0;
    info->colour_bottom = colour(line);
    int32_t *const ints[3] = { &info->ring, &info->ring_y_offset, &info->ring_height };
    for (int i = 0; i < 3; i++) {
        if (!next_value(&r, line)) return 0;
        *ints[i] = atoi(line);
    }
    /* The file lists the values group by group: 004fcd28, 34, 40, 4c, 58, then 2c.. */
    for (int g = 0; g < 3; g++) {
        for (int k = 0; k < 5; k++) {
            if (!next_value(&r, line)) return 0;
            info->time_of_day[g][k] = atoi(line) * (k == 4 ? 256 : 1);
        }
    }
    if (!next_value(&r, line)) return 0;
    info->final_flag = atoi(line);
    if (info->final_flag != 0) info->ring = 1;
    return 1;
}

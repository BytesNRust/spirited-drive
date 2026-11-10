/* The default records of config.dat (00429278), from the exe: the string tables 00429278 fills the records from
 * (004b9638..004bad00, six pointers a row: a name and five numbers, as text, that 00481f93 reads) and the three names it
 * writes at 0052ac68 / 0052ac9a / 0052acd6 (004aec24, 004aec2c, 004aec30). */
#include "game/exe/exe_tables.h"

#include <stdio.h>
#include <string.h>

enum { NUMBER_TEXT = 64 };

static const uint32_t NAME_ADDR[3] = { 0x4AEC24, 0x4AEC2C, 0x4AEC30 };

/* 00481f93: blanks skipped, a sign, then digits (the C library's ctype table, 004b6a78); the value wraps at 32 bits */
static int32_t exe_atoi(const char *text)
{
    size_t i = 0;
    while (text[i] == ' ' || (text[i] >= '\t' && text[i] <= '\r')) i++;
    char sign = text[i];
    if (sign == '+' || sign == '-') i++;
    uint32_t v = 0;
    while (text[i] >= '0' && text[i] <= '9') v = v * 10u + (uint32_t)(text[i++] - '0');
    if (sign == '-') v = 0u - v;
    return (int32_t)v;
}

static int fail(char *err, size_t errsz, const char *what, int row, uint32_t va)
{
    if (err && errsz) snprintf(err, errsz, "config records: %s (row %d, address %08x)", what, row, (unsigned)va);
    return -1;
}

/* a name of the exe into `out` (EXE_NAME_MAX); -1 when it is in no section or does not fit */
static int read_name(const ExeImage *img, uint32_t va, char out[EXE_NAME_MAX], const char **why)
{
    char text[NUMBER_TEXT];
    if (exe_va_cstr(img, va, text, sizeof text) != 0) { *why = "a string is in no section"; return -1; }
    if (strlen(text) >= EXE_NAME_MAX) { *why = "a name is too long"; return -1; }
    memcpy(out, text, strlen(text) + 1);
    return 0;
}

int exe_tables_config(const ExeImage *img, ExeTables *t, char *err, size_t errsz)
{
    const char *why = "";
    for (int r = 0; r < CONFIG_RECORD_ROW_COUNT; r++) {
        uint32_t at = CONFIG_RECORD_FIRST + (uint32_t)r * CONFIG_RECORD_ROW_SIZE;
        uint8_t raw[CONFIG_RECORD_ROW_SIZE];
        if (exe_va_read(img, at, raw, sizeof raw) != 0) return fail(err, errsz, "the table is in no section", r, at);
        uint32_t ptr[6];
        for (int i = 0; i < 6; i++)
            ptr[i] = (uint32_t)raw[4 * i] | (uint32_t)raw[4 * i + 1] << 8 | (uint32_t)raw[4 * i + 2] << 16 | (uint32_t)raw[4 * i + 3] << 24;
        if (read_name(img, ptr[0], t->config_row_name[r], &why) != 0) return fail(err, errsz, why, r, ptr[0]);
        for (int n = 0; n < 5; n++) {
            char text[NUMBER_TEXT];
            if (exe_va_cstr(img, ptr[n + 1], text, sizeof text) != 0)
                return fail(err, errsz, "a number's text is in no section", r, ptr[n + 1]);
            t->config_row_number[r][n] = exe_atoi(text);
        }
    }
    for (int i = 0; i < 3; i++)
        if (read_name(img, NAME_ADDR[i], t->config_name[i], &why) != 0) return fail(err, errsz, why, -1, NAME_ADDR[i]);
    return 0;
}

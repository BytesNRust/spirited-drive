#include "formats/exchange.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fileset_init(FileSet *set)
{
    memset(set, 0, sizeof *set);
}

void fileset_free(FileSet *set)
{
    for (size_t i = 0; i < set->count; ++i) {
        free(set->file[i].name);
        free(set->file[i].data);
    }
    free(set->file);
    memset(set, 0, sizeof *set);
}

int fileset_name_ok(const char *name)
{
    if (name == NULL || name[0] == '\0' || name[0] == '/') return 0;
    for (const char *p = name; *p; ++p) {
        if (*p == '\\') return 0;
        const int start = p == name || p[-1] == '/';
        if (start && p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) return 0;
        if (start && p[0] == '/') return 0;   /* empty component */
    }
    return name[strlen(name) - 1] != '/';
}

const SetFile *fileset_find(const FileSet *set, const char *name)
{
    for (size_t i = 0; i < set->count; ++i)
        if (strcmp(set->file[i].name, name) == 0) return &set->file[i];
    return NULL;
}

int fileset_add_owned(FileSet *set, const char *name, uint8_t *data, size_t size)
{
    if (!fileset_name_ok(name) || fileset_find(set, name) != NULL) return 0;
    if (set->count == set->cap) {
        const size_t cap = set->cap ? set->cap * 2 : 16;
        SetFile *grown = realloc(set->file, cap * sizeof *grown);
        if (grown == NULL) return 0;
        set->file = grown;
        set->cap = cap;
    }
    char *copy = malloc(strlen(name) + 1);
    if (copy == NULL) return 0;
    strcpy(copy, name);
    set->file[set->count].name = copy;
    set->file[set->count].data = data;
    set->file[set->count].size = size;
    set->count++;
    return 1;
}

int fileset_add(FileSet *set, const char *name, const uint8_t *data, size_t size)
{
    uint8_t *copy = malloc(size + 1);
    if (copy == NULL) return 0;
    if (size) memcpy(copy, data, size);
    copy[size] = '\0';   /* a text file can be read as a C string */
    if (!fileset_add_owned(set, name, copy, size)) { free(copy); return 0; }
    return 1;
}

void exchange_fail(ExchangeError *err, const char *format, ...)
{
    if (err == NULL) return;
    va_list args;
    va_start(args, format);
    vsnprintf(err->message, sizeof err->message, format, args);
    va_end(args);
}

int fileset_move_prefixed(FileSet *to, FileSet *from, const char *prefix)
{
    int ok = 1;
    for (size_t i = 0; i < from->count; ++i) {
        const size_t n = strlen(prefix) + strlen(from->file[i].name) + 1;
        char *name = malloc(n);
        if (name == NULL) { ok = 0; continue; }
        strcpy(name, prefix);
        strcat(name, from->file[i].name);
        if (fileset_add_owned(to, name, from->file[i].data, from->file[i].size)) from->file[i].data = NULL;
        else ok = 0;
        free(name);
    }
    fileset_free(from);
    return ok;
}

int fileset_copy_stripped(FileSet *to, const FileSet *from, const char *prefix)
{
    const size_t p = strlen(prefix);
    for (size_t i = 0; i < from->count; ++i) {
        if (strncmp(from->file[i].name, prefix, p) != 0 || from->file[i].name[p] == '\0') continue;
        if (!fileset_add(to, from->file[i].name + p, from->file[i].data, from->file[i].size)) return 0;
    }
    return 1;
}

/* Shared pieces of the exchange modules (shpi_exchange, ...): a set of named files held in memory, so
 * export and import are testable without a disk, and the error record the CLI prints. A file set holds
 * relative names with '/' separators; names that are absolute, hold ".." or a backslash are refused. */
#ifndef TNFS_EXCHANGE_H
#define TNFS_EXCHANGE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *name;
    uint8_t *data;
    size_t size;
} SetFile;

typedef struct {
    SetFile *file;
    size_t count, cap;
} FileSet;

void fileset_init(FileSet *set);
void fileset_free(FileSet *set);
/* Copies name and data; the copy is followed by a NUL byte, so text files can be used as C strings.
 * 0 when the name is unsafe or taken, or on allocation failure. */
int fileset_add(FileSet *set, const char *name, const uint8_t *data, size_t size);
/* Takes ownership of `data` (malloc'd) on success. */
int fileset_add_owned(FileSet *set, const char *name, uint8_t *data, size_t size);
const SetFile *fileset_find(const FileSet *set, const char *name);
int fileset_name_ok(const char *name);

/* Moves every file of `from` into `to` under `prefix` ("dir/" or ""), emptying `from`. 0 on a name clash or
 * allocation failure (what moved stays moved). */
int fileset_move_prefixed(FileSet *to, FileSet *from, const char *prefix);
/* Copies the files of `from` whose names start with `prefix` into `to`, prefix stripped. */
int fileset_copy_stripped(FileSet *to, const FileSet *from, const char *prefix);

/* What an exchange function reports on failure: a message that names the file or resource and,
 * where it applies, the offset or line. */
typedef struct {
    char message[200];
} ExchangeError;

void exchange_fail(ExchangeError *err, const char *format, ...);

/* Pixels an import had to move to a colour of the resource's palette. */
typedef struct {
    uint32_t pixels;
    uint32_t reassigned;
} ImportStats;

#endif

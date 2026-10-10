/* validate: checks a game file or a mod against the limits of the original executable (limits.h). Every
 * finding names the limit, the value found, the maximum and the element, so the message says what to fix. */
#ifndef TNFS_VALIDATE_H
#define TNFS_VALIDATE_H

#include "formats/exchange.h"
#include "formats/limits.h"

#include <stdio.h>

typedef struct {
    const Limit *limit;
    int64_t value;        /* what was found (0 when the rule is yes/no) */
    char item[200];       /* file, element and offset */
} Finding;

typedef struct {
    Finding *item;
    size_t count, cap;
} Report;

typedef enum { VALIDATE_COMPATIBLE, VALIDATE_PORT_ONLY } ValidateMode;

void report_init(Report *r);
void report_free(Report *r);
/* Records a finding. `format` describes the element. */
void report_add(Report *r, const char *limit_id, int64_t value, const char *format, ...) __attribute__((format(printf, 4, 5)));
/* Findings that make compatible mode fail. */
size_t report_fatal_count(const Report *r);
/* One line per finding: "error|warning ID: item: value V, maximum M (EXE address)". In port-only mode every
 * finding is a warning. */
void report_print(const Report *r, ValidateMode mode, FILE *out);

/* Validates one file by its name and content (QFS unpacked first). Findings are appended. */
void validate_file(const uint8_t *file, size_t size, const char *name, Report *r);

/* A mod or a directory of game files: every file by itself, plus the rules that span files (sound numbers of
 * banks loaded together, the INFO file of a track the EXE has no built-in row for). Names are relative paths
 * with '/' separators; the FileSet holds the file contents. */
void validate_tree(const FileSet *files, Report *r);

#endif

/* The limits of the original executable that validate enforces, one record each, with the NFS_WIN.EXE
 * function or address the limit was read from (docs/modding/inventory.md §7 has the reasoning). `fatal` is 1
 * when the original misbehaves outside the limit (hangs, aborts, writes past a buffer, zeroes a value) and
 * the compatible mode must refuse the file; 0 when it only ignores or tolerates it (a warning in both
 * modes). `inferred` is 1 when the limit follows from the memory layout rather than from a compare. */
#ifndef TNFS_LIMITS_H
#define TNFS_LIMITS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *id;       /* "TRI.SEGMENTS": area.name */
    const char *what;     /* what is limited */
    int64_t max;          /* the largest accepted value; for yes/no rules 0 */
    const char *exe;      /* function or address in NFS_WIN.EXE */
    int fatal;
    int inferred;
} Limit;

extern const Limit limit_table[];
extern const size_t limit_count;

/* NULL when the id is not in the table. */
const Limit *limit_find(const char *id);

#endif

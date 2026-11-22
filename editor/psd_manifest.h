/* The manifest of a car kit's generated masks (art-generated/.generated.tsv): a line a PNG Studio made from a PSD, `<name><TAB><sha256 of its bytes>`, sorted by
 * name. It is how a PNG somebody edited by hand is told from one Studio made: its hash is no longer the one written down. A PNG the manifest does not know (a
 * kit from before it, a PNG put there by hand) is "unknown", never "edited": nothing is claimed about it. The build ignores the file. POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_PSD_MANIFEST_H
#define TNFS_TRACK_EDITOR_PSD_MANIFEST_H

#include <stddef.h>

enum { MANIFEST_NAME = 64, MANIFEST_MAX = 64, MANIFEST_SHA = 65 };

typedef struct {
    int count;
    struct { char name[MANIFEST_NAME]; char sha[MANIFEST_SHA]; } entry[MANIFEST_MAX];
} Manifest;

typedef enum { GEN_OK = 0, GEN_EDITED, GEN_UNKNOWN, GEN_MISSING } GenState;

/* The manifest in `path`; a file that is not there is an empty manifest (1). A line that is not `name<TAB>64 hex digits` is skipped. 0 when it cannot be read. */
int manifest_load(const char *path, Manifest *m);
/* The hash written for `name` (case as written), NULL when there is none. */
const char *manifest_sha(const Manifest *m, const char *name);
/* Writes (or replaces) the hash of `name`; 0 when the name does not fit or the manifest is full. */
int manifest_set(Manifest *m, const char *name, const char *sha);
/* Drops the line of `name`; 1 when there was one. */
int manifest_forget(Manifest *m, const char *name);
/* The file written whole beside itself and renamed over `path` (a failure leaves the old one); 0 on failure. */
int manifest_save(const char *path, const Manifest *m);
/* The SHA-256 of a file as 64 lower-case hex digits and a NUL; 0 when it cannot be read. */
int manifest_file_sha(const char *file, char sha[MANIFEST_SHA]);
/* Hashes `dir`/`name` and writes it down; 0 when the file cannot be read or does not fit. */
int manifest_record(Manifest *m, const char *dir, const char *name);
/* The manifest in `path`, as manifest_load, but a file whose time and size are those of the last one read is not opened again (a slow, synced disk makes every open dear). */
int manifest_load_cached(const char *path, Manifest *m);
/* How many files this module has opened (manifest_load, manifest_file_sha) since the program began: a test's way to see that nothing was read again. */
unsigned manifest_opens(void);
/* What `dir`/`name` is against the manifest: GEN_MISSING (no such file), GEN_UNKNOWN (no line), GEN_EDITED (another hash) or GEN_OK. The hash of a file is kept by its time and size and made again only when one changes. */
GenState manifest_state(const Manifest *m, const char *dir, const char *name);

#endif

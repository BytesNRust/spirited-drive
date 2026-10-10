/* Editable containers built on offset tables: the track and car "wwww" trees
 * (FAM, CFM; see chunk.h) and the SHPI bitmap directories (FSH; see fsh.h).
 *
 * A document keeps what it cannot name verbatim and rebuilds only the offsets.
 * Each container is a head (everything before its first child, table included),
 * its children in file order, and which child every table entry points at. A
 * child's bytes run to the next larger offset, or to the end of the container,
 * so padding between children belongs to the child before it. Two entries with
 * the same offset share one child, and an entry of 0 stays 0 (absent). Writing
 * puts the head and the children back one after another and patches the table
 * entries (and an SHPI's total-length word when it held the file size), so an
 * untouched document writes back byte for byte.
 *
 * Replace-only: entries cannot be added or removed (Level 1; inventory.md §2).
 * Replacing a child with other bytes moves every later offset; fields inside the
 * new bytes (an ORIP's length, an SHPI entry's block size) are the caller's. */
#ifndef TNFS_DOC_H
#define TNFS_DOC_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    DOC_OK = 0,
    DOC_ERR_BOUNDS,   /* table or offset outside the data */
    DOC_ERR_HEADER,   /* not the expected tag */
    DOC_ERR_INDEX,    /* entry past the table, or not a container */
    DOC_ERR_MEMORY,
    DOC_ERR_CAPACITY, /* destination too small */
    DOC_ERR_DEPTH
} DocResult;

typedef struct DocNode DocNode;

/* Parses a "wwww" tree. Children that are themselves "wwww" with a sound table
 * become containers; anything else (ORIP, SHPI, odd bytes) stays a leaf, so any
 * file whose root table is sound can be rewritten. */
DocResult doc_parse_chunks(const uint8_t *data, size_t size, DocNode **out);

/* Same; on failure *at is the file offset of the table entry that is out of bounds (or of the count). */
DocResult doc_parse_chunks_at(const uint8_t *data, size_t size, DocNode **out, size_t *at);
DocResult doc_parse_shpi_at(const uint8_t *data, size_t size, DocNode **out, size_t *at);

/* Parses an SHPI directory (header + count x (name, offset)) as a container whose
 * children are the entries. */
DocResult doc_parse_shpi(const uint8_t *data, size_t size, DocNode **out);

/* Turns a leaf holding an SHPI into a container, in place. DOC_ERR_HEADER when it
 * does not hold one. */
DocResult doc_expand_shpi(DocNode *leaf);

void doc_free(DocNode *node);

int doc_is_container(const DocNode *node);
uint32_t doc_entry_count(const DocNode *node);

/* The child an entry points at; *child is NULL for an absent entry. Entries that
 * share an offset return the same node. */
DocResult doc_entry(const DocNode *node, uint32_t index, DocNode **child);

/* A leaf's bytes (extent included). */
const uint8_t *doc_bytes(const DocNode *leaf, size_t *size);

/* A container's head, mutable in place for fields outside the table (a "wwww" count
 * is not one of them). */
uint8_t *doc_head(DocNode *node, size_t *size);

/* Replaces a leaf's bytes with a copy. */
DocResult doc_set_bytes(DocNode *leaf, const uint8_t *bytes, size_t size);

size_t doc_size(const DocNode *node);
/* Writes doc_size(node) bytes. */
DocResult doc_write(const DocNode *node, uint8_t *dst, size_t cap, size_t *written);

const char *doc_result_str(DocResult result);

#endif

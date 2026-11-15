/* A car model file (CFM) put together from its four chunks, and taken apart into them (dep59 tools B1). The file is a "wwww" table
 * of four children (formats/chunk.h): the high mesh (ORIP), its bank (SHPI), the low mesh, its bank. The CD's 39 hold them in that
 * order, the table followed at once by the first, each at a multiple of 4; the bytes between a child's declared length and the
 * next child are leftovers, and every file's length is a multiple of 4 (docs/modding/car-model.md). cfm_describe gives each child
 * with those bytes (its extent); cfm_build lays the children out again, padding each with zeros to a multiple of 4, so the extents
 * of a CD file build it again byte for byte. */
#ifndef TNFS_CFM_BUILD_H
#define TNFS_CFM_BUILD_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum { CFM_CHILDREN = 4 };

/* the CFM of `child[i]` (`len[i]` bytes each, in chunk.h's order) (malloc'd); 0 with the reason in `err`: a child that is not an
 * ORIP (0, 2) or a SHPI (1, 3), or whose declared length is under 8 or past its bytes */
int cfm_build(const uint8_t *const child[CFM_CHILDREN], const size_t len[CFM_CHILDREN], uint8_t **out, size_t *out_len, ExchangeError *err);
/* the four children of a CFM, pointing into it, each to the next child or the end of the file; 0 with the reason in `err` */
int cfm_describe(const uint8_t *cfm, size_t size, const uint8_t *child[CFM_CHILDREN], size_t len[CFM_CHILDREN], ExchangeError *err);

#endif

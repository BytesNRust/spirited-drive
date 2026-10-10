/* TRI <-> TSV. An export gives track.tsv (header scalars), nodes.tsv (the road nodes, one extra closing
 * node), brightness.tsv, descriptors.tsv, scenery.tsv (the placements up to the -1 terminator) and
 * segments.tsv (the terrain strips, 4 rows of 11 points each). An import overlays the tables on the
 * original TRI, so bytes no table covers stay as they were. Replace-only on the road: nodes, segments,
 * descriptors and brightness rows must keep their counts; scenery may hold 0..1000 placements (the
 * terminator is rewritten). Counts the writer derives (segments, terrain size, descriptor count) are
 * listed in track.tsv for information and ignored on import. */
#ifndef TNFS_TRI_EXCHANGE_H
#define TNFS_TRI_EXCHANGE_H

#include "formats/exchange.h"

int tri_export(const uint8_t *data, size_t size, FileSet *out, ExchangeError *err);
int tri_import(const uint8_t *original, size_t size, const FileSet *in, uint8_t **out, size_t *out_len,
               ExchangeError *err);

#endif

/* Export original track (dep103, later): the centre line of a CD track's TRI as the editor's own trace (editor/project.h's Trace): control points on the road's nodes, the
 * road's width and bank as road keys. What the trace cannot hold (scenery, terrain, tunnels, the 3D models) the editor builds again from the art, so the track that comes
 * back from Build follows the original's road within the error original_track_trace_error measures. Pure C, no files, no window. */
#ifndef TNFS_EDITOR_ORIGINAL_TRACK_TRACE_H
#define TNFS_EDITOR_ORIGINAL_TRACK_TRACE_H

#include "project.h"

#include <stddef.h>
#include <stdint.h>

/* The trace of the TRI `tri`: a control point on every `stride`-th node (1..8; the closing node is the last), heights from the first node, the road keys for the verge width and the
 * bank. `out` is filled (the caller frees out->points and out->keys), out->width is the byte at the start. 1, or 0 with `why`. */
int original_track_trace(const uint8_t *tri, size_t size, int stride, Trace *out, char *why, size_t why_cap);

/* How far the nodes the trace builds are from the TRI's: the largest and the mean distance on the plan in metres, and the largest height difference, node by node.
 * 1, or 0 with `why` (the trace does not build). */
int original_track_trace_error(const uint8_t *tri, size_t size, const Trace *trace, double *max_m, double *mean_m, double *max_dy, char *why, size_t why_cap);

/* The CD track `stem`'s road as a new track project <tracks_dir>/<id> (project.ini named `name`, art `generic_art` (Studio's GENERIC.FAM, or NULL), trace.json from the TRI at
 * <cd>/SIMDATA/MISC/<stem>.TRI at every 4th node). Made beside as "<id>.making" and renamed when whole; the folder must not be there. `dir` gets the folder. 1, or 0 with `why` in
 * words (a track of more than 599 segments is refused: the generator builds no more), nothing left behind. */
int original_track_export(const char *cd, const char *stem, const char *tracks_dir, const char *id, const char *name, const char *generic_art, char *dir, size_t dir_cap, char *why, size_t why_cap);

/* The node count of the game's track `stem` (the TRI's, the closing node's index): 1 and *nodes, or 0 with `why` (no such track in `cd`, a file that does not open). */
int original_track_nodes(const char *cd, const char *stem, unsigned *nodes, char *why, size_t why_cap);

/* 1 when the CD track `stem` can be exported: its file reads and holds at most 599 segments (the editor's own limit); else 0 with `why` */
int original_track_exportable(const char *cd, const char *stem, char *why, size_t why_cap);

#endif

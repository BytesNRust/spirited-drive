/* Create PSD: the layered PSD of one texture of a track, to paint its mask in, written here in C (no python, no Pillow, no tools folder). The same file tools/track_mask_psd.py makes without its
 * `draft` guess: 8-bit RGB at the texture's size, two layers, the texture (locked) and over it an EMPTY `mask` layer (at 50 % opacity; nothing painted yet). */
#ifndef TNFS_TRACK_EDITOR_TRACK_MASK_MAKE_PSD_H
#define TNFS_TRACK_EDITOR_TRACK_MASK_MAKE_PSD_H

#include <stddef.h>
#include <stdint.h>

/* The PSD's bytes for the texture `rgba` (w x h, alpha ignored). Free with free(). NULL when w or h is not 1..8192 or memory is out. */
uint8_t *track_mask_psd_bytes(const uint8_t *rgba, int w, int h, size_t *len);

/* Writes <root>/art-source/masks/<name>.psd (the folders made when missing) and never over a file that is there. 1 when written; 0 with `why` in words and nothing left behind. */
int track_mask_make_psd(const char *root, const char *name, const uint8_t *rgba, int w, int h, char *why, size_t why_cap);

#endif

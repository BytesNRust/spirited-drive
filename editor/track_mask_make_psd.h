/* Create PSD (light tuning design, piece 4): the layered PSD of one texture of a track, to paint its mask in, made by tools/track_mask_psd.py (Pillow) from the texture's pixels.
 * The texture goes to a scratch folder as <STEM>/<name>.rgb.png, the script writes <name>.psd there, and the PSD is put in <root>/art-source/masks/ (never over one that is there). */
#ifndef TNFS_TRACK_EDITOR_TRACK_MASK_MAKE_PSD_H
#define TNFS_TRACK_EDITOR_TRACK_MASK_MAKE_PSD_H

#include <stddef.h>
#include <stdint.h>

/* `python`: the interpreter ("" for python3 of the PATH); `tools`: the repository's tools folder; `rgba` the texture w x h. 1 when <root>/art-source/masks/<name>.psd is made;
 * 0 with `why` in words (no tools folder, a PSD already there, no Pillow, the script's own last line) and nothing left behind. */
int track_mask_make_psd(const char *python, const char *tools, const char *root, const char *stem, const char *name, const uint8_t *rgba, int w, int h, char *why, size_t why_cap);

#endif
